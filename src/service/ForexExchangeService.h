#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <unordered_map>
#include "../domain/Domain.h"
#include "../rates/ExchangeRateBook.h"
#include "../fees/FeeStrategy.h"
#include "../observers/TransactionObserver.h"

using namespace std;

// =============================================
// ForexExchangeService: Orchestrator Singleton
// Implements Subject role in Observer Pattern.
// Delegates fee calculation to FeeService (Strategy Pattern).
// Delegates rate lookup to ExchangeRateBook (Singleton).
// (UML Pages 4 & 5)
// =============================================
class ForexExchangeService {
private:
    int nextRequestId = 1;
    int nextTxId = 1;

    // Data Structures (UML Pages 1, 2 & 5)
    queue<ExchangeRequest>          regularQueue;  // FIFO Queue for regular customers
    priority_queue<ExchangeRequest> vipQueue;      // Priority Queue for VIP customers
    vector<Transaction>             history;       // Transaction history (vector)
    stack<Transaction>              undoStack;     // Undo stack

    // Observer list (Observer Pattern - UML Page 3 & 5)
    vector<TransactionObserver*> observers;

    // Singleton service references
    FeeService*          feeService;
    ExchangeRateBook*    exchangeRateBook;

    ForexExchangeService() {
        feeService       = FeeService::getInstance();
        exchangeRateBook = ExchangeRateBook::getInstance();
    }

    string getCurrentTime() const {
        time_t now = time(nullptr);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return string(buf);
    }

    // Notify all registered observers (UML Page 3: notifyObservers)
    void notifyObservers(const Transaction& tx) {
        for (auto* obs : observers) {
            if (obs) obs->update(tx);
        }
    }

public:
    // Meyer's Singleton (UML Page 4 & 5: <<Singleton>>)
    static ForexExchangeService* getInstance() {
        static ForexExchangeService instance;
        return &instance;
    }

    // ---- Observer Management (UML Page 3 & 5) ----

    void addObserver(TransactionObserver* obs) {
        if (obs) observers.push_back(obs);
    }

    void removeObserver(TransactionObserver* obs) {
        observers.erase(
            remove(observers.begin(), observers.end(), obs),
            observers.end()
        );
    }

    // ---- Strategy Management (UML Page 5) ----

    void setFeeStrategy(FeeStrategy* strategy) {
        feeService->setStrategy(strategy);
    }

    FeeStrategy* getFeeStrategy() const {
        return feeService->getStrategy();
    }

    // ---- Core Operations ----

    // Submit a new exchange request into the correct queue (UML Page 1 & 5)
    void submitRequest(const string& customerId,
                       const string& srcCode, const string& tgtCode,
                       double amount, bool isVip) {
        Money source(amount, Currency(srcCode));
        Currency target(tgtCode);
        ExchangeRequest req;
        req.id             = nextRequestId++;
        req.customerId     = customerId;
        req.source         = source;
        req.targetCurrency = target;
        // VIP qualification is determined strictly by exchange amount (>= 5000)
        bool qualifiesVip  = (amount >= 5000.0);
        req.isVip          = qualifiesVip;
        req.timestamp      = time(nullptr);

        if (qualifiesVip) {
            vipQueue.push(req);
            cout << "Submitted VIP Request #" << req.id << " [" << srcCode << " -> " << tgtCode
                 << " | " << fixed << setprecision(2) << amount << "] -> VIP Priority Queue (Amount >= 5000)." << endl;
        } else {
            regularQueue.push(req);
            cout << "Submitted Request #" << req.id << " [" << srcCode << " -> " << tgtCode
                 << " | " << fixed << setprecision(2) << amount << "] -> Regular FIFO Queue." << endl;
        }
    }

    // Process the highest-priority pending request (VIP first, then FIFO)
    bool processNextRequest(unordered_map<string, Customer*>& customers) {
        if (vipQueue.empty() && regularQueue.empty()) {
            cout << "No pending requests in queue." << endl;
            return false;
        }

        ExchangeRequest req;
        if (!vipQueue.empty()) {
            req = vipQueue.top();
            vipQueue.pop();
            cout << "\n[PROCESSING] VIP Request #" << req.id
                 << " | Customer: " << req.customerId << endl;
        } else {
            req = regularQueue.front();
            regularQueue.pop();
            cout << "\n[PROCESSING] FIFO Regular Request #" << req.id
                 << " | Customer: " << req.customerId << endl;
        }

        if (!customers.count(req.customerId)) {
            cout << "Error: Customer '" << req.customerId << "' not found." << endl;
            return false;
        }

        Customer* cust = customers[req.customerId];
        const string& srcCode = req.source.currency.code;
        const string& tgtCode = req.targetCurrency.code;
        double        amount  = req.source.amount;

        // 1. Find best rate path via graph traversal (UML Page 2: Currency Graph)
        vector<string> path;
        double rate = exchangeRateBook->getBestRate(srcCode, tgtCode, path);
        if (rate <= 0) {
            cout << "Error: No exchange route found: " << srcCode << " -> " << tgtCode << "." << endl;
            return false;
        }

        if (path.size() > 2) {
            cout << "  Route: ";
            for (size_t i = 0; i < path.size(); ++i)
                cout << path[i] << (i + 1 < path.size() ? " -> " : "");
            cout << " (Effective Rate: " << rate << ")" << endl;
        }

        // 2. Calculate fee via Strategy Pattern (UML Page 2 & 5)
        // Request VIP tier determines eligibility for VIP fee discounts
        double fee           = feeService->calculateFee(amount, req.isVip);
        double totalDebit    = amount + fee;

        // 3. Check and deduct source balance (using Money)
        Money debit(totalDebit, req.source.currency);
        if (!cust->withdraw(debit)) {
            cout << "  Transaction Failed: Insufficient " << srcCode << " balance for " << cust->name
                 << " (Available: " << fixed << setprecision(2) << cust->getBalance(srcCode)
                 << ", Needed: " << totalDebit << " [incl. fee " << fee << "])." << endl;
            return false;
        }

        // 4. Deposit converted amount (using Money)
        double convertedAmt = amount * rate;
        Money received(convertedAmt, req.targetCurrency);
        cust->deposit(received);

        // 5. Record transaction with full Money objects
        Transaction tx;
        tx.id           = nextTxId++;
        tx.requestId    = req.id;
        tx.customerId   = cust->id;
        tx.sourceAmount = Money(amount, req.source.currency);
        tx.targetAmount = received;
        tx.fee          = fee;
        tx.rate         = rate;
        tx.timestamp    = getCurrentTime();

        history.push_back(tx);
        undoStack.push(tx);

        // 6. Notify all observers (UML Page 1 & 3)
        notifyObservers(tx);

        return true;
    }

    // Undo last completed transaction using the stack (UML Page 1 & 5)
    bool undoLastTransaction(unordered_map<string, Customer*>& customers) {
        if (undoStack.empty()) {
            cout << "Undo Stack is empty. No transactions to undo." << endl;
            return false;
        }

        Transaction tx = undoStack.top();
        if (!customers.count(tx.customerId)) {
            cout << "Error: Customer not found for undo." << endl;
            return false;
        }

        Customer* cust = customers[tx.customerId];

        // Guard: ensure customer still has the converted amount to reverse
        if (cust->getBalance(tx.targetAmount.currency.code) < tx.targetAmount.amount) {
            cout << "Undo Failed: " << cust->name << " has already spent the converted "
                 << tx.targetAmount.currency.code << " funds." << endl;
            return false;
        }

        // Reverse: take back converted amount, restore original + refund fee
        cust->withdraw(tx.targetAmount);
        double restoreAmt = tx.sourceAmount.amount + tx.fee;
        cust->deposit(Money(restoreAmt, tx.sourceAmount.currency));
        undoStack.pop();

        cout << "\n[UNDO SUCCESSFUL] Reverted Tx #" << tx.id
             << " for " << cust->name << ". Restored "
             << fixed << setprecision(2) << restoreAmt << " " << tx.sourceAmount.currency.code
             << " (fee " << tx.fee << " refunded)." << endl;
        return true;
    }

    // Show full transaction history (vector iteration)
    void showHistory() const {
        cout << "\n================= Transaction History =================" << endl;
        if (history.empty()) {
            cout << "  No transactions completed yet." << endl;
            cout << "=======================================================" << endl;
            return;
        }
        for (const auto& tx : history) {
            cout << "  Tx #" << tx.id
                 << " [" << tx.timestamp << "] Cust: " << tx.customerId
                 << " | " << fixed << setprecision(2)
                 << tx.sourceAmount.amount << " " << tx.sourceAmount.currency.code
                 << " -> " << tx.targetAmount.amount << " " << tx.targetAmount.currency.code
                 << " | Rate: " << tx.rate
                 << " | Fee: " << tx.fee << endl;
        }
        cout << "=======================================================" << endl;
    }

    const vector<Transaction>& getHistory() const {
        return history;
    }

    vector<ExchangeRequest> getRegularQueueRequests() const {
        vector<ExchangeRequest> list;
        queue<ExchangeRequest> temp = regularQueue;
        while (!temp.empty()) {
            list.push_back(temp.front());
            temp.pop();
        }
        return list;
    }

    vector<ExchangeRequest> getVipQueueRequests() const {
        vector<ExchangeRequest> list;
        priority_queue<ExchangeRequest> temp = vipQueue;
        while (!temp.empty()) {
            list.push_back(temp.top());
            temp.pop();
        }
        return list;
    }

    size_t getUndoCount() const {
        return undoStack.size();
    }

    void clearQueues() {
        while (!regularQueue.empty()) regularQueue.pop();
        while (!vipQueue.empty()) vipQueue.pop();
    }
};
