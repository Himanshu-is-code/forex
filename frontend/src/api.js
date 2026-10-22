const API_BASE = (import.meta.env.VITE_API_URL || 'http://localhost:8080').replace(/\/+$/, '') + '/api';

export const api = {
  // Health check
  async getHealth() {
    try {
      const res = await fetch(`${API_BASE}/health`);
      return await res.json();
    } catch {
      return { status: 'offline' };
    }
  },

  // Exchange Rates
  async getRates() {
    const res = await fetch(`${API_BASE}/rates`);
    if (!res.ok) throw new Error('Failed to fetch rates');
    return await res.json();
  },

  async updateRate(from, to, rate) {
    const res = await fetch(`${API_BASE}/rates`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ from, to, rate: parseFloat(rate) })
    });
    if (!res.ok) throw new Error('Failed to update rate');
    return await res.json();
  },

  // BFS Best Rate Path
  async getBestRate(from, to) {
    const res = await fetch(`${API_BASE}/best-rate?from=${from}&to=${to}`);
    if (!res.ok) throw new Error('Failed to calculate best rate');
    return await res.json();
  },

  // Accounts
  async getAccounts() {
    const res = await fetch(`${API_BASE}/accounts`);
    if (!res.ok) throw new Error('Failed to fetch accounts');
    return await res.json();
  },

  async deposit(customerId, currency, amount) {
    const res = await fetch(`${API_BASE}/accounts/deposit`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ customerId, currency, amount: parseFloat(amount) })
    });
    if (!res.ok) throw new Error('Deposit failed');
    return await res.json();
  },

  async onboardCustomer(id, name) {
    try {
      const res = await fetch(`${API_BASE}/accounts/onboard`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id, name })
      });
      return await res.json();
    } catch {
      return null;
    }
  },

  async restoreBalances(id, name, balances) {
    try {
      const res = await fetch(`${API_BASE}/accounts/restore`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id, name, balances })
      });
      return await res.json();
    } catch {
      return null;
    }
  },

  async updateProfile(id, name) {
    const res = await fetch(`${API_BASE}/accounts/update-profile`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id, name })
    });
    if (!res.ok) throw new Error('Failed to update profile');
    return await res.json();
  },

  // Queue
  async getQueue() {
    const res = await fetch(`${API_BASE}/queue`);
    if (!res.ok) throw new Error('Failed to fetch queue');
    return await res.json();
  },

  async submitRequest(data) {
    const res = await fetch(`${API_BASE}/queue/request`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });
    const result = await res.json();
    if (!res.ok) throw new Error(result.error || 'Failed to submit request');
    return result;
  },

  async processNext() {
    const res = await fetch(`${API_BASE}/queue/process-next`, {
      method: 'POST'
    });
    const result = await res.json();
    if (!res.ok) throw new Error(result.message || 'Processing failed');
    return result;
  },

  async clearQueue() {
    const res = await fetch(`${API_BASE}/queue/clear`, {
      method: 'POST'
    });
    const result = await res.json();
    if (!res.ok) throw new Error(result.message || 'Clear queue failed');
    return result;
  },

  async undo() {
    const res = await fetch(`${API_BASE}/undo`, {
      method: 'POST'
    });
    const result = await res.json();
    if (!res.ok) throw new Error(result.message || 'Undo failed');
    return result;
  },

  // Transactions
  async getTransactions() {
    const res = await fetch(`${API_BASE}/transactions`);
    if (!res.ok) throw new Error('Failed to fetch transactions');
    return await res.json();
  },

  // Fee Strategies
  async getFeeStrategies() {
    const res = await fetch(`${API_BASE}/fees/strategies`);
    if (!res.ok) throw new Error('Failed to fetch fee strategies');
    return await res.json();
  },

  async setFeeStrategy(strategy) {
    const res = await fetch(`${API_BASE}/fees/strategy`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ strategy })
    });
    if (!res.ok) throw new Error('Failed to change fee strategy');
    return await res.json();
  },

  // Live Exchange Rates Sync
  async syncLiveRates() {
    const res = await fetch('https://open.er-api.com/v6/latest/USD');
    if (!res.ok) throw new Error('Failed to fetch rates from live API');
    const data = await res.json();
    if (!data.rates) throw new Error('Invalid data received from live API');

    const currencies = ['USD', 'EUR', 'GBP', 'INR', 'JPY'];
    const ratesWithUSD = { ...data.rates, USD: 1.0 };

    const updates = [];
    // Update all pairs consistently from the single live snapshot: rate(A -> B) = rate(USD -> B) / rate(USD -> A)
    for (let i = 0; i < currencies.length; i++) {
      for (let j = i + 1; j < currencies.length; j++) {
        const c1 = currencies[i];
        const c2 = currencies[j];
        if (ratesWithUSD[c1] && ratesWithUSD[c2]) {
          const crossRate = ratesWithUSD[c2] / ratesWithUSD[c1];
          updates.push(api.updateRate(c1, c2, crossRate));
        }
      }
    }
    await Promise.all(updates);
    return {
      success: true,
      time: data.time_last_update_utc,
      rates: data.rates
    };
  }
};
