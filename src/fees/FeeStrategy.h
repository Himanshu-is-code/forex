#pragma once
#include <string>
#include <memory>
#include <iostream>

using namespace std;

// Abstract Fee Strategy (Page 2, 4 & 5 UML)
class FeeStrategy {
public:
    virtual ~FeeStrategy() = default;
    virtual double calculateFee(double amount, bool isVip) const = 0;
    virtual string getName() const = 0;
};

// 1. Percentage-based Strategy (Page 5 UML: "PercentageBased")
class PercentageBasedFeeStrategy : public FeeStrategy {
private:
    double percentage; // e.g. 0.015 = 1.5%
public:
    PercentageBasedFeeStrategy(double pct = 0.015) : percentage(pct) {}

    double calculateFee(double amount, bool isVip) const override {
        double fee = amount * percentage;
        return isVip ? fee * 0.50 : fee; // 50% discount for VIP
    }

    string getName() const override {
        return "Percentage Based (1.5% fee, 50% VIP discount)";
    }
};

// 2. Fixed-cost Strategy (Page 5 UML: "FixedCost")
class FixedCostFeeStrategy : public FeeStrategy {
private:
    double regularFee;
    double vipFee;
public:
    FixedCostFeeStrategy(double regFee = 5.0, double vFee = 1.0)
        : regularFee(regFee), vipFee(vFee) {}

    double calculateFee(double amount, bool isVip) const override {
        return isVip ? vipFee : regularFee;
    }

    string getName() const override {
        return "Fixed Cost ($5.00 Regular / $1.00 VIP)";
    }
};

// 3. Slab-based Strategy (Page 5 UML: "SlabBased")
class SlabBasedFeeStrategy : public FeeStrategy {
public:
    double calculateFee(double amount, bool isVip) const override {
        // Slab 1: > 5000: 0.5%
        // Slab 2: 1000 - 5000: 1.0%
        // Slab 3: < 1000: 2.0%
        double rate = (amount > 5000.0) ? 0.005 : (amount > 1000.0) ? 0.01 : 0.02;
        double fee = amount * rate;
        return isVip ? fee * 0.75 : fee; // 25% extra discount for VIP
    }

    string getName() const override {
        return "Slab Based (Tier 1: 2% (<1k), Tier 2: 1% (1k-5k), Tier 3: 0.5% (>5k))";
    }
};

// 4. VIP Privilege Strategy (Page 1: "VIP customers will be given priority")
class VipPrivilegeStrategy : public FeeStrategy {
public:
    double calculateFee(double amount, bool isVip) const override {
        if (isVip) return 0.0; // 0% fee for VIPs
        return amount * 0.02;  // 2% for regular
    }

    string getName() const override {
        return "VIP Privilege (Zero Fee for VIPs, 2.0% Regular)";
    }
};

// <<Singleton>> FeeService (Page 5 UML)
class FeeService {
private:
    FeeStrategy* strategy;

    FeeService() {
        strategy = new PercentageBasedFeeStrategy();
    }

public:
    static FeeService* getInstance() {
        static FeeService instance;
        return &instance;
    }

    void setStrategy(FeeStrategy* newStrategy) {
        if (newStrategy) {
            strategy = newStrategy;
        }
    }

    FeeStrategy* getStrategy() const {
        return strategy;
    }

    double calculateFee(double amount, bool isVip) const {
        return strategy ? strategy->calculateFee(amount, isVip) : 0.0;
    }
};
