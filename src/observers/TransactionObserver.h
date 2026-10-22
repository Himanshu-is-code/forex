#pragma once
#include <iostream>
#include <fstream>
#include "../domain/Domain.h"

using namespace std;

// =============================================
// OBSERVER PATTERN (UML Pages 1, 3 & 5)
// Subject calls notifyObservers() after every transaction.
// Each concrete observer reacts independently.
// =============================================

// <<Abstract>> Observer base class
class TransactionObserver {
public:
    virtual ~TransactionObserver() = default;
    virtual void update(const Transaction& tx) = 0;
};

// Concrete Observer 1: Console / Printer receipt
class PrinterObserver : public TransactionObserver {
public:
    void update(const Transaction& tx) override {
        cout << "[PRINTER RECEIPT] Tx #" << tx.id
             << " | Customer: " << tx.customerId
             << " | " << fixed << setprecision(2) << tx.sourceAmount.amount << " " << tx.sourceAmount.currency.code
             << " -> " << tx.targetAmount.amount << " " << tx.targetAmount.currency.code
             << " | Rate: " << tx.rate
             << " | Fee: " << tx.fee
             << " | Time: " << tx.timestamp << endl;
    }
};

// Concrete Observer 2: SMS notification
class SmsObserver : public TransactionObserver {
public:
    void update(const Transaction& tx) override {
        cout << "[SMS NOTIFICATION] Customer " << tx.customerId
             << ": Your exchange of " << fixed << setprecision(2)
             << tx.sourceAmount.amount << " " << tx.sourceAmount.currency.code
             << " -> " << tx.targetAmount.amount << " " << tx.targetAmount.currency.code
             << " was successful!" << endl;
    }
};

// Concrete Observer 3: File logger (writes to transactions.log)
class FileObserver : public TransactionObserver {
public:
    void update(const Transaction& tx) override {
        ofstream logFile("transactions.log", ios::app);
        if (logFile.is_open()) {
            logFile << "TxID:" << tx.id
                    << " | Cust:" << tx.customerId
                    << " | From:" << fixed << setprecision(2)
                    << tx.sourceAmount.amount << " " << tx.sourceAmount.currency.code
                    << " | To:" << tx.targetAmount.amount << " " << tx.targetAmount.currency.code
                    << " | Fee:" << tx.fee
                    << " | Rate:" << tx.rate
                    << " | Time:" << tx.timestamp << "\n";
            logFile.close();
        }
    }
};

// Concrete Observer 4: Database persistence
class DbObserver : public TransactionObserver {
public:
    void update(const Transaction& tx) override {
        cout << "[DB PERSISTENCE] Inserted Tx #" << tx.id
             << " into 'forex_transactions' table." << endl;
    }
};
