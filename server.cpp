#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "httplib.h"
#include "json.hpp"

#include "src/domain/Domain.h"
#include "src/rates/ExchangeRateBook.h"
#include "src/fees/FeeStrategy.h"
#include "src/observers/TransactionObserver.h"
#include "src/service/ForexExchangeService.h"

using namespace std;
using json = nlohmann::json;

// Global state for the server
unordered_map<string, Customer*> customers;

// Strategy references (matching UML Page 5)
PercentageBasedFeeStrategy percentageFee(0.015);
FixedCostFeeStrategy       fixedCostFee(5.0, 1.0);
SlabBasedFeeStrategy       slabFee;

// Observer instances (matching UML Page 3)
PrinterObserver  printerObs;
FileObserver     fileObs;
SmsObserver      smsObs;
DbObserver       dbObs;

void initMockData() {
    // Mock accounts (Alice, Bob, Charlie) removed.
    // Accounts are created dynamically when real authenticated users log in.
}

void setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

int main(int argc, char* argv[]) {
    cout << unitbuf;
    cerr << unitbuf;

    int port = 8080;
    if (const char* envPort = getenv("PORT")) {
        int p = atoi(envPort);
        if (p > 0) port = p;
    }
    if (argc > 1) {
        int p = atoi(argv[1]);
        if (p > 0) port = p;
    }

    cout << "Starting Forex Server on port: " << port << "..." << endl;

    initMockData();

    ForexExchangeService* service = ForexExchangeService::getInstance();
    ExchangeRateBook* rateBook = ExchangeRateBook::getInstance();

    // Register Observers
    service->addObserver(&printerObs);
    service->addObserver(&fileObs);
    service->addObserver(&smsObs);
    service->addObserver(&dbObs);
    service->setFeeStrategy(&percentageFee);

    httplib::Server svr;

    // Handle preflight CORS for all routes
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        res.status = 204;
    });

    // 1. Health check
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        json j = {
            {"status", "online"},
            {"engine", "C++ ForexExchange Engine"},
            {"port", 8080}
        };
        res.set_content(j.dump(), "application/json");
    });

    // 2. GET all exchange rates graph
    svr.Get("/api/rates", [rateBook](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        json j;
        j["rates"] = rateBook->getAllRates();
        res.set_content(j.dump(), "application/json");
    });

    // 3. POST new rate or update existing
    svr.Post("/api/rates", [rateBook](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string from = body.at("from");
            string to = body.at("to");
            double rate = body.at("rate");

            if (rate <= 0) {
                res.status = 400;
                res.set_content("{\"error\":\"Rate must be positive\"}", "application/json");
                return;
            }

            rateBook->setRate(from, to, rate);
            json j = {{"success", true}, {"from", from}, {"to", to}, {"rate", rate}};
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 4. GET best exchange rate route (BFS Maximum Product Path algorithm)
    svr.Get("/api/best-rate", [rateBook](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        string from = req.get_param_value("from");
        string to = req.get_param_value("to");

        if (from.empty() || to.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"'from' and 'to' query parameters required\"}", "application/json");
            return;
        }

        vector<string> path;
        double rate = rateBook->getBestRate(from, to, path);

        json j = {
            {"from", from},
            {"to", to},
            {"rate", rate},
            {"path", path},
            {"isDirect", path.size() <= 2}
        };
        res.set_content(j.dump(), "application/json");
    });

    // 5. GET all customer accounts & balances
    svr.Get("/api/accounts", [](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        json accountList = json::array();
        for (const auto& [id, cust] : customers) {
            json c = {
                {"id", cust->id},
                {"name", cust->name},
                {"isVip", cust->isVip},
                {"balances", cust->balances}
            };
            accountList.push_back(c);
        }
        res.set_content(accountList.dump(), "application/json");
    });

    // 6. POST deposit to account
    svr.Post("/api/accounts/deposit", [](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string custId = body.at("customerId");
            string curr = body.at("currency");
            double amount = body.at("amount");

            if (!customers.count(custId)) {
                Customer* c = new Customer(custId, custId, false);
                customers[custId] = c;
            }
            if (amount <= 0) {
                res.status = 400;
                res.set_content("{\"error\":\"Amount must be positive\"}", "application/json");
                return;
            }

            Customer* cust = customers[custId];
            cust->deposit(Money(amount, Currency(curr)));

            json j = {
                {"success", true},
                {"customerId", custId},
                {"currency", curr},
                {"newBalance", cust->getBalance(curr)}
            };
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 6b. POST onboard / register authenticated customer into C++ engine
    svr.Post("/api/accounts/onboard", [](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string custId = body.at("id");
            string name = body.value("name", custId);

            if (!customers.count(custId)) {
                Customer* c = new Customer(custId, name, false);
                c->deposit("USD", 10000.0);
                c->deposit("EUR", 5000.0);
                c->deposit("INR", 50000.0);
                c->deposit("GBP", 3000.0);
                customers[custId] = c;
            } else if (!name.empty()) {
                customers[custId]->name = name;
            }

            Customer* cust = customers[custId];
            json j = {
                {"success", true},
                {"id", cust->id},
                {"name", cust->name},
                {"balances", cust->balances}
            };
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 6c. POST update customer profile / name
    svr.Post("/api/accounts/update-profile", [](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string custId = body.at("id");
            string name = body.at("name");

            if (!customers.count(custId)) {
                Customer* c = new Customer(custId, name, false);
                c->deposit("USD", 10000.0);
                c->deposit("EUR", 5000.0);
                customers[custId] = c;
            } else {
                customers[custId]->name = name;
            }

            json j = {{"success", true}, {"id", custId}, {"name", name}};
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 6d. POST restore/sync customer balances from SQL database
    svr.Post("/api/accounts/restore", [](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string custId = body.at("id");
            string name = body.value("name", custId);

            if (!customers.count(custId)) {
                customers[custId] = new Customer(custId, name, false);
            } else if (!name.empty()) {
                customers[custId]->name = name;
            }

            Customer* cust = customers[custId];
            if (body.contains("balances") && body["balances"].is_object()) {
                for (auto& [curr, val] : body["balances"].items()) {
                    cust->balances[curr] = val.get<double>();
                }
            }

            json j = {
                {"success", true},
                {"id", cust->id},
                {"name", cust->name},
                {"balances", cust->balances}
            };
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 7. GET queue status (Regular FIFO + VIP Priority Queue)
    svr.Get("/api/queue", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        auto reg = service->getRegularQueueRequests();
        auto vip = service->getVipQueueRequests();

        json regList = json::array();
        for (const auto& r : reg) {
            regList.push_back({
                {"id", r.id},
                {"customerId", r.customerId},
                {"sourceAmount", r.source.amount},
                {"sourceCurrency", r.source.currency.code},
                {"targetCurrency", r.targetCurrency.code},
                {"isVip", r.isVip},
                {"timestamp", r.timestamp}
            });
        }

        json vipList = json::array();
        for (const auto& r : vip) {
            vipList.push_back({
                {"id", r.id},
                {"customerId", r.customerId},
                {"sourceAmount", r.source.amount},
                {"sourceCurrency", r.source.currency.code},
                {"targetCurrency", r.targetCurrency.code},
                {"isVip", r.isVip},
                {"timestamp", r.timestamp}
            });
        }

        json j = {
            {"regularQueue", regList},
            {"vipQueue", vipList},
            {"totalPending", reg.size() + vip.size()}
        };
        res.set_content(j.dump(), "application/json");
    });

    // 8. POST submit new exchange request
    svr.Post("/api/queue/request", [service](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string custId = body.at("customerId");
            string srcCurr = body.at("sourceCurrency");
            string tgtCurr = body.at("targetCurrency");
            double amount = body.at("amount");

            if (!customers.count(custId)) {
                // Auto-create customer if authenticated with initial trading balance
                string cName = body.value("name", custId);
                Customer* c = new Customer(custId, cName, false);
                c->deposit("USD", 10000.0);
                c->deposit("EUR", 5000.0);
                c->deposit("INR", 50000.0);
                c->deposit("GBP", 3000.0);
                customers[custId] = c;
            }
            if (amount <= 0) {
                res.status = 400;
                res.set_content("{\"error\":\"Amount must be positive\"}", "application/json");
                return;
            }

            Customer* cust = customers[custId];
            // VIP is decided strictly based on the exchange amount (>= 5000)
            bool qualifiesVip = (amount >= 5000.0);
            service->submitRequest(custId, srcCurr, tgtCurr, amount, qualifiesVip);

            json j = {
                {"success", true},
                {"customerId", custId},
                {"sourceCurrency", srcCurr},
                {"targetCurrency", tgtCurr},
                {"amount", amount},
                {"isVip", qualifiesVip}
            };
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // 9. POST clear queues
    svr.Post("/api/queue/clear", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        service->clearQueues();
        json j = {{"success", true}, {"message", "All queues cleared"}};
        res.set_content(j.dump(), "application/json");
    });

    // 10. POST process next pending request from queue
    svr.Post("/api/queue/process-next", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        bool success = service->processNextRequest(customers);
        if (!success) {
            res.status = 400;
            res.set_content("{\"success\":false,\"message\":\"No request processed (Queue empty or balance insufficient)\"}", "application/json");
            return;
        }

        const auto& hist = service->getHistory();
        json lastTx;
        if (!hist.empty()) {
            const auto& tx = hist.back();
            lastTx = {
                {"id", tx.id},
                {"requestId", tx.requestId},
                {"customerId", tx.customerId},
                {"sourceAmount", tx.sourceAmount.amount},
                {"sourceCurrency", tx.sourceAmount.currency.code},
                {"targetAmount", tx.targetAmount.amount},
                {"targetCurrency", tx.targetAmount.currency.code},
                {"fee", tx.fee},
                {"rate", tx.rate},
                {"timestamp", tx.timestamp}
            };
        }

        json j = {
            {"success", true},
            {"message", "Request processed successfully via C++ Engine"},
            {"transaction", lastTx}
        };
        res.set_content(j.dump(), "application/json");
    });

    // 10. POST undo last transaction
    svr.Post("/api/undo", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        bool success = service->undoLastTransaction(customers);
        if (!success) {
            res.status = 400;
            res.set_content("{\"success\":false,\"message\":\"Undo failed or stack empty\"}", "application/json");
            return;
        }
        res.set_content("{\"success\":true,\"message\":\"Last transaction undone successfully via C++ Undo Stack\"}", "application/json");
    });

    // 11. GET transaction history
    svr.Get("/api/transactions", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        const auto& hist = service->getHistory();
        json list = json::array();
        for (const auto& tx : hist) {
            list.push_back({
                {"id", tx.id},
                {"requestId", tx.requestId},
                {"customerId", tx.customerId},
                {"sourceAmount", tx.sourceAmount.amount},
                {"sourceCurrency", tx.sourceAmount.currency.code},
                {"targetAmount", tx.targetAmount.amount},
                {"targetCurrency", tx.targetAmount.currency.code},
                {"fee", tx.fee},
                {"rate", tx.rate},
                {"timestamp", tx.timestamp}
            });
        }
        res.set_content(list.dump(), "application/json");
    });

    // 12. GET & POST fee strategies
    svr.Get("/api/fees/strategies", [service](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        FeeStrategy* current = service->getFeeStrategy();
        json j = {
            {"currentStrategy", current ? current->getName() : "None"},
            {"availableStrategies", {
                {{"id", "percentage"}, {"name", "Percentage Based (1.5% fee, 50% VIP discount)"}},
                {{"id", "fixed"}, {"name", "Fixed Cost ($5.00 Regular / $1.00 VIP)"}},
                {{"id", "slab"}, {"name", "Slab Based (Tiered rates + 25% VIP discount)"}}
            }}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/fees/strategy", [service](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        try {
            auto body = json::parse(req.body);
            string strat = body.at("strategy");
            if (strat == "percentage") {
                service->setFeeStrategy(&percentageFee);
            } else if (strat == "fixed") {
                service->setFeeStrategy(&fixedCostFee);
            } else if (strat == "slab") {
                service->setFeeStrategy(&slabFee);
            } else {
                res.status = 400;
                res.set_content("{\"error\":\"Unknown strategy\"}", "application/json");
                return;
            }
            json j = {
                {"success", true},
                {"activeStrategy", service->getFeeStrategy()->getName()}
            };
            res.set_content(j.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    cout << "=====================================================" << endl;
    cout << "  ForexExchange C++ REST API Server Running!" << endl;
    cout << "  Listening on: http://localhost:" << port << endl;
    cout << "  Core Engine: C++ BFS Graph, Priority Queues, Observers" << endl;
    cout << "=====================================================" << endl;

    bool ok = svr.listen("0.0.0.0", port);
    if (!ok) {
        cerr << "CRITICAL: Server failed to bind/listen on 0.0.0.0:" << port << endl;
        return 1;
    }
    return 0;
}
