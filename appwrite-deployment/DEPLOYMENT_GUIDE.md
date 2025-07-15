# Appwrite Deployment Guide

## Prerequisites

1. **Appwrite Account**: Sign up at [appwrite.io](https://appwrite.io)
2. **Appwrite CLI**: Installed globally (already done)
3. **Node.js**: Version 18 or higher

## Quick Start Deployment

### Step 1: Login to Appwrite

```bash
appwrite login
```

### Step 2: Initialize Project

```bash
cd appwrite-deployment
appwrite init project
```

When prompted:
- Project ID: `arbitrage-detection-engine`
- Project Name: `Synthetic Arbitrage Detection Engine`

### Step 3: Deploy Functions

```bash
# Deploy market data API
appwrite deploy function --function-id market-data-api

# Deploy arbitrage detection
appwrite deploy function --function-id arbitrage-detection
```

### Step 4: Deploy Database

```bash
appwrite deploy collection
```

### Step 5: Configure Environment Variables

In your Appwrite console:
1. Go to Functions → market-data-api → Settings
2. Add environment variables:
   - `BINANCE_API_KEY`: Your Binance API key
   - `BYBIT_API_KEY`: Your Bybit API key
   - `OKX_API_KEY`: Your OKX API key
   - `WEBSOCKET_TIMEOUT`: 30000
   - `MAX_RECONNECT_ATTEMPTS`: 5

### Step 6: Deploy Dashboard

1. Upload `dashboard/index.html` to Appwrite Storage
2. Enable public access for the file
3. Update the Appwrite endpoint in the HTML file

## Alternative: Automated Deployment

Run the deployment script:

```bash
chmod +x deploy.sh
./deploy.sh
```

## Architecture Overview

### Functions
- **market-data-api**: REST API for market data and opportunities
- **arbitrage-detection**: Scheduled function for detecting opportunities

### Database Collections
- **market-data**: Real-time market data
- **arbitrage-opportunities**: Detected opportunities
- **risk-metrics**: Risk assessment data
- **performance-metrics**: Performance tracking

### API Endpoints

#### Market Data API
- `GET /market-data?exchange=binance&symbol=BTC/USDT`
- `GET /opportunities`
- `GET /risk-metrics`
- `GET /performance`

## Monitoring and Maintenance

### Function Logs
```bash
appwrite logs --function-id market-data-api
appwrite logs --function-id arbitrage-detection
```

### Database Queries
```bash
appwrite databases list-documents --database-id arbitrage-db --collection-id arbitrage-opportunities
```

### Performance Monitoring
- Check function execution times in Appwrite console
- Monitor database query performance
- Set up alerts for failed executions

## Integration with C++ Engine

The current deployment uses mock data. To integrate with your C++ engine:

1. **Container Deployment**: Use the provided Dockerfile
2. **WebSocket Bridge**: Create a WebSocket connection between Appwrite Functions and C++ engine
3. **API Gateway**: Use Appwrite Functions as an API gateway

## Security Considerations

1. **API Keys**: Store all API keys in Appwrite environment variables
2. **Function Permissions**: Limit function execution to authenticated users
3. **Database Security**: Configure proper read/write permissions
4. **CORS**: Configure CORS settings for dashboard access

## Troubleshooting

### Common Issues

1. **Function Timeout**: Increase timeout in appwrite.json
2. **Memory Issues**: Optimize function code or increase memory limit
3. **Database Permissions**: Check collection permissions
4. **CORS Errors**: Configure CORS in project settings

### Debug Commands

```bash
# Check function status
appwrite functions list

# View function logs
appwrite logs --function-id market-data-api --limit 100

# Test function execution
appwrite functions create-execution --function-id market-data-api --data '{}'
```

## Production Checklist

- [ ] API keys configured
- [ ] Database permissions set
- [ ] Function timeouts optimized
- [ ] CORS configured
- [ ] Monitoring alerts set up
- [ ] Backup strategy implemented
- [ ] Security review completed

## Next Steps

1. **Real-time Updates**: Implement WebSocket connections
2. **Advanced Analytics**: Add more sophisticated metrics
3. **Alert System**: Set up email/SMS alerts for opportunities
4. **Mobile App**: Create mobile dashboard
5. **Machine Learning**: Add ML-based opportunity scoring
