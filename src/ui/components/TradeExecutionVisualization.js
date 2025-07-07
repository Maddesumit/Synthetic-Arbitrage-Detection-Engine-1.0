import React, { useState, useEffect } from 'react';
import { 
  Box, Typography, Paper, Grid, Card, CardContent, Divider, 
  Stepper, Step, StepLabel, StepContent, Button, TableContainer, 
  Table, TableHead, TableRow, TableCell, TableBody, Chip,
  CircularProgress, LinearProgress, Tooltip, IconButton
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import AssignmentTurnedInIcon from '@mui/icons-material/AssignmentTurnedIn';
import WarningIcon from '@mui/icons-material/Warning';
import InfoIcon from '@mui/icons-material/Info';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import StopIcon from '@mui/icons-material/Stop';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import ErrorIcon from '@mui/icons-material/Error';
import TimelineIcon from '@mui/icons-material/Timeline';
import NetworkCheckIcon from '@mui/icons-material/NetworkCheck';
import { 
  ResponsiveContainer, LineChart, Line, BarChart, Bar,
  XAxis, YAxis, CartesianGrid, Tooltip as RechartsTooltip, Legend, 
  AreaChart, Area
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

// Format timestamp with milliseconds
const formatTimestampPrecise = (timestamp) => {
  const date = new Date(timestamp);
  return `${date.toLocaleTimeString()}.${String(date.getMilliseconds()).padStart(3, '0')}`;
};

// Get color based on status
const getStatusColor = (status) => {
  switch (status) {
    case 'completed': return '#4caf50';
    case 'in_progress': return '#2196f3';
    case 'pending': return '#ff9800';
    case 'failed': return '#f44336';
    case 'partial': return '#ff9800';
    default: return '#9e9e9e';
  }
};

// Get color based on latency
const getLatencyColor = (latency) => {
  if (latency < 50) return '#4caf50'; // Good
  if (latency < 200) return '#ffeb3b'; // Acceptable
  if (latency < 500) return '#ff9800'; // Concerning
  return '#f44336'; // Poor
};

// Get color based on slippage
const getSlippageColor = (slippage) => {
  if (slippage < 0.0005) return '#4caf50'; // Minimal
  if (slippage < 0.001) return '#8bc34a'; // Low
  if (slippage < 0.002) return '#ffeb3b'; // Medium
  if (slippage < 0.005) return '#ff9800'; // High
  return '#f44336'; // Excessive
};

const TradeExecutionVisualization = ({ opportunityId, executionPlanId }) => {
  // State for execution data
  const [execution, setExecution] = useState(null);
  const [executionSteps, setExecutionSteps] = useState([]);
  const [marketImpact, setMarketImpact] = useState([]);
  const [executionMetrics, setExecutionMetrics] = useState({
    averageLatency: 0,
    totalSlippage: 0,
    executionDuration: 0,
    totalFees: 0,
    profitImpact: 0
  });
  const [activeStep, setActiveStep] = useState(0);
  const [isLive, setIsLive] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  
  // Fetch execution data
  useEffect(() => {
    const fetchExecutionData = async () => {
      try {
        setIsLoading(true);
        
        // Fetch execution details
        const response = await fetch(`/api/execution-metrics/${executionPlanId}`);
        const data = await response.json();
        
        setExecution(data.execution);
        setExecutionSteps(data.steps);
        setMarketImpact(data.marketImpact);
        setExecutionMetrics(data.metrics);
        setActiveStep(data.execution.status === 'completed' ? data.steps.length : data.currentStep);
        setIsLive(data.execution.status === 'in_progress');
        setIsLoading(false);
      } catch (error) {
        console.error('Error fetching execution data:', error);
        setIsLoading(false);
      }
    };
    
    fetchExecutionData();
    
    // Set up WebSocket connection for real-time updates if execution is live
    const ws = new WebSocket('ws://localhost:8080/ws/execution');
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      
      if (data.executionPlanId === executionPlanId) {
        setExecution(data.execution);
        setExecutionSteps(data.steps);
        setMarketImpact(data.marketImpact);
        setExecutionMetrics(data.metrics);
        setActiveStep(data.currentStep);
        setIsLive(data.execution.status === 'in_progress');
      }
    };
    
    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, [executionPlanId]);
  
  if (isLoading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '400px' }}>
        <CircularProgress />
        <Typography variant="h6" sx={{ ml: 2 }}>
          Loading Execution Data...
        </Typography>
      </Box>
    );
  }
  
  if (!execution) {
    return (
      <Box sx={{ p: 3 }}>
        <Typography variant="h5" color="error">
          Execution data not found for ID: {executionPlanId}
        </Typography>
      </Box>
    );
  }
  
  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ p: 2 }}>
        <Paper sx={{ p: 3, mb: 3 }}>
          <Grid container spacing={2} alignItems="center">
            <Grid item xs={12} md={6}>
              <Typography variant="h4" gutterBottom>
                Trade Execution Visualization
              </Typography>
              <Typography variant="body1" color="text.secondary">
                Real-time trade execution monitoring and analytics
              </Typography>
            </Grid>
            <Grid item xs={12} md={6} sx={{ display: 'flex', justifyContent: 'flex-end' }}>
              <Box sx={{ display: 'flex', alignItems: 'center' }}>
                <Chip 
                  label={execution.status}
                  icon={
                    execution.status === 'completed' ? <CheckCircleIcon /> :
                    execution.status === 'in_progress' ? <TimelineIcon /> :
                    execution.status === 'failed' ? <ErrorIcon /> :
                    <InfoIcon />
                  }
                  color={
                    execution.status === 'completed' ? 'success' :
                    execution.status === 'in_progress' ? 'primary' :
                    execution.status === 'failed' ? 'error' :
                    'default'
                  }
                  sx={{ mr: 2 }}
                />
                {isLive && (
                  <Box sx={{ display: 'flex', alignItems: 'center' }}>
                    <CircularProgress size={20} sx={{ mr: 1 }} />
                    <Typography variant="body2" color="primary">
                      Live Execution
                    </Typography>
                  </Box>
                )}
              </Box>
            </Grid>
          </Grid>
          
          {isLive && (
            <LinearProgress sx={{ mt: 2 }} variant="determinate" value={(activeStep / executionSteps.length) * 100} />
          )}
        </Paper>
        
        {/* Execution Overview */}
        <Grid container spacing={2} sx={{ mb: 3 }}>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Execution Time
                </Typography>
                <Typography variant="h4">
                  {(executionMetrics.executionDuration / 1000).toFixed(2)}s
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Total Duration
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Average Latency
                </Typography>
                <Typography variant="h4" sx={{ color: getLatencyColor(executionMetrics.averageLatency) }}>
                  {executionMetrics.averageLatency.toFixed(2)}ms
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Per Transaction
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Price Slippage
                </Typography>
                <Typography variant="h4" sx={{ color: getSlippageColor(executionMetrics.totalSlippage) }}>
                  {formatPercentage(executionMetrics.totalSlippage)}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Impact: {formatCurrency(executionMetrics.profitImpact)}
                </Typography>
              </CardContent>
            </Card>
          </Grid>
          <Grid item xs={12} md={3}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Total Fees
                </Typography>
                <Typography variant="h4">
                  {formatCurrency(executionMetrics.totalFees)}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  Transaction Costs
                </Typography>
              </CardContent>
            </Card>
          </Grid>
        </Grid>
        
        {/* Execution Timeline */}
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h5" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <TimelineIcon /> Execution Timeline
          </Typography>
          <Stepper activeStep={activeStep} orientation="vertical">
            {executionSteps.map((step, index) => (
              <Step key={index}>
                <StepLabel
                  StepIconProps={{
                    style: { color: getStatusColor(step.status) }
                  }}
                >
                  <Typography variant="subtitle1">
                    {step.description}
                  </Typography>
                  <Typography variant="caption" color="text.secondary">
                    {step.completedAt ? formatTimestampPrecise(step.completedAt) : 'Pending'}
                    {step.latency && ` (${step.latency.toFixed(2)}ms)`}
                  </Typography>
                </StepLabel>
                <StepContent>
                  <Box sx={{ mb: 2 }}>
                    <Typography variant="body2">
                      {step.details}
                    </Typography>
                    
                    {step.warnings && step.warnings.length > 0 && (
                      <Box sx={{ mt: 1, p: 1, bgcolor: 'rgba(255, 152, 0, 0.1)', borderRadius: 1 }}>
                        {step.warnings.map((warning, i) => (
                          <Typography 
                            key={i} 
                            variant="body2" 
                            sx={{ 
                              color: '#ff9800',
                              display: 'flex',
                              alignItems: 'center',
                              gap: 0.5
                            }}
                          >
                            <WarningIcon fontSize="small" />
                            {warning}
                          </Typography>
                        ))}
                      </Box>
                    )}
                    
                    {step.metrics && (
                      <Grid container spacing={2} sx={{ mt: 1 }}>
                        {Object.entries(step.metrics).map(([key, value], i) => (
                          <Grid item xs={6} sm={4} md={3} key={i}>
                            <Tooltip title={key}>
                              <Chip 
                                label={`${key}: ${typeof value === 'number' ? 
                                  (key.includes('price') || key.includes('cost') ? 
                                    formatCurrency(value) : 
                                    (key.includes('percentage') || key.includes('slippage') ? 
                                      formatPercentage(value) : 
                                      value.toFixed(2))) : 
                                  value}`}
                                size="small"
                                sx={{ mb: 1 }}
                              />
                            </Tooltip>
                          </Grid>
                        ))}
                      </Grid>
                    )}
                  </Box>
                </StepContent>
              </Step>
            ))}
          </Stepper>
        </Paper>
        
        {/* Market Impact Analysis */}
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h5" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <NetworkCheckIcon /> Market Impact Analysis
          </Typography>
          <Box sx={{ height: 300, mt: 2 }}>
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart
                data={marketImpact}
                margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
              >
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis 
                  dataKey="timestamp" 
                  tickFormatter={(tick) => formatTimestampPrecise(tick).split('.')[0]} 
                />
                <YAxis yAxisId="left" />
                <YAxis yAxisId="right" orientation="right" />
                <RechartsTooltip 
                  formatter={(value, name) => {
                    if (name === 'price') return formatCurrency(value);
                    if (name === 'slippage') return formatPercentage(value);
                    if (name === 'volume') return value.toFixed(2);
                    return value;
                  }}
                  labelFormatter={(label) => formatTimestampPrecise(label)}
                />
                <Legend />
                <Area 
                  yAxisId="left"
                  type="monotone" 
                  dataKey="price" 
                  name="Execution Price" 
                  stroke="#8884d8" 
                  fill="#8884d8" 
                  fillOpacity={0.3} 
                />
                <Area 
                  yAxisId="left"
                  type="monotone" 
                  dataKey="expectedPrice" 
                  name="Expected Price" 
                  stroke="#82ca9d" 
                  fill="#82ca9d" 
                  fillOpacity={0.3} 
                />
                <Line 
                  yAxisId="right"
                  type="monotone" 
                  dataKey="slippage" 
                  name="Slippage %" 
                  stroke="#ff7300" 
                />
              </AreaChart>
            </ResponsiveContainer>
          </Box>
        </Paper>
        
        {/* Latency & Performance Analysis */}
        <Paper sx={{ p: 3 }}>
          <Typography variant="h5" gutterBottom>
            Latency & Performance Analysis
          </Typography>
          <Box sx={{ height: 300, mt: 2 }}>
            <ResponsiveContainer width="100%" height="100%">
              <BarChart
                data={executionSteps.filter(step => step.latency)}
                margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
              >
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis 
                  dataKey="description" 
                  tick={{ fontSize: 12 }}
                  interval={0}
                  angle={-45}
                  textAnchor="end"
                  height={80}
                />
                <YAxis 
                  label={{ value: 'Latency (ms)', angle: -90, position: 'insideLeft' }}
                />
                <RechartsTooltip 
                  formatter={(value, name) => {
                    return [`${value.toFixed(2)} ms`, 'Latency'];
                  }}
                />
                <Bar 
                  dataKey="latency" 
                  name="Execution Latency" 
                  fill="#2196f3"
                  isAnimationActive={!isLive}
                />
              </BarChart>
            </ResponsiveContainer>
          </Box>
          
          {/* Performance Recommendations */}
          {execution.recommendations && execution.recommendations.length > 0 && (
            <Box sx={{ mt: 3 }}>
              <Typography variant="h6" gutterBottom>
                Performance Recommendations
              </Typography>
              <Grid container spacing={2}>
                {execution.recommendations.map((rec, index) => (
                  <Grid item xs={12} sm={6} md={4} key={index}>
                    <Card variant="outlined">
                      <CardContent>
                        <Typography variant="subtitle1" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          <InfoIcon color="primary" />
                          {rec.title}
                        </Typography>
                        <Typography variant="body2">
                          {rec.description}
                        </Typography>
                        <Box sx={{ mt: 1 }}>
                          <Chip 
                            label={rec.impact}
                            size="small"
                            color={
                              rec.impact === 'High' ? 'error' :
                              rec.impact === 'Medium' ? 'warning' :
                              'success'
                            }
                          />
                        </Box>
                      </CardContent>
                    </Card>
                  </Grid>
                ))}
              </Grid>
            </Box>
          )}
        </Paper>
      </Box>
    </ThemeProvider>
  );
};

export default TradeExecutionVisualization;
