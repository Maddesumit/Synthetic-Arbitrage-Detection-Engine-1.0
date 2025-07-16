# Configuration Guide

## Overview

The Synthetic Pair Deviation Engine uses JSON configuration files to manage settings for different environments and components.

## Configuration Files

### `config.json`
The main configuration file containing all settings for the arbitrage engine.

## Configuration Structure

```json
{
  "exchanges": {
    "binance": {
      "enabled": true,
      "api_key": "your_binance_api_key",
      "secret_key": "your_binance_secret_key",
      "base_url": "https://api.binance.com",
      "websocket_url": "wss://stream.binance.com:9443/ws/",
      "rate_limit": 1200,
      "timeout": 10000
    },
    "bybit": {
      "enabled": true,
      "api_key": "your_bybit_api_key",
      "secret_key": "your_bybit_secret_key",
      "base_url": "https://api.bybit.com",
      "websocket_url": "wss://stream.bybit.com/v5/public/linear",
      "rate_limit": 600,
      "timeout": 10000
    },
    "okx": {
      "enabled": true,
      "api_key": "your_okx_api_key",
      "secret_key": "your_okx_secret_key",
      "passphrase": "your_okx_passphrase",
      "base_url": "https://www.okx.com",
      "websocket_url": "wss://ws.okx.com:8443/ws/v5/public",
      "rate_limit": 600,
      "timeout": 10000
    }
  },
  "arbitrage": {
    "min_profit_threshold": 0.001,
    "max_position_size": 1000.0,
    "symbols": ["BTC/USDT", "ETH/USDT", "BNB/USDT"],
    "update_interval": 100,
    "opportunity_timeout": 5000
  },
  "risk_management": {
    "max_exposure": 10000.0,
    "position_limit": 0.1,
    "stop_loss": 0.02,
    "take_profit": 0.01,
    "max_drawdown": 0.05
  },
  "logging": {
    "level": "info",
    "file": "logs/arbitrage.log",
    "console": true,
    "max_file_size": "10MB",
    "max_files": 5
  }
}
```

## Configuration Sections

### 1. Exchanges
Configure API credentials and connection settings for each exchange:
- **enabled**: Whether to use this exchange
- **api_key/secret_key**: API credentials
- **base_url**: REST API endpoint
- **websocket_url**: WebSocket stream endpoint
- **rate_limit**: Requests per minute limit
- **timeout**: Connection timeout in milliseconds

### 2. Arbitrage
Configure arbitrage detection parameters:
- **min_profit_threshold**: Minimum profit percentage to trigger (e.g., 0.001 = 0.1%)
- **max_position_size**: Maximum position size in USDT
- **symbols**: List of trading pairs to monitor
- **update_interval**: Data update frequency in milliseconds
- **opportunity_timeout**: How long to wait for opportunity execution

### 3. Risk Management
Configure risk control parameters:
- **max_exposure**: Maximum total exposure across all positions
- **position_limit**: Maximum position size as percentage of portfolio
- **stop_loss**: Stop loss percentage
- **take_profit**: Take profit percentage
- **max_drawdown**: Maximum allowed drawdown

### 4. Logging
Configure logging behavior:
- **level**: Logging level (trace, debug, info, warn, error)
- **file**: Log file path
- **console**: Whether to log to console
- **max_file_size**: Maximum log file size before rotation
- **max_files**: Number of rotated log files to keep

## Setup Instructions

1. **Copy the template**: Use the provided `config.json` as a starting point
2. **Add API credentials**: Replace placeholder values with your actual API keys
3. **Configure parameters**: Adjust arbitrage and risk parameters to your strategy
4. **Set logging**: Configure logging level and output preferences
5. **Test configuration**: Use the validation tools to ensure settings are correct

## Security Best Practices

1. **Never commit API keys**: Add `config.json` to `.gitignore`
2. **Use environment variables**: Consider using environment variables for sensitive data
3. **Restrict permissions**: Use read-only API keys where possible
4. **Regular rotation**: Rotate API keys periodically
5. **Monitor usage**: Keep track of API usage and unusual activity

## Environment-Specific Configuration

For different environments (development, staging, production):
1. Create separate config files: `config.dev.json`, `config.prod.json`
2. Use environment variables to specify which config to load
3. Override specific settings via command line arguments

## Validation

The engine validates configuration on startup and will report errors for:
- Missing required fields
- Invalid parameter ranges
- Malformed API credentials
- Unreachable exchange endpoints

## Troubleshooting

### Common Issues:
1. **API Key Errors**: Check credentials and permissions
2. **Connection Timeouts**: Increase timeout values or check network
3. **Rate Limiting**: Reduce request frequency or increase limits
4. **Invalid Symbols**: Ensure trading pairs are available on all exchanges

### Debug Mode:
Set logging level to "debug" for detailed troubleshooting information.

---

*Keep your configuration secure and regularly review settings for optimal performance.*
