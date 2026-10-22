import React, { useState } from 'react';
import { PlusCircle, Wallet, CheckCircle2 } from 'lucide-react';

const CURRENCIES = ['USD', 'EUR', 'GBP', 'INR', 'JPY'];

export default function BalancesView({ user, accounts, onDeposit }) {
  const [showDeposit, setShowDeposit] = useState(false);
  const [depCurr, setDepCurr] = useState('USD');
  const [depAmount, setDepAmount] = useState('');
  const [saving, setSaving] = useState(false);

  // Find the logged-in user's account from the C++ engine list
  const myAccount = accounts.find(a => a.id === user?.email);

  const handleDeposit = async (e) => {
    e.preventDefault();
    if (!depAmount || parseFloat(depAmount) <= 0) return;
    setSaving(true);
    await onDeposit(depCurr, depAmount);
    setDepAmount('');
    setShowDeposit(false);
    setSaving(false);
  };

  const displayName = user.user_metadata?.full_name
    || user.user_metadata?.name
    || user.email?.split('@')[0];

  const avatarUrl = user.user_metadata?.avatar_url;

  return (
    <div className="profile-card">
      {/* User identity row */}
      <div className="profile-header-row">
        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          {avatarUrl ? (
            <img
              src={avatarUrl}
              alt=""
              style={{ width: 44, height: 44, borderRadius: '50%', objectFit: 'cover', border: '2px solid #e2e8f0' }}
            />
          ) : (
            <div className="avatar-placeholder">👤</div>
          )}
          <div>
            <span style={{ fontWeight: 700, fontSize: 15, display: 'block' }}>{displayName}</span>
            <span style={{ fontSize: 12, color: 'var(--text-sub)' }}>{user.email}</span>
          </div>
        </div>
        <span className="account-badge">✓ Verified Account</span>
      </div>

      {/* Balances */}
      <div>
        <span style={{ fontSize: 12, fontWeight: 600, color: 'var(--text-sub)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
          <Wallet size={12} style={{ marginRight: 4, verticalAlign: 'middle' }} />
          Wallet Balances
        </span>

        {myAccount ? (
          <div className="curr-chips" style={{ marginTop: 10 }}>
            {Object.entries(myAccount.balances || {}).map(([curr, amt]) => (
              <div key={curr} className="curr-chip" style={{ padding: '8px 14px' }}>
                <span style={{ fontWeight: 600, marginRight: 4 }}>{curr}</span>
                <strong style={{ color: '#0f172a' }}>{Number(amt).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</strong>
              </div>
            ))}
          </div>
        ) : (
          <div style={{ color: 'var(--text-sub)', fontSize: 13, marginTop: 10 }}>
            Wallet loading... (sign in again if balances don't appear)
          </div>
        )}
      </div>

      {/* Deposit Section */}
      <div className="deposit-box">
        {!showDeposit ? (
          <button
            type="button"
            onClick={() => setShowDeposit(true)}
            style={{
              display: 'inline-flex', alignItems: 'center', gap: 6,
              background: 'transparent', border: '1px solid var(--border)',
              color: 'var(--primary)', fontSize: 13, fontWeight: 600,
              padding: '6px 12px', borderRadius: 8, cursor: 'pointer'
            }}
          >
            <PlusCircle size={14} /> Add Funds to Wallet
          </button>
        ) : (
          <form onSubmit={handleDeposit}>
            <span style={{ fontSize: 13, fontWeight: 600, display: 'block', marginBottom: 10 }}>
              Add funds to your wallet
            </span>
            <div style={{ display: 'grid', gridTemplateColumns: '140px 1fr auto auto', gap: 12, alignItems: 'flex-end' }}>
              <div>
                <label className="input-label">Currency</label>
                <select
                  className="select-box"
                  value={depCurr}
                  onChange={e => setDepCurr(e.target.value)}
                >
                  {CURRENCIES.map(c => (
                    <option key={c} value={c}>{c}</option>
                  ))}
                </select>
              </div>
              <div>
                <label className="input-label">Amount</label>
                <input
                  type="number"
                  step="0.01"
                  min="1"
                  className="input-box"
                  placeholder="e.g. 5000"
                  value={depAmount}
                  onChange={e => setDepAmount(e.target.value)}
                  required
                  autoFocus
                />
              </div>
              <button
                type="submit"
                className="main-btn"
                disabled={saving}
                style={{ margin: 0, padding: '11px 22px', whiteSpace: 'nowrap', height: 46 }}
              >
                {saving ? 'Adding...' : 'Add Funds'}
              </button>
              <button
                type="button"
                onClick={() => setShowDeposit(false)}
                className="btn-secondary"
                style={{ margin: 0, padding: '11px 16px', height: 46 }}
              >
                Cancel
              </button>
            </div>
            <span style={{ fontSize: 11, color: 'var(--text-sub)', display: 'block', marginTop: 8 }}>
              Funds are added to your C++ trading wallet immediately.
            </span>
          </form>
        )}
      </div>
    </div>
  );
}
