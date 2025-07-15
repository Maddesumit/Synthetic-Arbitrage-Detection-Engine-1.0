#!/bin/bash

# Simple Appwrite Deployment Script
# This script uses the modern Appwrite CLI commands

set -e

echo "🚀 Deploying Arbitrage Detection Engine to Appwrite"
echo "=================================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Step 1: Using appwrite deploy command...${NC}"

# Since you have appwrite.json configured, let's deploy directly
echo "Using appwrite deploy to deploy everything from appwrite.json..."

# This should deploy functions, databases, and buckets from appwrite.json
appwrite deploy || {
    echo -e "${RED}Deploy failed. Let's try manual approach...${NC}"
    
    # Manual approach
    echo -e "${YELLOW}Trying manual deployment...${NC}"
    
    # Deploy functions manually
    echo "Deploying functions..."
    cd functions/market-data-api
    echo "Installing dependencies for market-data-api..."
    npm install
    cd ../..
    
    cd functions/arbitrage-detection  
    echo "Installing dependencies for arbitrage-detection..."
    npm install
    cd ../..
    
    echo -e "${GREEN}✅ Dependencies installed${NC}"
    echo -e "${YELLOW}Please continue deployment via Appwrite Web Console${NC}"
}

echo -e "${GREEN}🎉 Deployment process completed!${NC}"
echo ""
echo "Next steps:"
echo "1. Go to https://cloud.appwrite.io/console"
echo "2. Verify your project: arbitrage-detection-engine"
echo "3. Check that functions, databases, and storage are created"
echo "4. Upload dashboard/index.html to the dashboard bucket"
echo "5. Configure environment variables in function settings"
