# Phase 6 UI Components - Integration Guide

This document provides integration instructions for the Phase 6 (Arbitrage Ranking & Execution Logic) UI components of the Synthetic Arbitrage Detection Engine.

## Component Overview

Phase 6 includes the following UI components:

1. **OpportunityRanking.js**: Real-time ranking and filtering of arbitrage opportunities
2. **ExecutionPlanner.js**: Detailed planning interface for optimizing arbitrage execution
3. **PnLTracker.js**: Comprehensive P&L tracking and attribution dashboard
4. **PaperTradingInterface.js**: Simulated trading environment for strategy validation
5. **TradeExecutionVisualization.js**: Real-time visualization of trade execution
6. **Phase6Dashboard.js**: Main dashboard integrating all Phase 6 components

## API Endpoints

The UI components require the following API endpoints:

### Opportunity Ranking Endpoints
- `GET /api/opportunity-ranking`: Returns ranked arbitrage opportunities
- `GET /api/opportunity-ranking/{id}`: Returns details for a specific opportunity
- `WebSocket ws://localhost:8080/ws/opportunities`: Real-time opportunity updates

### Execution Planning Endpoints
- `GET /api/execution-plan/{opportunityId}`: Returns optimal execution plan for an opportunity
- `POST /api/execution-plan`: Creates a new execution plan
- `GET /api/market-depth/{exchange}/{symbol}`: Returns market depth data for slippage estimation

### P&L Tracking Endpoints
- `GET /api/pnl-tracking/summary`: Returns P&L summary statistics
- `GET /api/pnl-tracking/historical`: Returns historical P&L data
- `GET /api/pnl-tracking/attribution`: Returns P&L attribution data
- `GET /api/pnl-tracking/trades`: Returns recent trade history
- `WebSocket ws://localhost:8080/ws/pnl`: Real-time P&L updates

### Paper Trading Endpoints
- `GET /api/paper-trading/account`: Returns paper trading account data
- `GET /api/paper-trading/active-trades`: Returns active paper trades
- `GET /api/paper-trading/history`: Returns historical paper trades
- `GET /api/paper-trading/metrics`: Returns simulation metrics
- `POST /api/paper-trading/simulation`: Controls the simulation (start/stop/reset)
- `POST /api/paper-trading/settings`: Updates simulation settings
- `WebSocket ws://localhost:8080/ws/paper-trading`: Real-time paper trading updates

### Execution Visualization Endpoints
- `GET /api/execution-metrics/{executionPlanId}`: Returns execution metrics for a plan
- `WebSocket ws://localhost:8080/ws/execution`: Real-time execution updates

## Integration Instructions

### 1. Add Components to Your Project

All Phase 6 UI components are located in `/src/ui/components/`. Make sure you have the following dependencies in your project:

```json
{
  "dependencies": {
    "@emotion/react": "^11.11.0",
    "@emotion/styled": "^11.11.0",
    "@mui/icons-material": "^5.14.0",
    "@mui/material": "^5.14.0",
    "react": "^18.2.0",
    "react-beautiful-dnd": "^13.1.1",
    "react-dom": "^18.2.0",
    "recharts": "^2.7.2"
  }
}
```

### 2. Backend Integration

The UI components expect JSON responses from the API endpoints with specific structures. Here's an example of the expected response format for `/api/opportunity-ranking`:

```json
{
  "opportunities": [
    {
      "id": "opp-1234",
      "underlying_symbol": "BTCUSDT",
      "expected_profit_pct": 0.0025,
      "required_capital": 10000.0,
      "risk_score": 0.15,
      "confidence": 0.85,
      "detected_at": "2023-08-01T15:30:45.123Z",
      "exchange": "cross_exchange",
      "instrument_type": "spot_perpetual",
      "legs": [
        {
          "symbol": "BTCUSDT",
          "type": "SPOT",
          "exchange": "BINANCE",
          "price": 45000.0,
          "synthetic_price": 45100.0,
          "deviation": -0.0022,
          "action": "BUY"
        },
        {
          "symbol": "BTCUSDT",
          "type": "SPOT",
          "exchange": "OKX",
          "price": 45100.0,
          "synthetic_price": 45000.0,
          "deviation": 0.0022,
          "action": "SELL"
        }
      ]
    }
  ],
  "total_count": 25,
  "timestamp": "2023-08-01T15:31:00.456Z"
}
```

Connect your backend implementation of the `ArbitrageOpportunity`, `ExecutionPlanner`, and `PnLTracker` classes to the corresponding API endpoints.

### 3. WebSocket Integration

For real-time updates, the UI components connect to WebSocket endpoints. Implement the WebSocket server with the following event types:

- `opportunity_update`: New or updated arbitrage opportunities
- `execution_update`: Execution plan status changes
- `pnl_update`: Real-time P&L data updates
- `paper_trading_update`: Paper trading simulation updates

### 4. Using the Phase6Dashboard

The `Phase6Dashboard.js` component integrates all Phase 6 UI components into a single dashboard. To use it in your application:

```jsx
import React from 'react';
import { render } from 'react-dom';
import Phase6Dashboard from './src/ui/components/Phase6Dashboard';

render(<Phase6Dashboard />, document.getElementById('root'));
```

### 5. Customization

The UI components use Material-UI's theming system with a dark theme optimized for trading applications. You can customize the theme by modifying the `darkTheme` object in each component or by providing a custom theme:

```jsx
import { ThemeProvider, createTheme } from '@mui/material/styles';
import Phase6Dashboard from './src/ui/components/Phase6Dashboard';

const customTheme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#00bcd4', // Custom primary color
    },
    // Additional customizations...
  },
});

const App = () => (
  <ThemeProvider theme={customTheme}>
    <Phase6Dashboard />
  </ThemeProvider>
);
```

## Component Features

### OpportunityRanking.js
- Real-time opportunity table with sorting and filtering
- Multi-factor ranking system (profit, risk, confidence)
- Customizable filters for exchange, instrument type, profit, etc.
- Action buttons for execution planning and simulation
- WebSocket integration for live updates

### ExecutionPlanner.js
- Drag-and-drop interface for leg sequencing
- Order sizing controls with slippage estimation
- Market depth visualization
- Timing strategy selection
- Execution cost optimization

### PnLTracker.js
- Real-time P&L dashboard with summary cards
- Historical and cumulative P&L charts
- Realized/unrealized P&L breakdown
- Attribution analysis by instrument, exchange, and strategy
- Recent trades table with performance metrics

### PaperTradingInterface.js
- Simulation controls (start, stop, reset)
- Configurable simulation settings
- Real-time account balance and position tracking
- Trade history and performance metrics
- Simulated execution with realistic market conditions

### TradeExecutionVisualization.js
- Step-by-step execution visualization
- Latency and slippage monitoring
- Market impact analysis
- Performance recommendations
- Real-time execution updates

## Error Handling

All components include comprehensive error handling for API requests and WebSocket connections. Errors are displayed to the user as appropriate.

## Mobile Responsiveness

The UI components are designed to be responsive across different screen sizes, with optimized layouts for desktop, tablet, and mobile devices.

## Browser Compatibility

The UI components are compatible with modern browsers (Chrome, Firefox, Safari, Edge) that support ES6 features and WebSockets.

## Performance Considerations

For optimal performance, consider implementing the following:

1. Server-side filtering and pagination for large datasets
2. WebSocket message batching for high-frequency updates
3. Client-side data caching for frequently accessed information
4. Memoization of expensive calculations and renders

## Security Considerations

When implementing the backend APIs, ensure you:

1. Implement proper authentication and authorization
2. Validate all input data
3. Apply rate limiting to prevent abuse
4. Use HTTPS for all API communications
5. Secure WebSocket connections with appropriate authentication

## Troubleshooting

If you experience issues with the UI components:

1. Check browser console for JavaScript errors
2. Verify API endpoints are returning the expected data format
3. Ensure WebSocket connections are established correctly
4. Validate dependencies are installed at the correct versions
5. Review network requests for any failed API calls
