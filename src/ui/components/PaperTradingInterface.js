import React, { useState, useEffect } from 'react';
import { 
  Box, Typography, Paper, Grid, Card, CardContent, Divider, 
  Button, TextField, Switch, FormControlLabel, TableContainer, 
  Table, TableHead, TableRow, TableCell, TableBody, Chip,
  Tooltip, IconButton, Dialog, DialogTitle, DialogContent,
  DialogActions, CircularProgress, Slider, Alert
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import StopIcon from '@mui/icons-material/Stop';
import RefreshIcon from '@mui/icons-material/Refresh';
import SettingsIcon from '@mui/icons-material/Settings';
import HistoryIcon from '@mui/icons-material/History';
import InfoIcon from '@mui/icons-material/Info';
import TimelineIcon from '@mui/icons-material/Timeline';
import { 
  ResponsiveContainer, LineChart, Line, AreaChart, Area,
  XAxis, YAxis, CartesianGrid, Tooltip as RechartsTooltip, Legend
} from 'recharts';

// Theme with dark mode for trading UI
const darkTheme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#2196f3',
    },
    secondary: {
      main: '#f50057',
    },
    background: {
      paper: '#1e1e1e',
      default: '#121212',
    },
  },
});

// Format currency
const formatCurrency = (value) => {
  return new Intl.NumberFormat('en-US', {
    style: 'currency',
    currency: 'USD',
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  }).format(value);
};

// Format percentage
const formatPercentage = (value) => {
  return new Intl.NumberFormat('en-US', {
    style: 'percent',
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  }).format(value);
};

// Format timestamp
const formatTimestamp = (timestamp) => {
  const date = new Date(timestamp);
  return date.toLocaleString();
};

// Get color based on status
const getStatusColor = (status) => {
  switch (status) {
    case 'completed': return '#4caf50';
    case 'in_progress': return '#2196f3';
    case 'pending': return '#ff9800';
    case 'failed': return '#f44336';
    default: return '#9e9e9e';
  }
};

// Get color based on profit/loss
const getProfitLossColor = (value) => {
  if (value > 0) return '#4caf50';
  if (value < 0) return '#f44336';
  return '#9e9e9e';
};

const PaperTradingInterface = () => {
  // Paper trading state
  const [isSimulationActive, setSimulationActive] = useState(false);
  const [simulationSpeed, setSimulationSpeed] = useState(1); // 1x speed
  const [paperTradingAccount, setPaperTradingAccount] = useState({
    balance: 100000,
    startingBalance: 100000,
    totalPnL: 0,
    openPositions: 0,
    totalTrades: 0,
    winRate: 0,
    sharpeRatio: 0
  });
  
  // Simulation settings
  const [settings, setSettings] = useState({
    maxPositionSize: 10000,
    maxPositions: 5,
    slippageModel: 'realistic', // 'none', 'simple', 'realistic'
    autoExecute: true,
    riskLevel: 'medium', // 'low', 'medium', 'high'
    takeProfit: 0.02, // 2%
    stopLoss: 0.01 // 1%
  });
  
  // UI state
  const [settingsDialogOpen, setSettingsDialogOpen] = useState(false);
  const [activeTrades, setActiveTrades] = useState([]);
  const [tradeHistory, setTradeHistory] = useState([]);
  const [simulationMetrics, setSimulationMetrics] = useState([]);
  const [alerts, setAlerts] = useState([]);
  
  // Fetch initial data
  useEffect(() => {
    const fetchPaperTradingData = async () => {
      try {
        // Fetch paper trading account data
        const accountResponse = await fetch('/api/paper-trading/account');
        const accountData = await accountResponse.json();
        setPaperTradingAccount(accountData);
        
        // Fetch active trades
        const activeTradesResponse = await fetch('/api/paper-trading/active-trades');
        const activeTradesData = await activeTradesResponse.json();
        setActiveTrades(activeTradesData.trades);
        
        // Fetch trade history
        const historyResponse = await fetch('/api/paper-trading/history');
        const historyData = await historyResponse.json();
        setTradeHistory(historyData.trades);
        
        // Fetch simulation metrics
        const metricsResponse = await fetch('/api/paper-trading/metrics');
        const metricsData = await metricsResponse.json();
        setSimulationMetrics(metricsData.metrics);
      } catch (error) {
        console.error('Error fetching paper trading data:', error);
        setAlerts([...alerts, {
          severity: 'error',
          message: 'Failed to load paper trading data. Please try again.',
          timestamp: new Date()
        }]);
      }
    };
    
    fetchPaperTradingData();
    
    // Set up WebSocket for real-time updates
    const ws = new WebSocket('ws://localhost:8080/ws/paper-trading');
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      
      if (data.type === 'account_update') {
        setPaperTradingAccount(data.account);
      } else if (data.type === 'trade_update') {
        setActiveTrades(data.active_trades);
        setTradeHistory(data.trade_history);
      } else if (data.type === 'metrics_update') {
        setSimulationMetrics(prevMetrics => [...prevMetrics, data.metrics]);
      } else if (data.type === 'alert') {
        setAlerts(prevAlerts => [
          ...prevAlerts,
          {
            severity: data.severity,
            message: data.message,
            timestamp: new Date()
          }
        ]);
      }
    };
    
    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, []);
  
  // Start/stop simulation
  const toggleSimulation = async () => {
    try {
      const response = await fetch('/api/paper-trading/simulation', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          action: isSimulationActive ? 'stop' : 'start',
          settings: settings
        })
      });
      
      const data = await response.json();
      
      if (data.success) {
        setSimulationActive(!isSimulationActive);
        setAlerts([...alerts, {
          severity: 'success',
          message: `Simulation ${isSimulationActive ? 'stopped' : 'started'} successfully.`,
          timestamp: new Date()
        }]);
      } else {
        setAlerts([...alerts, {
          severity: 'error',
          message: `Failed to ${isSimulationActive ? 'stop' : 'start'} simulation: ${data.error}`,
          timestamp: new Date()
        }]);
      }
    } catch (error) {
      console.error('Error toggling simulation:', error);
      setAlerts([...alerts, {
        severity: 'error',
        message: `Failed to ${isSimulationActive ? 'stop' : 'start'} simulation.`,
        timestamp: new Date()
      }]);
    }
  };
  
  // Reset simulation
  const resetSimulation = async () => {
    if (isSimulationActive) {
      setAlerts([...alerts, {
        severity: 'warning',
        message: 'Please stop the simulation before resetting.',
        timestamp: new Date()
      }]);
      return;
    }
    
    try {
      const response = await fetch('/api/paper-trading/simulation', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          action: 'reset'
        })
      });
      
      const data = await response.json();
      
      if (data.success) {
        setPaperTradingAccount({
          balance: 100000,
          startingBalance: 100000,
          totalPnL: 0,
          openPositions: 0,
          totalTrades: 0,
          winRate: 0,
          sharpeRatio: 0
        });
        setActiveTrades([]);
        setTradeHistory([]);
        setSimulationMetrics([]);
        setAlerts([...alerts, {
          severity: 'success',
          message: 'Simulation reset successfully.',
          timestamp: new Date()
        }]);
      } else {
        setAlerts([...alerts, {
          severity: 'error',
          message: `Failed to reset simulation: ${data.error}`,
          timestamp: new Date()
        }]);
      }
    } catch (error) {
      console.error('Error resetting simulation:', error);
      setAlerts([...alerts, {
        severity: 'error',
        message: 'Failed to reset simulation.',
        timestamp: new Date()
      }]);
    }
  };
  
  // Update simulation speed
  const handleSpeedChange = async (event, newValue) => {
    setSimulationSpeed(newValue);
    
    if (isSimulationActive) {
      try {
        await fetch('/api/paper-trading/simulation/speed', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            speed: newValue
          })
        });
      } catch (error) {
        console.error('Error updating simulation speed:', error);
      }
    }
  };
  
  // Update settings
  const handleSettingsChange = (setting, value) => {
    setSettings({
      ...settings,
      [setting]: value
    });
  };
  
  // Save settings
  const saveSettings = async () => {
    try {
      const response = await fetch('/api/paper-trading/settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
      });
      
      const data = await response.json();
      
      if (data.success) {
        setSettingsDialogOpen(false);
        setAlerts([...alerts, {
          severity: 'success',
          message: 'Settings saved successfully.',
          timestamp: new Date()
        }]);
      } else {
        setAlerts([...alerts, {
          severity: 'error',
          message: `Failed to save settings: ${data.error}`,
          timestamp: new Date()
        }]);
      }
    } catch (error) {
      console.error('Error saving settings:', error);
      setAlerts([...alerts, {
        severity: 'error',
        message: 'Failed to save settings.',
        timestamp: new Date()
      }]);
    }
  };
  
  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ p: 2 }}>
        <Paper sx={{ p: 3, mb: 3 }}>
          <Grid container spacing={2} alignItems="center">
            <Grid item xs={12} md={6}>
              <Typography variant="h4" gutterBottom>
                Paper Trading Simulation
              </Typography>
              <Typography variant="body1" color="text.secondary">
                Test arbitrage strategies with realistic market simulation
              </Typography>
            </Grid>
            <Grid item xs={12} md={6} sx={{ display: 'flex', justifyContent: 'flex-end' }}>
              <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
                <Button
                  variant="contained"
                  color={isSimulationActive ? "error" : "success"}
                  startIcon={isSimulationActive ? <StopIcon /> : <PlayArrowIcon />}
                  onClick={toggleSimulation}
                >
                  {isSimulationActive ? "Stop Simulation" : "Start Simulation"}
                </Button>
                <Button
                  variant="outlined"
                  startIcon={<RefreshIcon />}
                  onClick={resetSimulation}
                  disabled={isSimulationActive}
                >
                  Reset
                </Button>
                <IconButton onClick={() => setSettingsDialogOpen(true)}>
                  <SettingsIcon />
                </IconButton>
              </Box>
            </Grid>
          </Grid>
          
          {isSimulationActive && (
            <Box sx={{ mt: 2, display: 'flex', alignItems: 'center', gap: 2 }}>
              <Typography variant="body2">Simulation Speed:</Typography>
              <Slider
                value={simulationSpeed}
                onChange={handleSpeedChange}
                step={0.5}
                marks
                min={0.5}
                max={5}
                valueLabelDisplay="auto"
                valueLabelFormat={x => `${x}x`}
                sx={{ width: 200 }}
              />
              <CircularProgress size={20} />
              <Typography variant="body2" color="primary">
                Simulation Running
              </Typography>
            </Box>
          )}
        </Paper>
        
        {/* Alerts */}
        <Box sx={{ mb: 3 }}>
          {alerts.slice(-3).map((alert, index) => (
            <Alert 
              key={index} 
              severity={alert.severity}
              sx={{ mb: 1 }}
              onClose={() => setAlerts(alerts.filter((_, i) => i !== index))}
            >
              {alert.message}
            </Alert>
          ))}
        </Box>
        
        {/* Account Summary */}
        <Grid container spacing={2} sx={{ mb: 3 }}>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Account Balance
                </Typography>
                <Typography variant="h4">
                  {formatCurrency(paperTradingAccount.balance)}
                </Typography>
                <Typography 
                  variant="body2" 
                  sx={{ 
                    color: getProfitLossColor(paperTradingAccount.totalPnL),
                    display: 'flex',
                    alignItems: 'center',
                    gap: 0.5
                  }}
                >
                  {paperTradingAccount.totalPnL > 0 ? <TrendingUpIcon fontSize="small" /> : <TrendingDownIcon fontSize="small" />}
                  {formatCurrency(paperTradingAccount.totalPnL)} ({formatPercentage(paperTradingAccount.totalPnL / paperTradingAccount.startingBalance)})
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Open Positions
                </Typography>
                <Typography variant="h4">
                  {paperTradingAccount.openPositions}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Total Trades: {paperTradingAccount.totalTrades}
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Win Rate
                </Typography>
                <Typography variant="h4">
                  {formatPercentage(paperTradingAccount.winRate)}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Historical Success Rate
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Sharpe Ratio
                </Typography>
                <Typography variant="h4">
                  {paperTradingAccount.sharpeRatio.toFixed(2)}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Risk-Adjusted Performance
                </Typography>
              </CardContent>
            </Card>
          </Grid>
        </Grid>
        
        {/* Active Trades */}
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h5" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <TimelineIcon /> Active Paper Trades
          </Typography>
          <TableContainer>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>ID</TableCell>
                  <TableCell>Strategy</TableCell>
                  <TableCell>Instruments</TableCell>
                  <TableCell>Size</TableCell>
                  <TableCell>Entry Time</TableCell>
                  <TableCell>Duration</TableCell>
                  <TableCell>Current P&L</TableCell>
                  <TableCell>Status</TableCell>
                  <TableCell>Actions</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {activeTrades.length > 0 ? (
                  activeTrades.map((trade) => (
                    <TableRow key={trade.id}>
                      <TableCell>{trade.id}</TableCell>
                      <TableCell>{trade.strategy}</TableCell>
                      <TableCell>
                        {trade.legs.map((leg, i) => (
                          <Chip 
                            key={i}
                            label={`${leg.action} ${leg.symbol}`}
                            size="small"
                            sx={{ m: 0.25 }}
                          />
                        ))}
                      </TableCell>
                      <TableCell>{formatCurrency(trade.size)}</TableCell>
                      <TableCell>{formatTimestamp(trade.entryTime)}</TableCell>
                      <TableCell>{Math.floor((Date.now() - new Date(trade.entryTime).getTime()) / 60000)} min</TableCell>
                      <TableCell sx={{ color: getProfitLossColor(trade.currentPnL) }}>
                        {formatCurrency(trade.currentPnL)} ({formatPercentage(trade.currentPnL / trade.size)})
                      </TableCell>
                      <TableCell>
                        <Chip 
                          label={trade.status}
                          size="small"
                          sx={{ backgroundColor: getStatusColor(trade.status) }}
                        />
                      </TableCell>
                      <TableCell>
                        <Button 
                          variant="outlined" 
                          size="small"
                          color="error"
                          onClick={() => {
                            // Close position logic
                          }}
                        >
                          Close
                        </Button>
                      </TableCell>
                    </TableRow>
                  ))
                ) : (
                  <TableRow>
                    <TableCell colSpan={9} align="center">
                      No active trades
                    </TableCell>
                  </TableRow>
                )}
              </TableBody>
            </Table>
          </TableContainer>
        </Paper>
        
        {/* Performance Metrics Chart */}
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h5" gutterBottom>
            Simulation Performance
          </Typography>
          <Box sx={{ height: 300 }}>
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart
                data={simulationMetrics}
                margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
              >
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis 
                  dataKey="timestamp" 
                  tickFormatter={(tick) => new Date(tick).toLocaleTimeString()} 
                />
                <YAxis />
                <RechartsTooltip 
                  formatter={(value, name) => {
                    if (name === 'balance') return formatCurrency(value);
                    return value;
                  }}
                  labelFormatter={(label) => formatTimestamp(label)}
                />
                <Legend />
                <Area 
                  type="monotone" 
                  dataKey="balance" 
                  name="Account Balance" 
                  stroke="#8884d8" 
                  fill="#8884d8" 
                  fillOpacity={0.3} 
                />
                <Area 
                  type="monotone" 
                  dataKey="unrealizedPnL" 
                  name="Unrealized P&L" 
                  stroke="#82ca9d" 
                  fill="#82ca9d" 
                  fillOpacity={0.3} 
                />
              </AreaChart>
            </ResponsiveContainer>
          </Box>
        </Paper>
        
        {/* Trade History */}
        <Paper sx={{ p: 3 }}>
          <Typography variant="h5" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <HistoryIcon /> Trade History
          </Typography>
          <TableContainer>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>ID</TableCell>
                  <TableCell>Strategy</TableCell>
                  <TableCell>Instruments</TableCell>
                  <TableCell>Size</TableCell>
                  <TableCell>Entry Time</TableCell>
                  <TableCell>Exit Time</TableCell>
                  <TableCell>Duration</TableCell>
                  <TableCell>P&L</TableCell>
                  <TableCell>Result</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {tradeHistory.length > 0 ? (
                  tradeHistory.map((trade) => (
                    <TableRow key={trade.id}>
                      <TableCell>{trade.id}</TableCell>
                      <TableCell>{trade.strategy}</TableCell>
                      <TableCell>
                        {trade.legs.map((leg, i) => (
                          <Chip 
                            key={i}
                            label={`${leg.action} ${leg.symbol}`}
                            size="small"
                            sx={{ m: 0.25 }}
                          />
                        ))}
                      </TableCell>
                      <TableCell>{formatCurrency(trade.size)}</TableCell>
                      <TableCell>{formatTimestamp(trade.entryTime)}</TableCell>
                      <TableCell>{formatTimestamp(trade.exitTime)}</TableCell>
                      <TableCell>
                        {Math.floor((new Date(trade.exitTime).getTime() - new Date(trade.entryTime).getTime()) / 60000)} min
                      </TableCell>
                      <TableCell sx={{ color: getProfitLossColor(trade.finalPnL) }}>
                        {formatCurrency(trade.finalPnL)} ({formatPercentage(trade.finalPnL / trade.size)})
                      </TableCell>
                      <TableCell>
                        <Chip 
                          label={trade.finalPnL > 0 ? "Profit" : "Loss"}
                          size="small"
                          sx={{ 
                            backgroundColor: getProfitLossColor(trade.finalPnL),
                            color: 'white'
                          }}
                        />
                      </TableCell>
                    </TableRow>
                  ))
                ) : (
                  <TableRow>
                    <TableCell colSpan={9} align="center">
                      No trade history
                    </TableCell>
                  </TableRow>
                )}
              </TableBody>
            </Table>
          </TableContainer>
        </Paper>
        
        {/* Settings Dialog */}
        <Dialog 
          open={settingsDialogOpen} 
          onClose={() => setSettingsDialogOpen(false)}
          maxWidth="md"
          fullWidth
        >
          <DialogTitle>Paper Trading Settings</DialogTitle>
          <DialogContent>
            <Grid container spacing={3} sx={{ mt: 1 }}>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Maximum Position Size"
                  type="number"
                  fullWidth
                  value={settings.maxPositionSize}
                  onChange={(e) => handleSettingsChange('maxPositionSize', parseFloat(e.target.value))}
                  InputProps={{
                    startAdornment: <Typography variant="body2">$</Typography>,
                  }}
                />
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Maximum Open Positions"
                  type="number"
                  fullWidth
                  value={settings.maxPositions}
                  onChange={(e) => handleSettingsChange('maxPositions', parseInt(e.target.value))}
                />
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Slippage Model"
                  select
                  fullWidth
                  value={settings.slippageModel}
                  onChange={(e) => handleSettingsChange('slippageModel', e.target.value)}
                >
                  <option value="none">None</option>
                  <option value="simple">Simple</option>
                  <option value="realistic">Realistic</option>
                </TextField>
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Risk Level"
                  select
                  fullWidth
                  value={settings.riskLevel}
                  onChange={(e) => handleSettingsChange('riskLevel', e.target.value)}
                >
                  <option value="low">Low</option>
                  <option value="medium">Medium</option>
                  <option value="high">High</option>
                </TextField>
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Take Profit (%)"
                  type="number"
                  fullWidth
                  value={settings.takeProfit * 100}
                  onChange={(e) => handleSettingsChange('takeProfit', parseFloat(e.target.value) / 100)}
                  InputProps={{
                    endAdornment: <Typography variant="body2">%</Typography>,
                  }}
                />
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  label="Stop Loss (%)"
                  type="number"
                  fullWidth
                  value={settings.stopLoss * 100}
                  onChange={(e) => handleSettingsChange('stopLoss', parseFloat(e.target.value) / 100)}
                  InputProps={{
                    endAdornment: <Typography variant="body2">%</Typography>,
                  }}
                />
              </Grid>
              <Grid item xs={12}>
                <FormControlLabel
                  control={
                    <Switch
                      checked={settings.autoExecute}
                      onChange={(e) => handleSettingsChange('autoExecute', e.target.checked)}
                    />
                  }
                  label="Auto-Execute Trades"
                />
              </Grid>
            </Grid>
          </DialogContent>
          <DialogActions>
            <Button onClick={() => setSettingsDialogOpen(false)}>Cancel</Button>
            <Button onClick={saveSettings} variant="contained">Save Settings</Button>
          </DialogActions>
        </Dialog>
      </Box>
    </ThemeProvider>
  );
};

export default PaperTradingInterface;
