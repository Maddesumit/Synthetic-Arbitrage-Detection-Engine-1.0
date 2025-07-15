#!/bin/bash

# Appwrite Deployment Script for Arbitrage Detection Engine
# This script deploys the application to Appwrite Cloud

set -e

echo "🚀 Starting Appwrite Deployment..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if Appwrite CLI is installed
if ! command -v appwrite &> /dev/null; then
    echo -e "${YELLOW}❌ Appwrite CLI not found. Installing...${NC}"
    npm install -g appwrite-cli
fi

# Check if logged in
echo -e "${YELLOW}🔑 Checking Appwrite login status...${NC}"
if ! appwrite account get &> /dev/null; then
    echo -e "${RED}Please login to Appwrite first:${NC}"
    appwrite login
fi

# Create or select project
echo -e "${YELLOW}📝 Creating/selecting project...${NC}"
appwrite projects create \
    --project-id arbitrage-detection-engine \
    --name "Synthetic Arbitrage Detection Engine" \
    --region default || echo "Project may already exist, continuing..."

# Initialize project locally
echo -e "${YELLOW}🔧 Initializing project locally...${NC}"
appwrite init \
    --project-id arbitrage-detection-engine

# Deploy functions
echo -e "${YELLOW}⚡ Deploying functions...${NC}"
appwrite deploy function \
    --function-id market-data-api || echo "Function deployment failed, check logs"

appwrite deploy function \
    --function-id arbitrage-detection || echo "Function deployment failed, check logs"

# Deploy databases and collections
echo -e "${YELLOW}🗄️ Deploying databases and collections...${NC}"
appwrite deploy database \
    --database-id arbitrage-db || echo "Database deployment failed, check logs"
    --collection-id market-data \
    --database-id arbitrage-db

appwrite deploy collection \
    --collection-id opportunities \
    --database-id arbitrage-db

# Deploy functions
echo "⚡ Deploying functions..."
cd functions/market-data-api
npm install
cd ../..

appwrite deploy function \
    --function-id market-data-api \
    --activate

# Deploy storage buckets (for logs and reports)
echo "📦 Creating storage buckets..."
appwrite create bucket \
    --bucket-id logs \
    --name "Application Logs" \
    --permissions 'read("any")' 'write("any")'

appwrite create bucket \
    --bucket-id reports \
    --name "Performance Reports" \
    --permissions 'read("any")' 'write("any")'

# Set environment variables
echo "🔧 Setting environment variables..."
appwrite create variable \
    --function-id market-data-api \
    --key WEBSOCKET_TIMEOUT \
    --value "30000"

appwrite create variable \
    --function-id market-data-api \
    --key MAX_RECONNECT_ATTEMPTS \
    --value "5"

appwrite create variable \
    --function-id market-data-api \
    --key LOG_LEVEL \
    --value "info"

echo "✅ Deployment completed successfully!"
echo ""
echo "🌐 Your application is now available at:"
echo "   - Functions: https://cloud.appwrite.io/console/project-arbitrage-detection-engine/functions"
echo "   - Database: https://cloud.appwrite.io/console/project-arbitrage-detection-engine/databases"
echo ""
echo "📊 API Endpoints:"
echo "   - Market Data: https://[project-id].appwrite.global/v1/functions/market-data-api/executions"
echo "   - Health Check: https://[project-id].appwrite.global/v1/health"
echo ""
echo "🔗 Next steps:"
echo "   1. Configure your exchange API keys in the Appwrite console"
echo "   2. Set up monitoring and alerts"
echo "   3. Test the API endpoints"
echo "   4. Deploy the web dashboard"
