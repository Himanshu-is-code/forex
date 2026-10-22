#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/domain/Domain.h"
#include "src/rates/ExchangeRateBook.h"
#include "src/fees/FeeStrategy.h"
#include "src/observers/TransactionObserver.h"
#include "src/service/ForexExchangeService.h"

using namespace std;

int main() {
    cout << fixed << setprecision(2);

    // === Setup: Singleton Service ===
    ForexExchangeService* service = ForexExchangeService::getInstance();

    // === Register Observers (Observer Pattern - UML Page 3) ===
    // All 4 concrete observers are registered via addObserver()
    PrinterObserver* printerObs = new PrinterObserver();
    SmsObserver*     smsObs     = new SmsObserver();
    FileObserver*    fileObs    = new FileObserver();
    DbObserver*      dbObs      = new DbObserver();

    service->addObserver(printerObs);
    service->addObserver(smsObs);
    service->addObserver(fileObs);
    service->addObserver(dbObs);

    // === Seed Customers (Customer class - UML Pages 4 & 5) ===
    // Using Currency and Money through deposit()
    unordered_map<string, Customer*> customers;
    customers["C1"] = new Customer("C1", "Asha", false);  // Regular customer
    customers["C2"] = new Customer("C2", "Ravi", true);   // VIP customer

    // Deposit balances using Money objects (Money + Currency classes)
    customers["C1"]->deposit(Money(1000.00, Currency("USD", "US Dollar")));
    customers["C1"]->deposit(Money(50000.00, Currency("INR", "Indian Rupee")));
    customers["C2"]->deposit(Money(2000.00, Currency("EUR", "Euro")));
    customers["C2"]->deposit(Money(1500.00, Currency("USD", "US Dollar")));

    int choice;
    while (true) {
        cout << "\n===============================================" << endl;
        cout << "        FOREX MONEY EXCHANGE SYSTEM           " << endl;
        cout << "===============================================" << endl;
        cout << "1.  Show Exchange Rates" << endl;
        cout << "2.  Submit Exchange Request" << endl;
        cout << "3.  Process Next Request (VIP first, then FIFO)" << endl;
        cout << "4.  Show Transaction History" << endl;
        cout << "5.  Undo Last Transaction (Stack)" << endl;
        cout << "6.  Show Customer Balances" << endl;
        cout << "7.  Add / Update Exchange Rate" << endl;
        cout << "8.  Select Fee Strategy (Strategy Pattern)" << endl;
        cout << "9.  Find Best Multi-Hop Rate Path (Graph)" << endl;
        cout << "10. Add Observer" << endl;
        cout << "11. Remove Observer" << endl;
        cout << "0.  Exit" << endl;
        cout << "-----------------------------------------------" << endl;
        cout << "Enter Choice: ";

        if (!(cin >> choice)) {
            cin.clear();
            string discard;
            cin >> discard;
            cout << "Invalid input. Enter a number." << endl;
            continue;
        }

        if (choice == 0) {
            cout << "Exiting Forex Money Exchange. Goodbye!" << endl;
            break;
        }

        try {
            // --- 1. Show Exchange Rates ---
            if (choice == 1) {
                ExchangeRateBook::getInstance()->printRates();

            // --- 2. Submit Exchange Request ---
            } else if (choice == 2) {
                string customerId, srcCode, tgtCode;
                double amount;

                cout << "\nAvailable Customers: C1 (Regular / Asha), C2 (VIP / Ravi)" << endl;
                cout << "Customer ID: ";
                cin >> customerId;

                if (!customers.count(customerId)) {
                    int vipChoice;
                    cout << "New customer. Is VIP? (1=Yes, 0=No): ";
                    cin >> vipChoice;
                    customers[customerId] = new Customer(customerId, customerId, vipChoice == 1);
                }

                cout << "Source Currency (USD / EUR / INR / GBP / JPY): ";
                cin >> srcCode;
                cout << "Amount to Exchange: ";
                cin >> amount;
                cout << "Target Currency (USD / EUR / INR / GBP / JPY): ";
                cin >> tgtCode;

                if (amount <= 0) { cout << "Amount must be > 0." << endl; continue; }

                bool qualifiesVip = (amount >= 5000.0);
                service->submitRequest(customerId, srcCode, tgtCode, amount, qualifiesVip);

            // --- 3. Process Next Request ---
            } else if (choice == 3) {
                service->processNextRequest(customers);

            // --- 4. Show History ---
            } else if (choice == 4) {
                service->showHistory();

            // --- 5. Undo Last Transaction ---
            } else if (choice == 5) {
                service->undoLastTransaction(customers);

            // --- 6. Show Balances ---
            } else if (choice == 6) {
                cout << "\n--- Customer Balances ---" << endl;
                for (const auto& [id, c] : customers)
                    c->printBalances();

            // --- 7. Update Rate ---
            } else if (choice == 7) {
                string from, to;
                double rate;
                cout << "From Currency: "; cin >> from;
                cout << "To Currency:   "; cin >> to;
                cout << "Rate (1 " << from << " = ? " << to << "): "; cin >> rate;
                if (rate <= 0) { cout << "Rate must be positive." << endl; continue; }
                ExchangeRateBook::getInstance()->setRate(from, to, rate);
                cout << "Updated: 1 " << from << " = " << rate << " " << to << endl;

            // --- 8. Select Fee Strategy ---
            } else if (choice == 8) {
                int s;
                cout << "\nFee Strategies:" << endl;
                cout << "1. Percentage Based  (1.5%, VIP gets 50% off)" << endl;
                cout << "2. Fixed Cost        ($5 regular / $1 VIP)" << endl;
                cout << "3. Slab Based        (2% <1k | 1% 1k-5k | 0.5% >5k)" << endl;
                cout << "4. VIP Privilege     (0% VIP | 2% regular)" << endl;
                cout << "Select (1-4): "; cin >> s;

                if      (s == 1) service->setFeeStrategy(new PercentageBasedFeeStrategy());
                else if (s == 2) service->setFeeStrategy(new FixedCostFeeStrategy());
                else if (s == 3) service->setFeeStrategy(new SlabBasedFeeStrategy());
                else if (s == 4) service->setFeeStrategy(new VipPrivilegeStrategy());
                else { cout << "Invalid option." << endl; continue; }

                cout << "Active Strategy: " << service->getFeeStrategy()->getName() << endl;

            // --- 9. Best Multi-Hop Rate ---
            } else if (choice == 9) {
                string from, to;
                cout << "From Currency: "; cin >> from;
                cout << "To Currency:   "; cin >> to;
                vector<string> path;
                double bestRate = ExchangeRateBook::getInstance()->getBestRate(from, to, path);
                if (bestRate <= 0) {
                    cout << "No route found: " << from << " -> " << to << "." << endl;
                } else {
                    cout << "Best Path: ";
                    for (size_t i = 0; i < path.size(); ++i)
                        cout << path[i] << (i + 1 < path.size() ? " -> " : "");
                    cout << "\nEffective Rate: " << fixed << setprecision(6) << bestRate << endl;
                }

            // --- 10. Add Observer (demo removeObserver) ---
            } else if (choice == 10) {
                int obs;
                cout << "1. PrinterObserver  2. SmsObserver  3. FileObserver  4. DbObserver" << endl;
                cout << "Select observer to add: "; cin >> obs;
                if      (obs == 1) { service->addObserver(new PrinterObserver()); cout << "PrinterObserver added." << endl; }
                else if (obs == 2) { service->addObserver(new SmsObserver());     cout << "SmsObserver added."    << endl; }
                else if (obs == 3) { service->addObserver(new FileObserver());    cout << "FileObserver added."   << endl; }
                else if (obs == 4) { service->addObserver(new DbObserver());      cout << "DbObserver added."     << endl; }
                else cout << "Invalid observer." << endl;

            // --- 11. Remove Observer ---
            } else if (choice == 11) {
                int obs;
                cout << "1. PrinterObserver  2. SmsObserver  3. FileObserver  4. DbObserver" << endl;
                cout << "Select observer to remove: "; cin >> obs;
                if      (obs == 1) { service->removeObserver(printerObs); cout << "PrinterObserver removed." << endl; }
                else if (obs == 2) { service->removeObserver(smsObs);     cout << "SmsObserver removed."     << endl; }
                else if (obs == 3) { service->removeObserver(fileObs);    cout << "FileObserver removed."    << endl; }
                else if (obs == 4) { service->removeObserver(dbObs);      cout << "DbObserver removed."      << endl; }
                else cout << "Invalid observer." << endl;

            } else {
                cout << "Invalid choice. Select 0-11." << endl;
            }

        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
        }
    }

    return 0;
}
