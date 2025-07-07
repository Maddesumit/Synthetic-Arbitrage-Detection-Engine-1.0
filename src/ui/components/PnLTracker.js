import React, { useState, useEffect } from 'react';
import { 
  Box, Typography, Paper, Grid, Card, CardContent, Divider, 
  TableContainer, Table, TableHead, TableRow, TableCell, TableBody,
  Chip, Tab, Tabs, ButtonGroup, Button, LinearProgress,
  ToggleButton, ToggleButtonGroup
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import AccountBalanceWalletIcon from '@mui/icons-material/AccountBalanceWallet';
import ShowChartIcon from '@mui/icons-material/ShowChart';
import TrendingUpIcon from '@mui/icons-material/TrendingUp';
import TrendingDownIcon from '@mui/icons-material/TrendingDown';
import AssessmentIcon from '@mui/icons-material/Assessment';
import TimelineIcon from '@mui/icons-material/Timeline';
import { 
  ResponsiveContainer, AreaChart, Area, LineChart, Line,
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend,
  PieChart, Pie, Cell
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

// Colors for charts
const COLORS = ['#00C49F', '#FFBB28', '#FF8042', '#0088FE', '#8884d8'];

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

// Get color based on profit/loss
const getProfitLossColor = (value) => {
  if (value > 0) return '#4caf50';
  if (value < 0) return '#f44336';
  return '#ffffff';
};

// Mock data generator for historical P&L
const generateHistoricalPnL = (days = 30) => {
  const data = [];
  let cumulativePnL = 0;
  
  for (let i = 0; i < days; i++) {
    const date = new Date();
    date.setDate(date.getDate() - (days - i - 1));
    
    const dailyPnL = (Math.random() * 2 - 0.8) * 1000; // Random P&L between -800 and 1200
    cumulativePnL += dailyPnL;
    
    data.push({
      date: date.toISOString().split('T')[0],
      dailyPnL: dailyPnL,
      cumulativePnL: cumulativePnL,
      realized: Math.random() > 0.4 ? dailyPnL * 0.7 : 0, // 60% chance of having realized P&L
      unrealized: Math.random() > 0.4 ? dailyPnL * 0.3 : dailyPnL // Remaining is unrealized
    });
  }
  
  return data;
};

// Mock data generator for P&L attribution
const generatePnLAttribution = () => {
  return [
    { name: 'BTC/USDT', value: 4800, percentage: 0.48 },
    { name: 'ETH/USDT', value: 2500, percentage: 0.25 },
    { name: 'SOL/USDT', value: 1500, percentage: 0.15 },
    { name: 'ADA/USDT', value: 700, percentage: 0.07 },
    { name: 'Others', value: 500, percentage: 0.05 }
  ];
};

// Generate exchange attribution data
const generateExchangeAttribution = () => {
  return [
    { name: 'Binance', value: 5200, percentage: 0.52 },
    { name: 'OKX', value: 3000, percentage: 0.3 },
    { name: 'Bybit', value: 1800, percentage: 0.18 }
  ];
};

// Generate strategy attribution data
const generateStrategyAttribution = () => {
  return [
    { name: 'Cross-Exchange', value: 6500, percentage: 0.65 },
    { name: 'Funding Rate', value: 2000, percentage: 0.2 },
    { name: 'Basis Trading', value: 1500, percentage: 0.15 }
  ];
};

// Generate recent trades data
const generateRecentTrades = () => {
  const trades = [];
  const symbols = ['BTC/USDT', 'ETH/USDT', 'SOL/USDT', 'ADA/USDT', 'BNB/USDT'];
  const exchanges = ['Binance', 'OKX', 'Bybit'];
  const strategies = ['Cross-Exchange', 'Funding Rate', 'Basis Trading'];
  
  for (let i = 0; i < 10; i++) {
    const isProfit = Math.random() > 0.3; // 70% chance of profit
    const pnl = (Math.random() * (isProfit ? 500 : 300)) * (isProfit ? 1 : -1);
    
    const date = new Date();
    date.setHours(date.getHours() - Math.floor(Math.random() * 24));
    
    trades.push({
      id: `trade-${i}`,
      timestamp: date.toISOString(),
      symbol: symbols[Math.floor(Math.random() * symbols.length)],
      strategy: strategies[Math.floor(Math.random() * strategies.length)],
      exchanges: [
        exchanges[Math.floor(Math.random() * exchanges.length)],
        exchanges[Math.floor(Math.random() * exchanges.length)]
      ],
      pnl: pnl,
      roi: pnl / (Math.random() * 10000 + 5000)
    });
  }
  
  return trades.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
};

const PnLTracker = () => {
  const [historicalData, setHistoricalData] = useState([]);
  const [attributionData, setAttributionData] = useState({
    byAsset: [],
    byExchange: [],
    byStrategy: []
  });
  const [recentTrades, setRecentTrades] = useState([]);
  const [summary, setSummary] = useState({
    totalPnL: 0,
    realizedPnL: 0,
    unrealizedPnL: 0,
    dailyPnL: 0,
    weeklyPnL: 0,
    monthlyPnL: 0,
    winRate: 0,
    profitFactor: 0,
    averageTrade: 0
  });
  const [isLoading, setIsLoading] = useState(true);
  const [timeRange, setTimeRange] = useState('1M');
  const [attributionTab, setAttributionTab] = useState(0);
  const [chartType, setChartType] = useState('area');
  
  // Fetch P&L data
  useEffect(() => {
    const fetchPnLData = async () => {
      try {
        setIsLoading(true);
        
        // In a real app, you would fetch from your API
        // For demo, we'll use our mock data generators
        const days = timeRange === '1W' ? 7 : timeRange === '1M' ? 30 : 90;
        const historical = generateHistoricalPnL(days);
        
        setHistoricalData(historical);
        
        setAttributionData({
          byAsset: generatePnLAttribution(),
          byExchange: generateExchangeAttribution(),
          byStrategy: generateStrategyAttribution()
        });
        
        setRecentTrades(generateRecentTrades());
        
        // Calculate summary statistics
        const totalPnL = historical[historical.length - 1].cumulativePnL;
        const realizedSum = historical.reduce((sum, day) => sum + day.realized, 0);
        const unrealizedSum = historical.reduce((sum, day) => sum + day.unrealized, 0);
        
        const dailyPnL = historical[historical.length - 1].dailyPnL;
        const weeklyPnL = historical.slice(-7).reduce((sum, day) => sum + day.dailyPnL, 0);
        const monthlyPnL = historical.reduce((sum, day) => sum + day.dailyPnL, 0);
        
        // Calculate win rate from recent trades
        const trades = generateRecentTrades();
        const winningTrades = trades.filter(trade => trade.pnl > 0).length;
        const winRate = winningTrades / trades.length;
        
        // Calculate profit factor (sum of winning trades / sum of losing trades)
        const winningSum = trades.filter(trade => trade.pnl > 0).reduce((sum, trade) => sum + trade.pnl, 0);
        const losingSum = Math.abs(trades.filter(trade => trade.pnl < 0).reduce((sum, trade) => sum + trade.pnl, 0));
        const profitFactor = losingSum === 0 ? winningSum : winningSum / losingSum;
        
        // Calculate average trade P&L
        const averageTrade = trades.reduce((sum, trade) => sum + trade.pnl, 0) / trades.length;
        
        setSummary({
          totalPnL,
          realizedPnL: realizedSum,
          unrealizedPnL: unrealizedSum,
          dailyPnL,
          weeklyPnL,
          monthlyPnL,
          winRate,
          profitFactor,
          averageTrade
        });
        
        setIsLoading(false);
      } catch (error) {
        console.error('Error fetching P&L data:', error);
        setIsLoading(false);
      }
    };
    
    fetchPnLData();
    
    // Set up WebSocket for real-time updates
    const ws = new WebSocket('ws://localhost:8080/ws/pnl');
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      // In a real app, you would update the state with real-time data
      // For demo purposes, we'll just log it
      console.log('Real-time P&L update:', data);
    };
    
    return () => {
      ws.close();
    };
  }, [timeRange]);
  
  const handleTimeRangeChange = (event, newValue) => {
    if (newValue !== null) {
      setTimeRange(newValue);
    }
  };
  
  const handleAttributionTabChange = (event, newValue) => {
    setAttributionTab(newValue);
  };
  
  const handleChartTypeChange = (event, newValue) => {
    if (newValue !== null) {
      setChartType(newValue);
    }
  };
  
  if (isLoading) {
    return (
      <Box sx={{ padding: 2 }}>
        <Typography variant="h6">Loading P&L data...</Typography>
        <LinearProgress sx={{ mt: 2 }} />
      </Box>
    );
  }
  
  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ padding: 2, backgroundColor: '#121212', borderRadius: 1, minHeight: '100vh' }}>
        <Typography variant="h5" component="h1" sx={{ mb: 3, display: 'flex', alignItems: 'center' }}>
          <AccountBalanceWalletIcon sx={{ mr: 1 }} /> 
          P&L Tracking & Performance
        </Typography>
        
        {/* Summary Cards */}
        <Grid container spacing={2} sx={{ mb: 3 }}>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="subtitle2" color="text.secondary">
                  Total P&L
                </Typography>
                <Typography variant="h5" sx={{ color: getProfitLossColor(summary.totalPnL) }}>
                  {formatCurrency(summary.totalPnL)}
                </Typography>
                <Box sx={{ display: 'flex', mt: 1 }}>
                  <Typography variant="body2" sx={{ mr: 2 }}>
                    Realized: <span style={{ color: getProfitLossColor(summary.realizedPnL) }}>
                      {formatCurrency(summary.realizedPnL)}
                    </span>
                  </Typography>
                  <Typography variant="body2">
                    Unrealized: <span style={{ color: getProfitLossColor(summary.unrealizedPnL) }}>
                      {formatCurrency(summary.unrealizedPnL)}
                    </span>
                  </Typography>
                </Box>
              </CardContent>
            </Card>
          </Grid>
          
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="subtitle2" color="text.secondary">
                  Period Performance
                </Typography>
                <Grid container spacing={1}>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Daily</Typography>
                    <Typography variant="body1" sx={{ color: getProfitLossColor(summary.dailyPnL) }}>
                      {formatCurrency(summary.dailyPnL)}
                    </Typography>
                  </Grid>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Weekly</Typography>
                    <Typography variant="body1" sx={{ color: getProfitLossColor(summary.weeklyPnL) }}>
                      {formatCurrency(summary.weeklyPnL)}
                    </Typography>
                  </Grid>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Monthly</Typography>
                    <Typography variant="body1" sx={{ color: getProfitLossColor(summary.monthlyPnL) }}>
                      {formatCurrency(summary.monthlyPnL)}
                    </Typography>
                  </Grid>
                </Grid>
              </CardContent>
            </Card>
          </Grid>
          
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="subtitle2" color="text.secondary">
                  Performance Metrics
                </Typography>
                <Grid container spacing={1}>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Win Rate</Typography>
                    <Typography variant="body1">
                      {formatPercentage(summary.winRate)}
                    </Typography>
                  </Grid>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Profit Factor</Typography>
                    <Typography variant="body1">
                      {summary.profitFactor.toFixed(2)}
                    </Typography>
                  </Grid>
                  <Grid item xs={4}>
                    <Typography variant="caption" color="text.secondary">Avg Trade</Typography>
                    <Typography variant="body1" sx={{ color: getProfitLossColor(summary.averageTrade) }}>
                      {formatCurrency(summary.averageTrade)}
                    </Typography>
                  </Grid>
                </Grid>
              </CardContent>
            </Card>
          </Grid>
          
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent sx={{ height: '100%', display: 'flex', flexDirection: 'column', justifyContent: 'center' }}>
                <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                  P&L Attribution Summary
                </Typography>
                <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <Chip 
                    label="By Asset" 
                    color="primary" 
                    variant={attributionTab === 0 ? "filled" : "outlined"} 
                    onClick={() => setAttributionTab(0)}
                    size="small"
                    sx={{ mr: 1 }}
                  />
                  <Chip 
                    label="By Exchange" 
                    color="primary" 
                    variant={attributionTab === 1 ? "filled" : "outlined"} 
                    onClick={() => setAttributionTab(1)}
                    size="small"
                    sx={{ mr: 1 }}
                  />
                  <Chip 
                    label="By Strategy" 
                    color="primary" 
                    variant={attributionTab === 2 ? "filled" : "outlined"} 
                    onClick={() => setAttributionTab(2)}
                    size="small"
                  />
                </Box>
              </CardContent>
            </Card>
          </Grid>
        </Grid>
        
        {/* Main Content */}
        <Grid container spacing={3}>
          {/* Historical P&L Chart */}
          <Grid item xs={12} md={8}>
            <Paper sx={{ p: 2, mb: 3 }}>
              <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                  <ShowChartIcon sx={{ mr: 1 }} /> Historical P&L
                </Typography>
                
                <Box sx={{ display: 'flex', alignItems: 'center' }}>
                  <ToggleButtonGroup
                    value={chartType}
                    exclusive
                    onChange={handleChartTypeChange}
                    aria-label="chart type"
                    size="small"
                    sx={{ mr: 2 }}
                  >
                    <ToggleButton value="area" aria-label="area chart">
                      <TimelineIcon fontSize="small" />
                    </ToggleButton>
                    <ToggleButton value="bar" aria-label="bar chart">
                      <AssessmentIcon fontSize="small" />
                    </ToggleButton>
                  </ToggleButtonGroup>
                  
                  <ButtonGroup size="small" aria-label="time range">
                    <Button 
                      variant={timeRange === '1W' ? 'contained' : 'outlined'} 
                      onClick={() => setTimeRange('1W')}
                    >
                      1W
                    </Button>
                    <Button 
                      variant={timeRange === '1M' ? 'contained' : 'outlined'} 
                      onClick={() => setTimeRange('1M')}
                    >
                      1M
                    </Button>
                    <Button 
                      variant={timeRange === '3M' ? 'contained' : 'outlined'} 
                      onClick={() => setTimeRange('3M')}
                    >
                      3M
                    </Button>
                  </ButtonGroup>
                </Box>
              </Box>
              
              <Box sx={{ height: 300 }}>
                {chartType === 'area' ? (
                  <ResponsiveContainer width="100%" height="100%">
                    <AreaChart
                      data={historicalData}
                      margin={{ top: 5, right: 30, left: 20, bottom: 5 }}
                    >
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="date" />
                      <YAxis />
                      <Tooltip 
                        formatter={(value) => formatCurrency(value)}
                        labelFormatter={(label) => `Date: ${label}`}
                      />
                      <Legend />
                      <Area 
                        type="monotone" 
                        dataKey="cumulativePnL" 
                        name="Cumulative P&L" 
                        stroke="#8884d8" 
                        fill="#8884d8" 
                        fillOpacity={0.3}
                      />
                      <Area 
                        type="monotone" 
                        dataKey="realized" 
                        name="Realized P&L" 
                        stroke="#4caf50" 
                        fill="#4caf50" 
                        fillOpacity={0.3}
                      />
                      <Area 
                        type="monotone" 
                        dataKey="unrealized" 
                        name="Unrealized P&L" 
                        stroke="#ff9800" 
                        fill="#ff9800" 
                        fillOpacity={0.3}
                      />
                    </AreaChart>
                  </ResponsiveContainer>
                ) : (
                  <ResponsiveContainer width="100%" height="100%">
                    <BarChart
                      data={historicalData}
                      margin={{ top: 5, right: 30, left: 20, bottom: 5 }}
                    >
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="date" />
                      <YAxis />
                      <Tooltip 
                        formatter={(value) => formatCurrency(value)}
                        labelFormatter={(label) => `Date: ${label}`}
                      />
                      <Legend />
                      <Bar 
                        dataKey="dailyPnL" 
                        name="Daily P&L" 
                        fill={(data) => data.dailyPnL >= 0 ? '#4caf50' : '#f44336'}
                      />
                      <Bar 
                        dataKey="realized" 
                        name="Realized P&L" 
                        fill="#4caf50" 
                      />
                      <Bar 
                        dataKey="unrealized" 
                        name="Unrealized P&L" 
                        fill="#ff9800" 
                      />
                    </BarChart>
                  </ResponsiveContainer>
                )}
              </Box>
            </Paper>
            
            {/* Recent Trades */}
            <Paper sx={{ p: 2 }}>
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <AssessmentIcon sx={{ mr: 1 }} /> Recent Trades
              </Typography>
              
              <TableContainer>
                <Table size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell>Time</TableCell>
                      <TableCell>Symbol</TableCell>
                      <TableCell>Strategy</TableCell>
                      <TableCell>Exchanges</TableCell>
                      <TableCell align="right">P&L</TableCell>
                      <TableCell align="right">ROI</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {recentTrades.map((trade) => (
                      <TableRow key={trade.id} hover>
                        <TableCell>
                          {new Date(trade.timestamp).toLocaleTimeString()}
                        </TableCell>
                        <TableCell>{trade.symbol}</TableCell>
                        <TableCell>{trade.strategy}</TableCell>
                        <TableCell>
                          <Box sx={{ display: 'flex', gap: 0.5 }}>
                            {trade.exchanges.map((exchange, i) => (
                              <Chip 
                                key={i} 
                                label={exchange} 
                                size="small" 
                                sx={{ height: 20, fontSize: '0.7rem' }}
                              />
                            ))}
                          </Box>
                        </TableCell>
                        <TableCell align="right">
                          <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end' }}>
                            {trade.pnl > 0 ? (
                              <TrendingUpIcon fontSize="small" sx={{ color: '#4caf50', mr: 0.5 }} />
                            ) : (
                              <TrendingDownIcon fontSize="small" sx={{ color: '#f44336', mr: 0.5 }} />
                            )}
                            <Typography sx={{ color: getProfitLossColor(trade.pnl) }}>
                              {formatCurrency(trade.pnl)}
                            </Typography>
                          </Box>
                        </TableCell>
                        <TableCell align="right">
                          <Typography sx={{ color: getProfitLossColor(trade.roi) }}>
                            {formatPercentage(trade.roi)}
                          </Typography>
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </TableContainer>
            </Paper>
          </Grid>
          
          {/* P&L Attribution */}
          <Grid item xs={12} md={4}>
            <Paper sx={{ p: 2, mb: 3 }}>
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <AssessmentIcon sx={{ mr: 1 }} /> P&L Attribution
              </Typography>
              
              <Tabs 
                value={attributionTab} 
                onChange={handleAttributionTabChange}
                variant="fullWidth"
                sx={{ mb: 2 }}
              >
                <Tab label="By Asset" />
                <Tab label="By Exchange" />
                <Tab label="By Strategy" />
              </Tabs>
              
              <Box sx={{ height: 300 }}>
                <ResponsiveContainer width="100%" height="100%">
                  <PieChart>
                    <Pie
                      data={
                        attributionTab === 0 ? attributionData.byAsset :
                        attributionTab === 1 ? attributionData.byExchange :
                        attributionData.byStrategy
                      }
                      cx="50%"
                      cy="50%"
                      labelLine={false}
                      outerRadius={100}
                      fill="#8884d8"
                      dataKey="value"
                      nameKey="name"
                      label={({ name, percent }) => `${name}: ${(percent * 100).toFixed(0)}%`}
                    >
                      {(attributionTab === 0 ? attributionData.byAsset :
                         attributionTab === 1 ? attributionData.byExchange :
                         attributionData.byStrategy).map((entry, index) => (
                        <Cell key={`cell-${index}`} fill={COLORS[index % COLORS.length]} />
                      ))}
                    </Pie>
                    <Tooltip 
                      formatter={(value) => formatCurrency(value)}
                    />
                  </PieChart>
                </ResponsiveContainer>
              </Box>
              
              <Divider sx={{ my: 2 }} />
              
              <TableContainer>
                <Table size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell>
                        {attributionTab === 0 ? 'Asset' : 
                         attributionTab === 1 ? 'Exchange' : 'Strategy'}
                      </TableCell>
                      <TableCell align="right">P&L</TableCell>
                      <TableCell align="right">%</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {(attributionTab === 0 ? attributionData.byAsset :
                       attributionTab === 1 ? attributionData.byExchange :
                       attributionData.byStrategy).map((item, index) => (
                      <TableRow key={item.name} hover>
                        <TableCell sx={{ display: 'flex', alignItems: 'center' }}>
                          <Box 
                            sx={{ 
                              width: 12, 
                              height: 12, 
                              borderRadius: '50%', 
                              backgroundColor: COLORS[index % COLORS.length],
                              mr: 1
                            }} 
                          />
                          {item.name}
                        </TableCell>
                        <TableCell align="right">
                          {formatCurrency(item.value)}
                        </TableCell>
                        <TableCell align="right">
                          {formatPercentage(item.percentage)}
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </TableContainer>
            </Paper>
            
            {/* Performance Metrics */}
            <Paper sx={{ p: 2 }}>
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <ShowChartIcon sx={{ mr: 1 }} /> Performance Metrics
              </Typography>
              
              <Grid container spacing={2}>
                <Grid item xs={6}>
                  <Card sx={{ bgcolor: '#2c2c2c' }}>
                    <CardContent>
                      <Typography variant="subtitle2" color="text.secondary">Win Rate</Typography>
                      <Typography variant="h6">
                        {formatPercentage(summary.winRate)}
                      </Typography>
                      <LinearProgress 
                        variant="determinate" 
                        value={summary.winRate * 100} 
                        sx={{ mt: 1, height: 8, borderRadius: 4 }}
                        color={summary.winRate > 0.5 ? "success" : "warning"}
                      />
                    </CardContent>
                  </Card>
                </Grid>
                
                <Grid item xs={6}>
                  <Card sx={{ bgcolor: '#2c2c2c' }}>
                    <CardContent>
                      <Typography variant="subtitle2" color="text.secondary">Profit Factor</Typography>
                      <Typography variant="h6">
                        {summary.profitFactor.toFixed(2)}
                      </Typography>
                      <LinearProgress 
                        variant="determinate" 
                        value={Math.min(summary.profitFactor / 3 * 100, 100)} 
                        sx={{ mt: 1, height: 8, borderRadius: 4 }}
                        color={summary.profitFactor > 1.5 ? "success" : "warning"}
                      />
                    </CardContent>
                  </Card>
                </Grid>
                
                <Grid item xs={6}>
                  <Card sx={{ bgcolor: '#2c2c2c' }}>
                    <CardContent>
                      <Typography variant="subtitle2" color="text.secondary">Avg. Trade</Typography>
                      <Typography variant="h6" sx={{ color: getProfitLossColor(summary.averageTrade) }}>
                        {formatCurrency(summary.averageTrade)}
                      </Typography>
                    </CardContent>
                  </Card>
                </Grid>
                
                <Grid item xs={6}>
                  <Card sx={{ bgcolor: '#2c2c2c' }}>
                    <CardContent>
                      <Typography variant="subtitle2" color="text.secondary">Monthly Return</Typography>
                      <Typography variant="h6" sx={{ color: getProfitLossColor(summary.monthlyPnL) }}>
                        {formatPercentage(summary.monthlyPnL / 100000)} {/* Assuming $100k capital */}
                      </Typography>
                    </CardContent>
                  </Card>
                </Grid>
              </Grid>
            </Paper>
          </Grid>
        </Grid>
      </Box>
    </ThemeProvider>
  );
};

export default PnLTracker;
