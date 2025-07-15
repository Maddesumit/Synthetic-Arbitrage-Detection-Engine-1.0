#!/bin/bash

# Quick deployment test script
# This script tests the deployment without actually deploying

set -e

echo "🧪 Testing Appwrite Deployment Setup..."

# Check if Appwrite CLI is installed
if ! command -v appwrite &> /dev/null; then
    echo "❌ Appwrite CLI not installed. Run: npm install -g appwrite-cli"
    exit 1
fi

echo "✅ Appwrite CLI is installed"

# Check if logged in
if ! appwrite account get &> /dev/null; then
    echo "❌ Not logged in to Appwrite. Run: appwrite login"
    exit 1
fi

echo "✅ Logged in to Appwrite"

# Check project files
if [ ! -f "appwrite.json" ]; then
    echo "❌ appwrite.json not found"
    exit 1
fi

echo "✅ appwrite.json found"

# Check function files
if [ ! -f "functions/market-data-api/src/main.js" ]; then
    echo "❌ market-data-api function not found"
    exit 1
fi

echo "✅ market-data-api function found"

if [ ! -f "functions/arbitrage-detection/src/main.js" ]; then
    echo "❌ arbitrage-detection function not found"
    exit 1
fi

echo "✅ arbitrage-detection function found"

# Check dashboard
if [ ! -f "dashboard/index.html" ]; then
    echo "❌ dashboard not found"
    exit 1
fi

echo "✅ dashboard found"

echo ""
echo "🎉 All deployment files are ready!"
echo ""
echo "Next steps:"
echo "1. Run: ./deploy.sh"
echo "2. Configure environment variables in Appwrite console"
echo "3. Upload dashboard to Appwrite Storage"
echo "4. Update dashboard with your Appwrite endpoint"
