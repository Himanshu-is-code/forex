import React, { useState, useEffect } from 'react';
import { ArrowRightLeft, Layers, Wallet, RotateCcw, ArrowRight, RefreshCw, Zap, LogIn, LogOut, User } from 'lucide-react';
import { api } from './api';
import { supabase } from './supabaseClient';
import AuthModal from './AuthModal';
import BalancesView from './BalancesView';
import './index.css';

export default function App() {
  const [activeTab, setActiveTab] = useState('exchange');
  const [health, setHealth] = useState({ status: 'checking' });
  const [accounts, setAccounts] = useState([]);
  const [queue, setQueue] = useState({ regularQueue: [], vipQueue: [], totalPending: 0 });
  const [transactions, setTransactions] = useState([]);

  // Supabase Auth state
  const [user, setUser] = useState(null);
  const [authModalOpen, setAuthModalOpen] = useState(false);

  // Exchange form
  const [selectedCust, setSelectedCust] = useState('C1');
  const [amount, setAmount] = useState(100);
  const [fromCurr, setFromCurr] = useState('USD');
  const [toCurr, setToCurr] = useState('EUR');
  const [bestRate, setBestRate] = useState(null);

  // Live rates sync state
  const [syncingRates, setSyncingRates] = useState(false);
  const [lastSyncTime, setLastSyncTime] = useState(null);

  // Status message
  const [message, setMessage] = useState(null);

  const showMsg = (text, type = 'success') => {
    setMessage({ text, type });
    setTimeout(() => setMessage(null), 3500);
  };

  const loadData = async () => {
    try {
      const [h, a, q, t] = await Promise.all([
        api.getHealth(),
        api.getAccounts(),
        api.getQueue(),
        api.getTransactions()
      ]);
      setHealth(h);
      if (a) setAccounts(a);
      if (q) setQueue(q);
      if (t) setTransactions(t);
    } catch {
      setHealth({ status: 'offline' });
    }
  };

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 3000);
    return () => clearInterval(interval);
  }, []);

  // Sync C++ engine wallet balances → Supabase SQL (called after trades/deposits)
  const syncWalletsToSupabase = async (u, updatedAccounts) => {
    if (!u) return;
    const myAccount = updatedAccounts?.find(a => a.id === u.email);
    if (!myAccount?.balances) return;
    try {
      // Ensure customer row exists in public.customers
      const displayName = u.user_metadata?.full_name || u.user_metadata?.name || u.email.split('@')[0];
      await supabase.from('customers').upsert({
        id: u.id,
        email: u.email,
        full_name: displayName
      }, { onConflict: 'id' }).catch(() => {});

      const upserts = Object.entries(myAccount.balances).map(([currency, balance]) => ({
        customer_id: u.id,
        currency,
        balance,
        updated_at: new Date().toISOString()
      }));
      await supabase.from('customer_wallets').upsert(upserts, { onConflict: 'customer_id,currency' });
    } catch (err) {
      console.debug('Wallet sync note:', err.message);
    }
  };

  // Restore saved wallet balances from Supabase → C++ engine on login
  const restoreWalletFromSupabase = async (u, displayName) => {
    if (!u) return false;
    try {
      const { data: wallets } = await supabase
        .from('customer_wallets')
        .select('currency,balance')
        .eq('customer_id', u.id);
      if (wallets && wallets.length > 0) {
        const balancesMap = {};
        for (const w of wallets) {
          balancesMap[w.currency] = Number(w.balance);
        }
        await api.restoreBalances(u.email, displayName, balancesMap);
        return true;
      }
    } catch (err) {
      console.debug('Wallet restore note:', err.message);
    }
    return false;
  };

  // Supabase Auth session listener
  useEffect(() => {
    const handleAuthUser = async (u) => {
      setUser(u);
      if (u?.email) {
        setSelectedCust(u.email);
        const displayName = u.user_metadata?.full_name || u.user_metadata?.name || u.email.split('@')[0];

        // Ensure user profile in Supabase
        await supabase.from('customers').upsert({
          id: u.id,
          email: u.email,
          full_name: displayName
        }, { onConflict: 'id' }).catch(() => {});

        // Check if user already has saved wallet balances in Supabase
        const restored = await restoreWalletFromSupabase(u, displayName);
        if (!restored) {
          // New user: onboard in C++ engine (with starter funds) and sync to Supabase
          await api.onboardCustomer(u.email, displayName);
          const fresh = await api.getAccounts();
          if (fresh) {
            setAccounts(fresh);
            await syncWalletsToSupabase(u, fresh);
          }
        }
        loadData();
      }
    };

    supabase.auth.getSession().then(({ data: { session } }) => {
      handleAuthUser(session?.user ?? null);
    });

    const { data: { subscription } } = supabase.auth.onAuthStateChange((_event, session) => {
      handleAuthUser(session?.user ?? null);
    });

    return () => subscription.unsubscribe();
  }, []);

  // Update BFS rate when currencies change
  useEffect(() => {
    if (fromCurr === toCurr) {
      setBestRate({ rate: 1.0, path: [fromCurr] });
      return;
    }
    api.getBestRate(fromCurr, toCurr)
      .then(res => setBestRate(res))
      .catch(() => setBestRate(null));
  }, [fromCurr, toCurr]);

  // Sync Live Market Rates
  const handleSyncLiveRates = async () => {
    setSyncingRates(true);
    try {
      const res = await api.syncLiveRates();
      const timeStr = new Date().toLocaleTimeString();
      setLastSyncTime(timeStr);
      showMsg(`Live market rates synced into C++ Graph at ${timeStr}!`);
      // Refresh current best rate
      api.getBestRate(fromCurr, toCurr)
        .then(r => setBestRate(r))
        .catch(() => setBestRate(null));
      loadData();
    } catch (err) {
      showMsg('Failed to sync live rates: ' + err.message, 'error');
    } finally {
      setSyncingRates(false);
    }
  };

  // Submit to queue
  const handleSubmitExchange = async (e) => {
    e.preventDefault();
    if (!user) {
      setAuthModalOpen(true);
      showMsg('Please sign in to place an exchange order', 'error');
      return;
    }
    try {
      const amtNum = parseFloat(amount);
      const res = await api.submitRequest({
        customerId: selectedCust,
        sourceCurrency: fromCurr,
        targetCurrency: toCurr,
        amount: amtNum
      });
      const queueName = res.isVip ? '★ VIP Priority Queue (Amount ≥ 5,000)' : 'Regular FIFO Queue';
      showMsg(`Submitted to ${queueName} for ${selectedCust}!`);

      // Optionally record to Supabase SQL database if user is logged in
      if (user) {
        try {
          await supabase.from('transactions').insert({
            customer_id: user.id,
            customer_email: user.email,
            from_currency: fromCurr,
            to_currency: toCurr,
            from_amount: amtNum,
            to_amount: amtNum * (bestRate?.rate || 1.0),
            exchange_rate: bestRate?.rate || 1.0,
            is_vip: amtNum >= 5000
          });
        } catch (dbErr) {
          console.debug('Supabase transactions table insert note:', dbErr);
        }
      }

      loadData();
    } catch (err) {
      showMsg(err.message, 'error');
    }
  };

  // Process next request
  const handleProcessNext = async () => {
    try {
      const res = await api.processNext();
      showMsg(res.message || 'Transaction processed successfully!');
      // Fetch fresh accounts then sync balances to Supabase SQL
      const fresh = await api.getAccounts();
      if (fresh) {
        setAccounts(fresh);
        await syncWalletsToSupabase(user, fresh);
      }
      loadData();
    } catch (err) {
      showMsg(err.message, 'error');
    }
  };

  // Clear queues
  const handleClearQueue = async () => {
    try {
      await api.clearQueue();
      showMsg('Queues cleared!');
      loadData();
    } catch (err) {
      showMsg(err.message, 'error');
    }
  };

  // Undo last
  const handleUndo = async () => {
    try {
      const res = await api.undo();
      showMsg(res.message || 'Transaction reversed!');
      const fresh = await api.getAccounts();
      if (fresh) {
        setAccounts(fresh);
        await syncWalletsToSupabase(user, fresh);
      }
      loadData();
    } catch (err) {
      showMsg(err.message, 'error');
    }
  };

  const isVipAmount = parseFloat(amount) >= 5000;

  return (
    <div className="simple-container">
      {/* Header */}
      <div className="simple-header">
        <div className="simple-title">
          <h1>Forex Exchange</h1>
          <p>C++ DSA Engine (BFS Graph, Amount-based Priority Queue, Undo Stack)</p>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          {user ? (
            <div className="user-pill" title={`User ID: ${user.id} • ${user.email}`}>
              {user.user_metadata?.avatar_url ? (
                <img 
                  src={user.user_metadata.avatar_url} 
                  alt="" 
                  style={{ width: 20, height: 20, borderRadius: '50%', objectFit: 'cover' }} 
                />
              ) : (
                <span>👤</span>
              )}
              <span style={{ fontWeight: 600 }}>
                {user.user_metadata?.full_name || user.user_metadata?.name || user.email?.split('@')[0]}
              </span>
              <button 
                type="button" 
                className="auth-btn-signout" 
                onClick={async () => {
                  await supabase.auth.signOut();
                  setUser(null);
                  setSelectedCust('C1');
                  showMsg('Signed out of Supabase');
                }}
              >
                Sign Out
              </button>
            </div>
          ) : (
            <button 
              type="button" 
              className="auth-btn-signin"
              onClick={() => setAuthModalOpen(true)}
            >
              <LogIn size={13} /> Sign In
            </button>
          )}

          <button 
            type="button" 
            onClick={handleSyncLiveRates}
            disabled={syncingRates || health.status !== 'online'}
            className="btn-secondary"
            style={{ 
              padding: '6px 12px', 
              fontSize: 13, 
              display: 'inline-flex', 
              alignItems: 'center', 
              gap: 6,
              cursor: syncingRates ? 'not-allowed' : 'pointer'
            }}
            title="Fetch real-world live Forex rates and update C++ Graph"
          >
            <RefreshCw size={13} className={syncingRates ? 'spin' : ''} />
            {syncingRates ? 'Syncing...' : 'Sync Live Rates'}
          </button>
          <div className={`status-pill ${health.status === 'online' ? '' : 'offline'}`}>
            <span className="dot"></span>
            {health.status === 'online' ? 'Engine Online' : 'Engine Offline'}
          </div>
        </div>
      </div>

      {lastSyncTime && (
        <div style={{ textAlign: 'right', fontSize: 11, color: '#16a34a', marginTop: -16, marginBottom: 16 }}>
          ● Live Market Rates Active (Last synced: {lastSyncTime})
        </div>
      )}

      {/* Alert toast */}
      {message && (
        <div className={`alert-box ${message.type === 'error' ? 'alert-error' : 'alert-success'}`}>
          {message.text}
        </div>
      )}

      {/* Tabs */}
      <div className="tab-bar">
        <button 
          className={`tab-btn ${activeTab === 'exchange' ? 'active' : ''}`}
          onClick={() => setActiveTab('exchange')}
        >
          <ArrowRightLeft size={16} /> Exchange
        </button>
        <button 
          className={`tab-btn ${activeTab === 'queue' ? 'active' : ''}`}
          onClick={() => setActiveTab('queue')}
        >
          <Layers size={16} /> Queues
          {queue.totalPending > 0 && <span className="tab-badge">{queue.totalPending}</span>}
        </button>
        <button 
          className={`tab-btn ${activeTab === 'balances' ? 'active' : ''}`}
          onClick={() => setActiveTab('balances')}
        >
          <Wallet size={16} /> Balances & History
        </button>
      </div>

      {/* Tab 1: Exchange */}
      {activeTab === 'exchange' && (
        <div className="simple-card">
          <form onSubmit={handleSubmitExchange}>
            <div className="row-2">
              <div>
                <label className="input-label">Customer Account</label>
                {user ? (
                  <div style={{ padding: '10px 14px', borderRadius: 8, border: '1px solid var(--border)', background: '#f8fafc', fontSize: 13, fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8 }}>
                    <span>👤</span>
                    <span>{user.user_metadata?.full_name || user.user_metadata?.name || user.email} (You)</span>
                  </div>
                ) : (
                  <div 
                    onClick={() => setAuthModalOpen(true)}
                    style={{ padding: '10px 14px', borderRadius: 8, border: '1px dashed #cbd5e1', background: '#f8fafc', fontSize: 13, color: '#64748b', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: 6 }}
                  >
                    <LogIn size={14} /> Click to Sign In First
                  </div>
                )}
              </div>

              <div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 4 }}>
                  <label className="input-label" style={{ marginBottom: 0 }}>Amount</label>
                  {isVipAmount ? (
                    <span style={{ fontSize: 11, fontWeight: 700, color: '#b45309', backgroundColor: '#fef3c7', padding: '2px 8px', borderRadius: 6 }}>
                      ★ VIP Queue (≥ 5,000)
                    </span>
                  ) : (
                    <span style={{ fontSize: 11, color: '#64748b' }}>
                      Regular Queue (&lt; 5,000)
                    </span>
                  )}
                </div>
                <input 
                  type="number" 
                  step="0.01" 
                  className="input-box" 
                  value={amount} 
                  onChange={e => setAmount(e.target.value)}
                  placeholder={user ? "e.g. 100" : "Sign in to enter amount"}
                  disabled={!user}
                  required 
                />
                {user ? (
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: 4, fontSize: 11 }}>
                    <span style={{ color: 'var(--text-sub)' }}>
                      Available: <strong>{Number(accounts.find(a => a.id === user.email)?.balances?.[fromCurr] || 0).toLocaleString('en-US', { minimumFractionDigits: 2 })} {fromCurr}</strong>
                    </span>
                    {amount && (
                      <span style={{ color: Number(accounts.find(a => a.id === user.email)?.balances?.[fromCurr] || 0) < (parseFloat(amount) * 1.015) ? '#ef4444' : 'var(--text-sub)' }}>
                        Est. Fee (1.5%): ~{(parseFloat(amount) * 0.015).toFixed(2)} {fromCurr}
                      </span>
                    )}
                  </div>
                ) : (
                  <span style={{ fontSize: 11, color: '#94a3b8', display: 'block', marginTop: 4 }}>
                    Orders of 5,000 or more qualify for VIP priority
                  </span>
                )}
              </div>
            </div>

            <div className="row-2">
              <div>
                <label className="input-label">You Send</label>
                <select 
                  className="select-box"
                  value={fromCurr} 
                  onChange={e => setFromCurr(e.target.value)}
                >
                  <option value="USD">USD - US Dollar</option>
                  <option value="EUR">EUR - Euro</option>
                  <option value="GBP">GBP - British Pound</option>
                  <option value="INR">INR - Indian Rupee</option>
                  <option value="JPY">JPY - Japanese Yen</option>
                </select>
              </div>

              <div>
                <label className="input-label">You Receive</label>
                <select 
                  className="select-box"
                  value={toCurr} 
                  onChange={e => setToCurr(e.target.value)}
                >
                  <option value="EUR">EUR - Euro</option>
                  <option value="USD">USD - US Dollar</option>
                  <option value="INR">INR - Indian Rupee</option>
                  <option value="GBP">GBP - British Pound</option>
                  <option value="JPY">JPY - Japanese Yen</option>
                </select>
              </div>
            </div>

            {/* Rate Banner with BFS Route */}
            {bestRate && (
              <div className="rate-banner">
                <div>
                  <span style={{ color: 'var(--text-sub)' }}>Rate: </span>
                  <strong>1 {fromCurr} = {bestRate.rate?.toFixed(4)} {toCurr}</strong>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <span style={{ fontSize: 11, color: 'var(--text-sub)' }}>BFS Route:</span>
                  <span className="route-pill">{bestRate.path?.join(' → ')}</span>
                </div>
              </div>
            )}

            {user ? (
              <button type="submit" className="main-btn">
                Submit to Queue <ArrowRight size={16} />
              </button>
            ) : (
              <button 
                type="button" 
                className="main-btn" 
                style={{ backgroundColor: '#475569', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 8 }}
                onClick={() => setAuthModalOpen(true)}
              >
                <LogIn size={16} /> Sign In to Place Order
              </button>
            )}
          </form>
        </div>
      )}

      {/* Tab 2: Queues */}
      {activeTab === 'queue' && (
        <div className="simple-card">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 14 }}>
            <div>
              <span style={{ fontSize: 14, fontWeight: 600, display: 'block' }}>Pending Requests</span>
              <span style={{ fontSize: 12, color: 'var(--text-sub)' }}>
                Amount ≥ 5,000 enters VIP Priority Queue (Max-Heap: highest amount processed first)
              </span>
            </div>
            <div style={{ display: 'flex', gap: 6, fontSize: 11 }}>
              <span className="vip-tag">★ VIP: {queue.vipQueue?.length || 0}</span>
              <span className="regular-tag">FIFO: {queue.regularQueue?.length || 0}</span>
            </div>
          </div>

          <div className="queue-box">
            {/* VIP Queue */}
            {queue.vipQueue?.map(r => (
              <div key={r.id} className="queue-row vip">
                <div>
                  <span className="vip-tag">★ VIP</span>
                  <strong>{r.customerId}</strong>: <span style={{ fontWeight: 700, color: '#b45309' }}>{Number(r.sourceAmount).toFixed(2)} {r.sourceCurrency}</span> → {r.targetCurrency}
                </div>
                <span style={{ fontSize: 12, color: '#b45309', fontWeight: 600 }}>Priority Heap</span>
              </div>
            ))}

            {/* Regular FIFO Queue */}
            {queue.regularQueue?.map(r => (
              <div key={r.id} className="queue-row">
                <div>
                  <span className="regular-tag">FIFO</span>
                  <strong>{r.customerId}</strong>: {Number(r.sourceAmount).toFixed(2)} {r.sourceCurrency} → {r.targetCurrency}
                </div>
                <span style={{ fontSize: 12, color: 'var(--text-sub)' }}>Standard (&lt; 5,000)</span>
              </div>
            ))}

            {queue.totalPending === 0 && (
              <div style={{ textAlign: 'center', padding: '30px 0', color: 'var(--text-sub)', fontSize: 14 }}>
                No requests pending in queue.
              </div>
            )}
          </div>

          <div className="btn-group">
            <button 
              className="main-btn" 
              style={{ margin: 0 }}
              onClick={handleProcessNext}
              disabled={queue.totalPending === 0}
            >
              Process Next Request
            </button>
            <button 
              className="btn-secondary" 
              onClick={handleUndo}
              title="Undo last completed trade"
            >
              <RotateCcw size={15} /> Undo
            </button>
            <button 
              className="btn-secondary" 
              onClick={handleClearQueue}
              disabled={queue.totalPending === 0}
              title="Clear all pending requests from queue"
              style={{ color: '#ef4444' }}
            >
              Clear Queue
            </button>
          </div>
        </div>
      )}

      {/* Tab 3: Balances & History */}
      {activeTab === 'balances' && (
        <div className="simple-card">
          <div style={{ marginBottom: 16 }}>
            <span style={{ fontSize: 14, fontWeight: 600 }}>My Wallet</span>
            <span style={{ fontSize: 12, color: 'var(--text-sub)', display: 'block', marginTop: 2 }}>
              Orders of 5,000 or more qualify for VIP priority and discounted fees
            </span>
          </div>

          {/* If signed in, show only their account */}
          {user ? (
            <BalancesView 
              user={user} 
              accounts={accounts} 
              transactions={transactions} 
              onDeposit={async (currency, amount) => {
                try {
                  await api.deposit(user.email, currency, parseFloat(amount));
                  showMsg(`Deposited ${amount} ${currency} to your wallet!`);
                  const fresh = await api.getAccounts();
                  if (fresh) {
                    setAccounts(fresh);
                    await syncWalletsToSupabase(user, fresh);
                  }
                  loadData();
                } catch (err) {
                  showMsg(err.message, 'error');
                }
              }} 
            />
          ) : (
            <div className="signin-prompt-box">
              <div>
                <span style={{ fontSize: 14, fontWeight: 600, display: 'block' }}>Sign in to see your wallet</span>
                <span style={{ fontSize: 12, color: '#92400e' }}>Log in to manage your balances and add funds</span>
              </div>
              <button type="button" className="auth-btn-signin" onClick={() => setAuthModalOpen(true)}>
                <LogIn size={13} /> Sign In
              </button>
            </div>
          )}

          <div style={{ borderTop: '1px solid var(--border)', paddingTop: 16, marginTop: 24 }}>
            <span style={{ fontSize: 14, fontWeight: 600, display: 'block', marginBottom: 10 }}>
              Recent Transactions
            </span>
            {transactions
              .filter(tx => !user || tx.customerId === user.email)
              .slice(0, 8).map(tx => {
              const isVipTx = Number(tx.sourceAmount) >= 5000;
              return (
                <div key={tx.id} className="history-row">
                  <div>
                    <strong>#{tx.id}</strong>: {Number(tx.sourceAmount).toFixed(2)} {tx.sourceCurrency} → {Number(tx.targetAmount).toFixed(2)} {tx.targetCurrency}
                    {isVipTx && (
                      <span style={{ marginLeft: 6, fontSize: 10, padding: '2px 6px', borderRadius: 4, backgroundColor: '#fef3c7', color: '#b45309', fontWeight: 700 }}>
                        ★ VIP
                      </span>
                    )}
                  </div>
                  <div style={{ color: 'var(--text-sub)', fontSize: 12 }}>
                    Fee: {Number(tx.fee).toFixed(2)} • Rate: {Number(tx.rate).toFixed(4)}
                  </div>
                </div>
              );
            })}
            {(user ? transactions.filter(tx => tx.customerId === user.email) : transactions).length === 0 && (
              <div style={{ color: 'var(--text-sub)', fontSize: 13, padding: '10px 0' }}>
                No completed transactions yet.
              </div>
            )}
          </div>
        </div>
      )}

      {/* Supabase Auth Modal */}
      <AuthModal
        isOpen={authModalOpen}
        onClose={() => setAuthModalOpen(false)}
        onAuthSuccess={(u) => showMsg(`Welcome, ${u.email}!`)}
      />
    </div>
  );
}
