-- ==============================================================================
-- FOREX EXCHANGE SUPABASE SQL SCHEMA
-- Run this script in your Supabase SQL Editor:
-- https://supabase.com/dashboard/project/ukmjgmryjbtpvulaqnau/sql
-- ==============================================================================

-- 1. Create Customers profile table linked to Supabase Auth
CREATE TABLE IF NOT EXISTS public.customers (
    id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT NOT NULL,
    full_name TEXT,
    is_vip BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 2. Wallets / Balances table (multi-currency balances per customer)
CREATE TABLE IF NOT EXISTS public.customer_wallets (
    id BIGSERIAL PRIMARY KEY,
    customer_id UUID REFERENCES public.customers(id) ON DELETE CASCADE,
    currency VARCHAR(3) NOT NULL,
    balance NUMERIC(15, 2) DEFAULT 1000.00 CHECK (balance >= 0),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL,
    UNIQUE (customer_id, currency)
);

-- 3. Transactions Audit Log table
CREATE TABLE IF NOT EXISTS public.transactions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    customer_id UUID REFERENCES public.customers(id) ON DELETE CASCADE,
    customer_email TEXT,
    from_currency VARCHAR(3) NOT NULL,
    to_currency VARCHAR(3) NOT NULL,
    from_amount NUMERIC(15, 2) NOT NULL,
    to_amount NUMERIC(15, 2) NOT NULL,
    exchange_rate NUMERIC(15, 6) NOT NULL,
    is_vip BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 4. Enable Row Level Security (RLS)
ALTER TABLE public.customers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.customer_wallets ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transactions ENABLE ROW LEVEL SECURITY;

-- 5. Security Policies: Users can only read/write their own data
CREATE POLICY "Users can view their own profile" 
ON public.customers FOR SELECT 
USING (auth.uid() = id);

CREATE POLICY "Users can insert their own profile" 
ON public.customers FOR INSERT 
WITH CHECK (auth.uid() = id);

CREATE POLICY "Users can update their own profile" 
ON public.customers FOR UPDATE 
USING (auth.uid() = id);

CREATE POLICY "Users can view their own wallets" 
ON public.customer_wallets FOR SELECT 
USING (auth.uid() = customer_id);

CREATE POLICY "Users can insert their own wallets" 
ON public.customer_wallets FOR INSERT 
WITH CHECK (auth.uid() = customer_id);

CREATE POLICY "Users can update their own wallets" 
ON public.customer_wallets FOR UPDATE 
USING (auth.uid() = customer_id);

CREATE POLICY "Users can view their own transactions" 
ON public.transactions FOR SELECT 
USING (auth.uid() = customer_id);

CREATE POLICY "Users can insert transactions" 
ON public.transactions FOR INSERT 
WITH CHECK (auth.uid() = customer_id);
