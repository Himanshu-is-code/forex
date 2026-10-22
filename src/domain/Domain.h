#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <ctime>
#include <iomanip>

using namespace std;

// =============================================
// DOMAIN CLASSES (UML Pages 4 & 5)
// =============================================

// Currency value object
class Currency {
public:
    string code;   // e.g. "USD", "INR", "EUR"
    string name;   // e.g. "US Dollar"

    Currency() : code(""), name("") {}
    Currency(string code, string name = "")
        : code(code), name(name.empty() ? code : name) {}

    bool operator==(const Currency& other) const { return code == other.code; }
    bool operator!=(const Currency& other) const { return code != other.code; }
};

// Money value object: an amount paired with its currency
class Money {
public:
    double amount;
    Currency currency;

    Money() : amount(0.0) {}
    Money(double amount, Currency currency)
        : amount(amount), currency(currency) {}

    Money operator+(const Money& other) const {
        // Only valid if same currency
        return Money(amount + other.amount, currency);
    }

    bool operator>=(const Money& other) const {
        return amount >= other.amount;
    }

    string toString() const {
        return to_string(amount) + " " + currency.code;
    }
};

// Customer entity (UML Pages 4 & 5)
class Customer {
public:
    string id;
    string name;
    bool isVip;
    unordered_map<string, double> balances; // currencyCode -> amount

    Customer() : isVip(false) {}
    Customer(string id, string name, bool isVip = false)
        : id(id), name(name), isVip(isVip) {}

    void deposit(const string& currCode, double amt) {
        if (amt > 0) balances[currCode] += amt;
    }

    // Deposit using Money
    void deposit(const Money& money) {
        deposit(money.currency.code, money.amount);
    }

    bool withdraw(const string& currCode, double amt) {
        if (amt > 0 && balances[currCode] >= amt) {
            balances[currCode] -= amt;
            return true;
        }
        return false;
    }

    // Withdraw using Money
    bool withdraw(const Money& money) {
        return withdraw(money.currency.code, money.amount);
    }

    double getBalance(const string& currCode) const {
        auto it = balances.find(currCode);
        return (it != balances.end()) ? it->second : 0.0;
    }

    Money getMoney(const string& currCode) const {
        return Money(getBalance(currCode), Currency(currCode));
    }

    void printBalances() const {
        cout << "Balances for " << name << " (" << (isVip ? "VIP" : "Regular") << "):" << endl;
        for (const auto& [curr, amt] : balances) {
            cout << "  " << curr << ": " << fixed << setprecision(2) << amt << endl;
        }
    }
};

// ExchangeRequest: submitted into FIFO / VIP Queue (UML Pages 1, 4 & 5)
struct ExchangeRequest {
    int id;
    string customerId;
    Money source;              // amount + source currency (uses Money class)
    Currency targetCurrency;   // desired output currency (uses Currency class)
    bool isVip;
    time_t timestamp;

    ExchangeRequest() : id(0), isVip(false), timestamp(0) {}

    // Priority Queue ordering: higher exchange amount = higher priority (Max-Heap),
    // earlier timestamp as tie-breaker
    bool operator<(const ExchangeRequest& other) const {
        if (source.amount != other.source.amount) {
            return source.amount < other.source.amount;
        }
        return timestamp > other.timestamp;
    }
};

// Transaction: completed exchange record (UML Pages 1, 4 & 5)
struct Transaction {
    int id;
    int requestId;
    string customerId;
    Money sourceAmount;    // what was deducted (uses Money class)
    Money targetAmount;    // what was received (uses Money class)
    double fee;
    double rate;
    string timestamp;
};
