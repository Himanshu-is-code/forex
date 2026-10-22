# Forex Exchange Platform

A high-performance foreign exchange trading platform featuring a **C++20 REST API engine** implementing classical Object-Oriented Design (OOD) patterns and Data Structures & Algorithms, paired with a modern **React 19 + Vite** web dashboard and persistent **Supabase PostgreSQL** database.

---

## Architecture Overview

```
                      ┌────────────────────────────────────────┐
                      │        React 19 + Vite Dashboard       │
                      │  (Exchange, Queue, Wallets, Rates Tab) │
                      └───────┬────────────────────────▲───────┘
                              │                        │
                   HTTP / REST│ (Port 8080)     Realtime Sync / RLS
                              ▼                        │
                      ┌────────────────────────────────┴───────┐
                      │    C++20 REST Server (cpp-httplib)     │
                      │     17 Endpoints (server.cpp)          │
                      └──────────────────┬─────────────────────┘
                                         │
        ┌────────────────────────────────┼────────────────────────────────┐
        ▼                                ▼                                ▼
┌──────────────────────┐      ┌──────────────────────┐      ┌──────────────────────┐
│  ForexExchangeService│      │   ExchangeRateBook   │      │      FeeService      │
│  (Meyer's Singleton) │      │  (Adjacency Graph)   │      │  (Strategy Pattern)  │
└──────────┬───────────┘      └──────────┬───────────┘      └──────────┬───────────┘
           │                             │                             │
    ┌──────┴──────────────┐              │ BFS Maximum                 │ Dynamic Switch:
    │  Dual-Queue Engine  │              │ Product Path                ├─ PercentageBased (1.5%)
    │  ├─ FIFO Queue      │              │ Cycle Protection            ├─ FixedCost ($5 / $1 VIP)
    │  └─ Priority Queue  │              └─────────────────────────────┤─ SlabBased (Tiered)
    │     (VIP: Amt≥5000) │                                            └─ VipPrivilege (0% VIP)
    │                     │
    │  Undo Stack (LIFO)  │              ┌─────────────────────────────┐
    │  (Rollback + Refund)│─────────────►│ TransactionObserver Pattern │
    └─────────────────────┘   Notifies   ├─ FileObserver (Disk Log)    │
                                         ├─ PrinterObserver (Receipts) │
                                         ├─ SmsObserver (Simulated SMS)│
                                         └─ DbObserver (DB Event Mock) │
                                         └─────────────────────────────┘
```

---

## Key Features

### 1. Dual-Queue Execution Engine
- **VIP Max-Heap Priority Queue (`std::priority_queue`)**: Orders with amounts $\ge 5,000$ qualify for instant VIP status, jumping ahead in execution order sorted by amount ($O(\log N)$ enqueue, $O(1)$ peek).
- **Regular Queue (`std::queue`)**: Orders $< 5,000$ are processed in fair First-In, First-Out (FIFO) sequence ($O(1)$ enqueue/dequeue).
- **Execution**: The orchestrator always prioritizes and drains the VIP queue before fulfilling regular FIFO orders.

### 2. Multi-Hop Graph Pathfinding (BFS with Cycle Protection)
- Currencies are modeled as a directed weighted adjacency graph (`std::unordered_map<string, unordered_map<string, double>>`).
- Implements a **simple-path BFS algorithm** to compute the maximum product path across intermediate hops (e.g., `USD → JPY → EUR`).
- Includes a **hop-penalty tie-breaker** and a **$0.01\%$ threshold** that prefers direct single-hop routes unless an indirect route offers a meaningful financial improvement, completely eliminating artificial arbitrage inflation.

### 3. LIFO Transaction Undo Stack (`std::stack`)
- Completed transactions are captured as immutable snapshots and pushed to an undo stack.
- Undoing reverses currency balances, refunds the exchange fee, and logs the reversal.
- **Safety Guard**: Verifies the customer still holds the converted currency before permitting an undo operation to prevent negative balance exploits.

### 4. Gang of Four (GoF) Design Patterns

| Pattern | Component | Purpose |
|---|---|---|
| **Strategy Pattern** | [`src/fees/FeeStrategy.h`](src/fees/FeeStrategy.h) | Open-Closed Principle for fee calculations. Allows runtime switching between **Percentage-Based**, **Fixed-Cost**, and **Slab-Based** models. |
| **Observer Pattern** | [`src/observers/TransactionObserver.h`](src/observers/TransactionObserver.h) | Single Responsibility Principle. Decouples trade execution from side effects (`FileObserver` appends to `transactions.log`, `PrinterObserver` prints console receipts, `SmsObserver`, `DbObserver`). |
| **Singleton Pattern** | [`ForexExchangeService.h`](src/service/ForexExchangeService.h), [`ExchangeRateBook.h`](src/rates/ExchangeRateBook.h) | Thread-safe Meyer's Singleton guaranteeing a single source of truth for exchange rates and orchestration state. |

### 5. Persistent Multi-Currency Wallets & Auth
- **Supabase Authentication**: Secure login supporting Email/Password and Google OAuth.
- **PostgreSQL Row-Level Security (RLS)**: Enforces `auth.uid() = customer_id` on profiles, multi-currency wallets, and transaction audit tables.
- **Automatic Sync**: Real-time hydration restores wallet balances into the C++ memory graph on login, and persists balance snapshots after every executed trade, deposit, or rollback.

---

## Technology Stack

- **Backend**: C++20, `cpp-httplib` (single-header HTTP server), `nlohmann/json` (modern JSON parser).
- **Frontend**: React 19, Vite 8, Lucide React icons, Vanilla CSS design tokens.
- **Database**: Supabase PostgreSQL (managed cloud database with RLS).
- **External Data**: Open Exchange Rates API integration for live global forex rates.

---

## Project Structure

```
forex/
├── CMakeLists.txt                 # CMake build configuration (C++20)
├── server.cpp                     # REST API server entrypoint (17 endpoints)
├── main.cpp                       # Interactive CLI demo harness
├── httplib.h                      # cpp-httplib single-header HTTP server
├── json.hpp                       # nlohmann/json single-header library
├── supabase_schema.sql            # Supabase PostgreSQL DDL and RLS policies
├── transactions.log               # Audit log generated by FileObserver
├── src/
│   ├── domain/
│   │   └── Domain.h               # Customer, Currency, Money, ExchangeRequest, Transaction
│   ├── rates/
│   │   └── ExchangeRateBook.h     # Currency graph, rates storage, BFS pathfinding
│   ├── fees/
│   │   └── FeeStrategy.h          # Abstract strategy and concrete fee implementations
│   ├── observers/
│   │   └── TransactionObserver.h  # Observer interfaces and concrete listeners
│   └── service/
│       └── ForexExchangeService.h # Central orchestrator, dual-queue, undo stack
└── frontend/
    ├── package.json               # Frontend dependencies (React 19, Vite, Supabase JS)
    ├── vite.config.js             # Vite configuration
    ├── index.html                 # Single-page application root HTML
    └── src/
        ├── main.jsx               # React entrypoint
        ├── App.jsx                # Main application controller, tab router, state polling
        ├── BalancesView.jsx       # Personal wallet, balance chips, and deposit form
        ├── AuthModal.jsx          # Supabase authentication modal (OAuth + Email)
        ├── api.js                 # API client for C++ REST engine & live rates
        ├── supabaseClient.js      # Supabase JavaScript SDK initialization
        └── index.css              # Custom responsive stylesheet & design system
```

---

## REST API Reference

The C++ server runs on `http://localhost:8080` and exposes **17 REST endpoints**:

### System Health
| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/health` | Check C++ engine status and port |

### Exchange Rates & Graph
| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/rates` | Get all currency graph rates (adjacency list) |
| `POST` | `/api/rates` | Add or update an exchange rate edge (`{ from, to, rate }`) |
| `GET` | `/api/best-rate` | Compute optimal conversion route via BFS (`?from=X&to=Y`) |

### Customer Accounts & Wallets
| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/accounts` | List customer accounts and their multi-currency balances |
| `POST` | `/api/accounts/deposit` | Add funds to a customer's currency balance |
| `POST` | `/api/accounts/onboard` | Register customer with default starter trading balances |
| `POST` | `/api/accounts/update-profile`| Update customer display name |
| `POST` | `/api/accounts/restore` | Hydrate saved balances from Supabase SQL into C++ memory |

### Queue & Trade Execution
| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/queue` | Inspect pending orders in FIFO and VIP queues |
| `POST` | `/api/queue/request` | Submit new exchange request (routed to VIP if amount $\ge 5,000$) |
| `POST` | `/api/queue/clear` | Clear all pending requests from both queues |
| `POST` | `/api/queue/process-next` | Execute the highest-priority pending trade |

### Transactions & Rollbacks
| Method | Endpoint | Description |
|---|---|---|
| `POST` | `/api/undo` | Reverse last executed transaction via LIFO stack |
| `GET` | `/api/transactions` | Retrieve in-memory transaction audit history |

### Fee Strategies
| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/fees/strategies` | List active and available fee strategy options |
| `POST` | `/api/fees/strategy` | Switch active fee strategy (`percentage`, `fixed`, `slab`) |

---

## Database Schema (PostgreSQL / Supabase)

Defined in [`supabase_schema.sql`](supabase_schema.sql):

- **`public.customers`**: User identity linked directly to `auth.users(id)`.
- **`public.customer_wallets`**: Multi-currency ledger records `(customer_id, currency, balance, updated_at)` with `UNIQUE(customer_id, currency)` and `CHECK (balance >= 0)`.
- **`public.transactions`**: Immutable transaction audit log tracking debits, credits, effective exchange rates, and VIP tiers.
- **Row-Level Security (RLS)**: Enforced across all tables so users can only view and update their own wallet and history.

---

## Getting Started

### Prerequisites
- **C++ Compiler**: GCC / Clang / MinGW-w64 with **C++20** support.
- **Node.js**: v18+ and `npm`.
- **Supabase Account**: (Optional, for persistent cloud auth and SQL wallets).

### 1. Build and Run the C++ Server

Using GCC / Clang directly:
```bash
# Compile the REST API server with Winsock (on Windows) or pthreads (on Linux/macOS)
g++ -std=c++20 server.cpp -o forex_server -lws2_32

# Start the server on port 8080
./forex_server
```

Or using CMake:
```bash
cmake -B build
cmake --build build
./build/forex_server
```

### 2. Run the Interactive CLI Demo (Optional)

To test the pure C++ engine in a standalone terminal menu:
```bash
g++ -std=c++20 main.cpp -o forex_cli
./forex_cli
```

### 3. Start the React Frontend

```bash
cd frontend
npm install
npm run dev
```

Open [http://localhost:5173](http://localhost:5173) in your browser to access the dashboard.

---

## Testing & Verification

1. **Exchange Conversion**: Enter amount $\rightarrow$ observe automatic live fee calculation and BFS route path.
2. **Dual-Queue Routing**:
   - Submit an order $< 5,000$ $\rightarrow$ routed to **Regular FIFO Queue**.
   - Submit an order $\ge 5,000$ $\rightarrow$ routed to **VIP Priority Queue** (processed first).
3. **Execution & Rollback**:
   - Click **"Process Next Request"** $\rightarrow$ balances update, observers log to console and `transactions.log`.
   - Click **"Undo"** $\rightarrow$ trade is reversed, converted funds returned, and fee reimbursed.
4. **Live Market Sync**: Click **"Sync Live Rates"** on the Rates tab $\rightarrow$ pulls real-world rates from Open Exchange Rates and updates all graph cross-rate edges.

---

## License

This project is licensed under the MIT License.
