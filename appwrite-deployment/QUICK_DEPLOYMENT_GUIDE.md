# 🚀 Appwrite Deployment Guide - Arbitrage Detection Engine

## Current Status: CLI Login Issue

You're experiencing a login issue with the Appwrite CLI. Here are **3 working solutions**:

---

## 🎯 **Solution 1: Web Console Deployment (Recommended)**

### Step 1: Go to Appwrite Console
1. Visit: https://cloud.appwrite.io/console
2. Login with your account: `maddesumit@gmail.com`

### Step 2: Create Project
- Click "Create Project"
- Project ID: `arbitrage-detection-engine`
- Name: `Synthetic Arbitrage Detection Engine`
- Region: Choose your preferred region
- Click "Create"

### Step 3: Create Database
1. Go to "Databases" → "Create Database"
2. Database ID: `arbitrage-db`
3. Name: `Arbitrage Database`
4. Click "Create"

### Step 4: Create Collections

**Collection 1: Market Data**
1. Click "Create Collection"
2. Collection ID: `market-data`
3. Name: `Market Data`
4. Permissions: `read("any")`, `write("any")`
5. Add Attributes:
   - `symbol` (string, 20 chars, required)
   - `exchange` (string, 20 chars, required)
   - `price` (double, required)
   - `volume` (double, required)
   - `timestamp` (datetime, required)

**Collection 2: Arbitrage Opportunities**
1. Click "Create Collection"
2. Collection ID: `arbitrage-opportunities`
3. Name: `Arbitrage Opportunities`  
4. Permissions: `read("any")`, `write("any")`
5. Add Attributes:
   - `symbol` (string, 20 chars, required)
   - `exchange_a` (string, 20 chars, required)
   - `exchange_b` (string, 20 chars, required)
   - `price_a` (double, required)
   - `price_b` (double, required)
   - `profit_potential` (double, required)
   - `profit_percentage` (double, required)
   - `confidence_score` (double, required)
   - `status` (string, 20 chars, required)
   - `detected_at` (datetime, required)

### Step 5: Create Functions

**Function 1: Market Data API**
1. Go to "Functions" → "Create Function"
2. Function ID: `market-data-api`
3. Name: `Market Data API`
4. Runtime: `Node.js 18.0`
5. Execute: `any`
6. Timeout: `30` seconds
7. Upload: Zip the `functions/market-data-api` folder and upload
8. Entrypoint: `src/main.js`

**Function 2: Arbitrage Detection**
1. Go to "Functions" → "Create Function"
2. Function ID: `arbitrage-detection`
3. Name: `Arbitrage Detection`
4. Runtime: `Node.js 18.0`
5. Execute: `any`
6. Timeout: `60` seconds
7. Schedule: `*/5 * * * *` (every 5 minutes)
8. Upload: Zip the `functions/arbitrage-detection` folder and upload
9. Entrypoint: `src/main.js`

### Step 6: Create Storage Bucket
1. Go to "Storage" → "Create Bucket"
2. Bucket ID: `dashboard`
3. Name: `Dashboard Files`
4. Permissions: `read("any")`
5. File Security: `true`
6. Maximum File Size: `10MB`
7. Allowed Extensions: `html, css, js, png, jpg, svg`

### Step 7: Upload Dashboard
1. Go to your `dashboard` bucket
2. Click "Create File"
3. Upload `dashboard/index.html`
4. Set permissions to `read("any")`

---

## 🔧 **Solution 2: Fix CLI Login**

### Reset Appwrite CLI
```bash
# Clear cache
rm -rf ~/.appwrite

# Reinstall CLI
npm uninstall -g appwrite-cli
npm install -g appwrite-cli

# Login again
appwrite login
```

### Then Run Deployment
```bash
cd "/Users/sumitmadde/Desktop/Synthetic Pair Deviation Engine/appwrite-deployment"
./deploy-step-by-step.sh
```

---

## 🐙 **Solution 3: GitHub Actions Deployment**

I can create a GitHub Actions workflow for automated deployment.

---

## 📋 **What's Already Prepared**

✅ **Project Structure**: All files are ready
✅ **Functions**: Both functions have dependencies installed
✅ **Database Schema**: Complete schema in `appwrite.json`
✅ **Dashboard**: Beautiful web interface ready
✅ **Deployment Scripts**: Multiple deployment options

---

## 🔥 **Quick Commands to Try**

```bash
# Check if login is actually working
appwrite account get

# Try creating project directly
appwrite projects create --project-id arbitrage-detection-engine --name "Synthetic Arbitrage Detection Engine"

# List existing projects
appwrite projects list
```

---

## 🎨 **Your Dashboard Features**

- **Real-time arbitrage opportunities**
- **Interactive charts with Chart.js**
- **Bootstrap 5 responsive design**
- **Auto-refresh every 30 seconds**
- **Exchange integration status**
- **Profit/loss tracking**

---

## 🔑 **Environment Variables to Set**

After deployment, add these in the Appwrite console:

```
BINANCE_API_KEY=your_binance_api_key
BYBIT_API_KEY=your_bybit_api_key
OKX_API_KEY=your_okx_api_key
WEBSOCKET_TIMEOUT=30000
MAX_RECONNECT_ATTEMPTS=5
```

---

## 📊 **API Endpoints**

Once deployed, you'll have:

- `POST /v1/functions/market-data-api/executions` - Market data API
- `POST /v1/functions/arbitrage-detection/executions` - Arbitrage detection
- `GET /v1/storage/buckets/dashboard/files/dashboard-html/view` - Dashboard

---

## 🚀 **Next Steps**

1. **Choose Solution 1 (Web Console)** - Most reliable
2. **Deploy all components** following the guide above
3. **Configure environment variables** in Appwrite console
4. **Update dashboard endpoint** in `index.html`
5. **Test API endpoints** and dashboard
6. **Connect to live exchange feeds**

**Everything is ready for deployment! The Web Console approach will definitely work.** 🎉
