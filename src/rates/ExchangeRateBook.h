#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <iomanip>

using namespace std;

// ExchangeRate Book Singleton (Page 2, 4 & 5 UML)
class ExchangeRateBook {
private:
    // Currency Graph Adjacency List: from -> (to -> rate)
    unordered_map<string, unordered_map<string, double>> rates;

    ExchangeRateBook() {
        // Initialize consistent default exchange rates
        // Base USD (1.0): EUR: 0.92, GBP: 0.79, INR: 83.25, JPY: 154.50
        setRate("USD", "EUR", 0.92);
        setRate("USD", "GBP", 0.79);
        setRate("USD", "INR", 83.25);
        setRate("USD", "JPY", 154.50);

        // Consistent cross rates across non-USD pairs
        setRate("EUR", "INR", 83.25 / 0.92);
        setRate("GBP", "INR", 83.25 / 0.79);
        setRate("EUR", "GBP", 0.79 / 0.92);
        setRate("EUR", "JPY", 154.50 / 0.92);
        setRate("GBP", "JPY", 154.50 / 0.79);
        setRate("INR", "JPY", 154.50 / 83.25);
    }

public:
    // Meyer's Singleton: thread-safe, no external definition required, no duplicate symbols
    static ExchangeRateBook* getInstance() {
        static ExchangeRateBook instance;
        return &instance;
    }

    void setRate(const string& from, const string& to, double rate) {
        if (rate <= 0) return;
        rates[from][to] = rate;
        rates[to][from] = 1.0 / rate;
    }

    double getDirectRate(const string& from, const string& to) const {
        auto itFrom = rates.find(from);
        if (itFrom != rates.end()) {
            auto itTo = itFrom->second.find(to);
            if (itTo != itFrom->second.end()) {
                return itTo->second;
            }
        }
        return 0.0;
    }

    const unordered_map<string, unordered_map<string, double>>& getAllRates() const {
        return rates;
    }

    // Best exchange rate path using simple-path BFS (Page 2 Graph)
    // Strictly enforces simple paths (no repeated currencies) to eliminate arbitrage cycle inflation
    double getBestRate(const string& from, const string& to, vector<string>& bestPath) {
        if (from == to) {
            bestPath = {from};
            return 1.0;
        }

        struct PathState {
            string node;
            vector<string> path;
            double rate;
        };

        queue<PathState> q;
        q.push({from, {from}, 1.0});

        double bestRateFound = 0.0;
        vector<string> bestPathFound;

        // Baseline: direct rate if exists
        double direct = getDirectRate(from, to);
        if (direct > 0.0) {
            bestRateFound = direct;
            bestPathFound = {from, to};
        }

        while (!q.empty()) {
            auto curr = q.front();
            q.pop();

            if (curr.node == to && curr.path.size() > 1) {
                bool isBetter = false;
                if (bestPathFound.empty()) {
                    isBetter = true;
                } else if (curr.rate > bestRateFound * 1.0001) {
                    // Accept indirect route only if it genuinely beats the current best by > 0.01%
                    isBetter = true;
                } else if (curr.rate >= bestRateFound * 0.9999 && curr.path.size() < bestPathFound.size()) {
                    // On tie or near-tie (e.g. floating-point rounding), strictly prefer the path with fewer hops
                    isBetter = true;
                }

                if (isBetter) {
                    bestRateFound = curr.rate;
                    bestPathFound = curr.path;
                }
                continue;
            }

            // Prune paths longer than total unique currencies
            if (curr.path.size() >= rates.size()) continue;

            auto it = rates.find(curr.node);
            if (it == rates.end()) continue;

            for (const auto& [next, edgeRate] : it->second) {
                // Prevent cycles: each currency can only appear once in a valid conversion path
                bool alreadyInPath = false;
                for (const auto& p : curr.path) {
                    if (p == next) {
                        alreadyInPath = true;
                        break;
                    }
                }
                if (alreadyInPath) continue;

                vector<string> nextPath = curr.path;
                nextPath.push_back(next);
                q.push({next, nextPath, curr.rate * edgeRate});
            }
        }

        if (bestRateFound > 0.0 && !bestPathFound.empty()) {
            bestPath = bestPathFound;
            return bestRateFound;
        }

        bestPath.clear();
        return 0.0;
    }

    void printRates() const {
        cout << "\n--- Direct Exchange Rates ---" << endl;
        for (const auto& [from, targets] : rates) {
            for (const auto& [to, rate] : targets) {
                cout << "  1 " << from << " = " << fixed << setprecision(4) << rate << " " << to << endl;
            }
        }
    }
};
