import React, { useState, useEffect } from 'react';
import { 
  Box, Typography, Paper, Stepper, Step, StepLabel, StepContent, 
  Button, Table, TableBody, TableCell, TableContainer, TableHead, 
  TableRow, Card, CardContent, Chip, Divider, Grid, Slider, 
  LinearProgress, IconButton, Tooltip, Snackbar, Alert
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import AssessmentIcon from '@mui/icons-material/Assessment';
import TimelineIcon from '@mui/icons-material/Timeline';
import LiquidityIcon from '@mui/icons-material/Waves';
import TimerIcon from '@mui/icons-material/Timer';
import WarningIcon from '@mui/icons-material/Warning';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import SwapVertIcon from '@mui/icons-material/SwapVert';
import { DragDropContext, Droppable, Draggable } from 'react-beautiful-dnd';
import { ResponsiveContainer, LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip as RechartsTooltip } from 'recharts';

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

// Mock data for market depth visualization
const generateMarketDepthData = (price, depth) => {
  const data = [];
  const basePrice = price;
  
  // Generate bid data (buyers)
  for (let i = 0; i < 10; i++) {
    const priceDelta = i * 0.005 * basePrice;
    const volume = depth * (1 - i * 0.09);
    data.push({
      price: basePrice - priceDelta,
      volume: volume,
      type: 'bid'
    });
  }
  
  // Generate ask data (sellers)
  for (let i = 0; i < 10; i++) {
    const priceDelta = i * 0.005 * basePrice;
    const volume = depth * (1 - i * 0.09);
    data.push({
      price: basePrice + priceDelta,
      volume: volume,
      type: 'ask'
    });
  }
  
  return data.sort((a, b) => a.price - b.price);
};

// Format large numbers
const formatNumber = (num) => {
  return new Intl.NumberFormat('en-US', {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  }).format(num);
};

// Function to get color based on slippage impact
const getSlippageColor = (slippage) => {
  if (slippage < 0.0005) return '#4caf50'; // Low impact
  if (slippage < 0.001) return '#8bc34a'; // Low-medium impact
  if (slippage < 0.002) return '#ffeb3b'; // Medium impact
  if (slippage < 0.004) return '#ff9800'; // High-medium impact
  return '#f44336'; // High impact
};

const ExecutionPlanner = ({ opportunityId, onExecutionPlanCreated }) => {
  const [opportunity, setOpportunity] = useState(null);
  const [executionPlan, setExecutionPlan] = useState([]);
  const [orderSizes, setOrderSizes] = useState({});
  const [marketDepth, setMarketDepth] = useState({});
  const [slippageEstimates, setSlippageEstimates] = useState({});
  const [timingStrategy, setTimingStrategy] = useState('sequential');
  const [isLoading, setIsLoading] = useState(true);
  const [activeStep, setActiveStep] = useState(0);
  
  // New state for backend connectivity
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [executionPlanId, setExecutionPlanId] = useState(null);
  const [error, setError] = useState(null);
  const [showAlert, setShowAlert] = useState(false);
  const [alertMessage, setAlertMessage] = useState('');
  const [alertSeverity, setAlertSeverity] = useState('info');
  const [marketDataRefreshTimestamp, setMarketDataRefreshTimestamp] = useState(Date.now());
  
  // API base URL - in a real app, this would be in a config or env variable
  const API_BASE_URL = '/api';
  
  useEffect(() => {
    const fetchOpportunityData = async () => {
      try {
        setIsLoading(true);
        setError(null);
        
        // Fetch opportunity details
        const oppResponse = await fetch(`${API_BASE_URL}/opportunity-ranking/${opportunityId}`);
        if (!oppResponse.ok) {
          throw new Error(`Failed to fetch opportunity data: ${oppResponse.status} ${oppResponse.statusText}`);
        }
        const oppData = await oppResponse.json();
        setOpportunity(oppData);
        
        // Fetch execution plan
        const planResponse = await fetch(`${API_BASE_URL}/execution-plan/${opportunityId}`);
        if (!planResponse.ok) {
          throw new Error(`Failed to fetch execution plan: ${planResponse.status} ${planResponse.statusText}`);
        }
        const planData = await planResponse.json();
        setExecutionPlan(planData.legs);
        
        // Initialize order sizes based on recommended values
        const initialOrderSizes = {};
        planData.legs.forEach(leg => {
          initialOrderSizes[leg.id] = leg.recommended_size;
        });
        setOrderSizes(initialOrderSizes);
        
        // Fetch market depth for each leg
        const depthData = {};
        const slippageData = {};
        
        for (const leg of planData.legs) {
          try {
            const marketDepthResponse = await fetch(
              `${API_BASE_URL}/market-depth/${leg.exchange}/${leg.symbol}`
            );
            
            if (marketDepthResponse.ok) {
              const marketDepthData = await marketDepthResponse.json();
              depthData[leg.id] = marketDepthData.depth;
              slippageData[leg.id] = marketDepthData.estimated_slippage || leg.estimated_slippage;
            } else {
              // Fallback to mock data if API fails
              console.warn(`Using mock market depth data for ${leg.symbol} on ${leg.exchange}`);
              depthData[leg.id] = generateMarketDepthData(leg.price, leg.available_liquidity);
              slippageData[leg.id] = leg.estimated_slippage;
            }
          } catch (depthError) {
            console.error(`Error fetching market depth for ${leg.symbol}:`, depthError);
            depthData[leg.id] = generateMarketDepthData(leg.price, leg.available_liquidity);
            slippageData[leg.id] = leg.estimated_slippage;
          }
        }
        
        setMarketDepth(depthData);
        setSlippageEstimates(slippageData);
        
        setIsLoading(false);
      } catch (error) {
        console.error('Error fetching execution planning data:', error);
        setError(error.message);
        setIsLoading(false);
        showAlertMessage(error.message, 'error');
      }
    };
    
    fetchOpportunityData();
    
    // Set up WebSocket for real-time market depth updates
    const ws = new WebSocket('ws://localhost:8080/ws/market-depth');
    
    ws.onopen = () => {
      if (opportunityId) {
        ws.send(JSON.stringify({ type: 'subscribe', opportunityId }));
      }
    };
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      
      if (data.type === 'market_depth_update' && data.opportunityId === opportunityId) {
        setMarketDepth(prevDepth => ({
          ...prevDepth,
          [data.legId]: data.depth
        }));
        
        setSlippageEstimates(prevSlippage => ({
          ...prevSlippage,
          [data.legId]: data.estimated_slippage
        }));
        
        setMarketDataRefreshTimestamp(Date.now());
      }
    };
    
    // Clean up WebSocket connection
    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, [opportunityId]);
  
  // Function to periodically refresh market depth data
  useEffect(() => {
    const refreshInterval = setInterval(async () => {
      if (!isLoading && executionPlan.length > 0) {
        try {
          for (const leg of executionPlan) {
            try {
              const marketDepthResponse = await fetch(
                `${API_BASE_URL}/market-depth/${leg.exchange}/${leg.symbol}`
              );
              
              if (marketDepthResponse.ok) {
                const marketDepthData = await marketDepthResponse.json();
                
                setMarketDepth(prevDepth => ({
                  ...prevDepth,
                  [leg.id]: marketDepthData.depth
                }));
                
                setSlippageEstimates(prevSlippage => ({
                  ...prevSlippage,
                  [leg.id]: marketDepthData.estimated_slippage || prevSlippage[leg.id]
                }));
              }
            } catch (depthError) {
              console.warn(`Error refreshing market depth for ${leg.symbol}:`, depthError);
            }
          }
          setMarketDataRefreshTimestamp(Date.now());
        } catch (error) {
          console.error('Error refreshing market depth data:', error);
        }
      }
    }, 30000); // Refresh every 30 seconds
    
    return () => clearInterval(refreshInterval);
  }, [executionPlan, isLoading]);
  
  // Show alert message helper
  const showAlertMessage = (message, severity = 'info') => {
    setAlertMessage(message);
    setAlertSeverity(severity);
    setShowAlert(true);
  };
  
  // Handle drag-and-drop reordering of execution steps
  const handleDragEnd = (result) => {
    if (!result.destination) return;
    
    const items = Array.from(executionPlan);
    const [reorderedItem] = items.splice(result.source.index, 1);
    items.splice(result.destination.index, 0, reorderedItem);
    
    setExecutionPlan(items);
  };
  
  // Update order size for a leg
  const handleOrderSizeChange = (legId, newSize) => {
    setOrderSizes({
      ...orderSizes,
      [legId]: newSize
    });
    
    // Update slippage estimates based on the new order size
    updateSlippageEstimate(legId, newSize);
  };
  
  // Calculate and update slippage estimate based on order size
  const updateSlippageEstimate = async (legId, newSize) => {
    const leg = executionPlan.find(leg => leg.id === legId);
    if (!leg) return;
    
    try {
      // Try to get slippage estimate from the backend
      const response = await fetch(`${API_BASE_URL}/slippage-estimate`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          exchange: leg.exchange,
          symbol: leg.symbol,
          action: leg.action,
          size: newSize
        })
      });
      
      if (response.ok) {
        const data = await response.json();
        setSlippageEstimates(prev => ({
          ...prev,
          [legId]: data.estimated_slippage
        }));
      } else {
        // Fallback to client-side estimation if API fails
        fallbackSlippageCalculation(legId, newSize);
      }
    } catch (error) {
      console.warn('Error fetching slippage estimate, using fallback calculation:', error);
      fallbackSlippageCalculation(legId, newSize);
    }
  };
  
  // Fallback slippage calculation when API is unavailable
  const fallbackSlippageCalculation = (legId, newSize) => {
    const leg = executionPlan.find(leg => leg.id === legId);
    if (leg) {
      const sizeFactor = newSize / leg.recommended_size;
      const newSlippage = leg.base_slippage * (1 + (sizeFactor - 1) * 1.5);
      
      setSlippageEstimates(prev => ({
        ...prev,
        [legId]: newSlippage
      }));
    }
  };
  
  // Handle timing strategy change
  const handleTimingStrategyChange = (strategy) => {
    setTimingStrategy(strategy);
  };
  
  // Next/back step handlers
  const handleNext = () => {
    setActiveStep((prevActiveStep) => prevActiveStep + 1);
  };
  
  const handleBack = () => {
    setActiveStep((prevActiveStep) => prevActiveStep - 1);
  };
  
  const handleReset = () => {
    setActiveStep(0);
  };
  
  // Calculate total slippage impact
  const calculateTotalSlippage = () => {
    return Object.values(slippageEstimates).reduce((sum, slippage) => sum + slippage, 0);
  };
  
  // Calculate profit after slippage
  const calculateNetProfit = () => {
    if (!opportunity) return 0;
    
    const totalSlippage = calculateTotalSlippage();
    return opportunity.expected_profit_pct - totalSlippage;
  };
  
  // Check if net profit is still positive
  const isProfitPositive = () => {
    return calculateNetProfit() > 0;
  };
  
  // Submit execution plan to backend
  const submitExecutionPlan = async () => {
    if (!opportunity || !isProfitPositive()) {
      showAlertMessage('Cannot submit execution plan with negative profit expectation', 'error');
      return;
    }
    
    try {
      setIsSubmitting(true);
      
      const planData = {
        opportunityId: opportunityId,
        timingStrategy: timingStrategy,
        executionSequence: executionPlan.map((leg, index) => ({
          legId: leg.id,
          sequenceOrder: index,
          orderSize: orderSizes[leg.id] || leg.recommended_size,
          estimatedSlippage: slippageEstimates[leg.id]
        })),
        totalExpectedProfit: calculateNetProfit(),
        totalSlippage: calculateTotalSlippage(),
        timestamp: new Date().toISOString()
      };
      
      const response = await fetch(`${API_BASE_URL}/execution-plan`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(planData)
      });
      
      if (!response.ok) {
        throw new Error(`Failed to submit execution plan: ${response.status} ${response.statusText}`);
      }
      
      const result = await response.json();
      setExecutionPlanId(result.executionPlanId);
      
      showAlertMessage('Execution plan created successfully!', 'success');
      
      // If callback provided, notify parent component of the created plan
      if (onExecutionPlanCreated && typeof onExecutionPlanCreated === 'function') {
        onExecutionPlanCreated(result.executionPlanId);
      }
      
    } catch (error) {
      console.error('Error submitting execution plan:', error);
      setError(error.message);
      showAlertMessage(`Failed to submit execution plan: ${error.message}`, 'error');
    } finally {
      setIsSubmitting(false);
    }
  };
  
  // Execute the plan immediately
  const executeNow = async () => {
    if (!executionPlanId) {
      try {
        // Submit the plan first if not already submitted
        await submitExecutionPlan();
      } catch (error) {
        return; // Early return if submission fails
      }
    }
    
    try {
      setIsSubmitting(true);
      
      const response = await fetch(`${API_BASE_URL}/execute-plan/${executionPlanId || 'latest'}`, {
        method: 'POST'
      });
      
      if (!response.ok) {
        throw new Error(`Failed to execute plan: ${response.status} ${response.statusText}`);
      }
      
      const result = await response.json();
      showAlertMessage('Execution initiated successfully!', 'success');
      
      // If callback provided, notify parent component
      if (onExecutionPlanCreated && typeof onExecutionPlanCreated === 'function') {
        onExecutionPlanCreated(result.executionPlanId || executionPlanId);
      }
      
    } catch (error) {
      console.error('Error executing plan:', error);
      setError(error.message);
      showAlertMessage(`Failed to execute plan: ${error.message}`, 'error');
    } finally {
      setIsSubmitting(false);
    }
  };
  
  if (isLoading) {
    return (
      <Box sx={{ padding: 2 }}>
        <Typography variant="h6">Loading execution plan...</Typography>
        <LinearProgress sx={{ mt: 2 }} />
      </Box>
    );
  }
  
  if (error && !opportunity) {
    return (
      <Box sx={{ padding: 2 }}>
        <Alert severity="error">
          <Typography variant="h6">Error loading data</Typography>
          <Typography variant="body2">{error}</Typography>
        </Alert>
      </Box>
    );
  }
  
  if (!opportunity) {
    return (
      <Box sx={{ padding: 2 }}>
        <Typography variant="h6">Opportunity not found</Typography>
      </Box>
    );
  }
  
  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ padding: 2, backgroundColor: '#121212', borderRadius: 1, minHeight: '100vh' }}>
        <Typography variant="h5" component="h1" sx={{ mb: 3, display: 'flex', alignItems: 'center' }}>
          <AssessmentIcon sx={{ mr: 1 }} /> 
          Execution Planning: {opportunity.underlying_symbol}
        </Typography>
        
        {/* Market data freshness indicator */}
        <Box sx={{ mb: 2, display: 'flex', alignItems: 'center' }}>
          <Typography variant="caption" color="text.secondary">
            Market data as of: {new Date(marketDataRefreshTimestamp).toLocaleTimeString()}
          </Typography>
          {Date.now() - marketDataRefreshTimestamp > 60000 && (
            <Chip 
              size="small" 
              color="warning" 
              label="Data may be stale" 
              sx={{ ml: 1 }}
            />
          )}
        </Box>
        
        {/* Summary Card */}
        <Card sx={{ mb: 3 }}>
          <CardContent>
            <Grid container spacing={2}>
              <Grid item xs={12} md={4}>
                <Typography variant="subtitle2" color="text.secondary">
                  Expected Profit
                </Typography>
                <Typography variant="h6" color="success.main">
                  {(opportunity.expected_profit_pct * 100).toFixed(4)}%
                </Typography>
              </Grid>
              
              <Grid item xs={12} md={4}>
                <Typography variant="subtitle2" color="text.secondary">
                  Required Capital
                </Typography>
                <Typography variant="h6">
                  ${opportunity.required_capital.toLocaleString()}
                </Typography>
              </Grid>
              
              <Grid item xs={12} md={4}>
                <Typography variant="subtitle2" color="text.secondary">
                  Risk Score
                </Typography>
                <Typography variant="h6" color={
                  opportunity.risk_score < 0.2 ? "success.main" : 
                  opportunity.risk_score < 0.4 ? "warning.main" : "error.main"
                }>
                  {(opportunity.risk_score * 100).toFixed(2)}%
                </Typography>
              </Grid>
            </Grid>
          </CardContent>
        </Card>
        
        <Grid container spacing={3}>
          {/* Left Panel - Execution Steps */}
          <Grid item xs={12} md={7}>
            <Paper sx={{ p: 2, mb: 3 }}>
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <TimelineIcon sx={{ mr: 1 }} /> Execution Sequence
              </Typography>
              
              <Typography variant="body2" color="text.secondary" paragraph>
                Drag and drop to reorder execution steps for optimal sequencing.
                The recommended order minimizes market impact and execution risk.
              </Typography>
              
              <DragDropContext onDragEnd={handleDragEnd}>
                <Droppable droppableId="execution-steps">
                  {(provided) => (
                    <Box
                      {...provided.droppableProps}
                      ref={provided.innerRef}
                    >
                      <Stepper orientation="vertical" activeStep={activeStep}>
                        {executionPlan.map((leg, index) => (
                          <Draggable key={leg.id} draggableId={leg.id} index={index}>
                            {(provided) => (
                              <Step 
                                ref={provided.innerRef}
                                {...provided.draggableProps}
                                {...provided.dragHandleProps}
                              >
                                <StepLabel StepIconComponent={() => (
                                  <Box sx={{ display: 'flex', alignItems: 'center' }}>
                                    <IconButton size="small" sx={{ mr: 1, cursor: 'grab' }}>
                                      <SwapVertIcon fontSize="small" />
                                    </IconButton>
                                    <Chip 
                                      label={leg.action} 
                                      color={leg.action === 'BUY' ? 'success' : 'error'} 
                                      size="small" 
                                    />
                                  </Box>
                                )}>
                                  <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                    <Typography>
                                      {leg.symbol} @ {leg.exchange}
                                    </Typography>
                                    
                                    <Tooltip title="Estimated slippage impact">
                                      <Chip
                                        size="small"
                                        label={`Slippage: ${(slippageEstimates[leg.id] * 100).toFixed(4)}%`}
                                        sx={{ 
                                          backgroundColor: getSlippageColor(slippageEstimates[leg.id]),
                                          color: '#000'
                                        }}
                                      />
                                    </Tooltip>
                                  </Box>
                                </StepLabel>
                                <StepContent>
                                  <Box sx={{ mb: 2 }}>
                                    <Typography variant="body2" paragraph>
                                      {leg.type} on {leg.exchange} at approximately ${leg.price.toFixed(2)}
                                    </Typography>
                                    
                                    <Typography variant="subtitle2" gutterBottom>
                                      Order Size:
                                    </Typography>
                                    
                                    <Box sx={{ px: 2, py: 1 }}>
                                      <Grid container spacing={2} alignItems="center">
                                        <Grid item xs>
                                          <Slider
                                            value={orderSizes[leg.id] || leg.recommended_size}
                                            min={leg.recommended_size * 0.5}
                                            max={leg.recommended_size * 1.5}
                                            step={leg.recommended_size * 0.01}
                                            onChange={(e, newValue) => handleOrderSizeChange(leg.id, newValue)}
                                            aria-labelledby={`order-size-slider-${leg.id}`}
                                          />
                                        </Grid>
                                        <Grid item>
                                          <Typography>
                                            {formatNumber(orderSizes[leg.id] || leg.recommended_size)} {leg.symbol.split('/')[0]}
                                          </Typography>
                                        </Grid>
                                      </Grid>
                                    </Box>
                                    
                                    <Typography variant="subtitle2" gutterBottom sx={{ mt: 1 }}>
                                      Market Depth:
                                    </Typography>
                                    
                                    <Box sx={{ height: 150 }}>
                                      <ResponsiveContainer width="100%" height="100%">
                                        <LineChart
                                          data={marketDepth[leg.id] || []}
                                          margin={{ top: 5, right: 5, left: 5, bottom: 5 }}
                                        >
                                          <CartesianGrid strokeDasharray="3 3" />
                                          <XAxis 
                                            dataKey="price"
                                            domain={['dataMin', 'dataMax']}
                                            tickFormatter={(value) => value.toFixed(1)}
                                          />
                                          <YAxis />
                                          <RechartsTooltip />
                                          <Line 
                                            type="monotone" 
                                            dataKey="volume" 
                                            stroke="#8884d8" 
                                            activeDot={{ r: 8 }} 
                                            dot={false}
                                            name="Volume"
                                          />
                                        </LineChart>
                                      </ResponsiveContainer>
                                    </Box>
                                    
                                    <Box sx={{ mt: 2 }}>
                                      <Typography variant="caption" color="text.secondary">
                                        Available Liquidity: {formatNumber(leg.available_liquidity)} {leg.symbol.split('/')[0]}
                                      </Typography>
                                    </Box>
                                  </Box>
                                  
                                  <Box sx={{ mb: 2 }}>
                                    <div>
                                      <Button
                                        variant="contained"
                                        onClick={handleNext}
                                        sx={{ mt: 1, mr: 1 }}
                                      >
                                        {index === executionPlan.length - 1 ? 'Finish' : 'Continue'}
                                      </Button>
                                      <Button
                                        disabled={index === 0}
                                        onClick={handleBack}
                                        sx={{ mt: 1, mr: 1 }}
                                      >
                                        Back
                                      </Button>
                                    </div>
                                  </Box>
                                </StepContent>
                              </Step>
                            )}
                          </Draggable>
                        ))}
                      </Stepper>
                      {provided.placeholder}
                      
                      {activeStep === executionPlan.length && (
                        <Paper square elevation={0} sx={{ p: 3, mt: 3, backgroundColor: '#2c2c2c' }}>
                          <Typography>All steps completed - you're ready to execute</Typography>
                          <Button onClick={handleReset} sx={{ mt: 1, mr: 1 }}>
                            Reset
                          </Button>
                          <Button 
                            variant="contained" 
                            color="primary" 
                            sx={{ mt: 1, mr: 1 }}
                            onClick={() => {/* In a real app, this would submit the execution plan */}}
                          >
                            Execute Plan
                          </Button>
                        </Paper>
                      )}
                    </Box>
                  )}
                </Droppable>
              </DragDropContext>
            </Paper>
          </Grid>
          
          {/* Right Panel - Timing and Impact Analysis */}
          <Grid item xs={12} md={5}>
            <Paper sx={{ p: 2, mb: 3 }}>
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <TimerIcon sx={{ mr: 1 }} /> Execution Timing
              </Typography>
              
              <Box sx={{ mb: 3 }}>
                <Typography variant="body2" gutterBottom>
                  Select execution timing strategy:
                </Typography>
                
                <Box sx={{ display: 'flex', flexDirection: 'column', gap: 1, mt: 1 }}>
                  <Button
                    variant={timingStrategy === 'sequential' ? 'contained' : 'outlined'}
                    onClick={() => handleTimingStrategyChange('sequential')}
                    startIcon={<TimelineIcon />}
                  >
                    Sequential Execution
                  </Button>
                  
                  <Button
                    variant={timingStrategy === 'parallel' ? 'contained' : 'outlined'}
                    onClick={() => handleTimingStrategyChange('parallel')}
                    startIcon={<TimelineIcon />}
                  >
                    Parallel Execution
                  </Button>
                  
                  <Button
                    variant={timingStrategy === 'scheduled' ? 'contained' : 'outlined'}
                    onClick={() => handleTimingStrategyChange('scheduled')}
                    startIcon={<TimelineIcon />}
                  >
                    Scheduled Execution
                  </Button>
                </Box>
              </Box>
              
              <Divider sx={{ my: 2 }} />
              
              <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center' }}>
                <LiquidityIcon sx={{ mr: 1 }} /> Market Impact Analysis
              </Typography>
              
              <TableContainer component={Paper} sx={{ mb: 2, backgroundColor: '#2c2c2c' }}>
                <Table size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell>Leg</TableCell>
                      <TableCell align="right">Order Size</TableCell>
                      <TableCell align="right">Slippage</TableCell>
                      <TableCell align="right">Impact</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {executionPlan.map((leg) => (
                      <TableRow key={leg.id}>
                        <TableCell>
                          {leg.symbol} ({leg.action})
                        </TableCell>
                        <TableCell align="right">
                          {formatNumber(orderSizes[leg.id] || leg.recommended_size)}
                        </TableCell>
                        <TableCell align="right">
                          {(slippageEstimates[leg.id] * 100).toFixed(4)}%
                        </TableCell>
                        <TableCell align="right">
                          <Box sx={{ 
                            width: 16, 
                            height: 16, 
                            borderRadius: '50%', 
                            backgroundColor: getSlippageColor(slippageEstimates[leg.id]),
                            display: 'inline-block'
                          }} />
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </TableContainer>
              
              <Box sx={{ p: 2, backgroundColor: '#2c2c2c', borderRadius: 1 }}>
                <Typography variant="subtitle2" gutterBottom>
                  Total Execution Impact:
                </Typography>
                
                <Grid container spacing={2}>
                  <Grid item xs={6}>
                    <Typography variant="body2" color="text.secondary">
                      Total Slippage:
                    </Typography>
                    <Typography variant="body1" sx={{ color: getSlippageColor(calculateTotalSlippage()) }}>
                      {(calculateTotalSlippage() * 100).toFixed(4)}%
                    </Typography>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Typography variant="body2" color="text.secondary">
                      Net Profit After Slippage:
                    </Typography>
                    <Typography variant="body1" sx={{ color: isProfitPositive() ? 'success.main' : 'error.main' }}>
                      {(calculateNetProfit() * 100).toFixed(4)}%
                    </Typography>
                  </Grid>
                </Grid>
                
                <Box sx={{ mt: 2, display: 'flex', alignItems: 'center' }}>
                  {isProfitPositive() ? (
                    <CheckCircleIcon color="success" sx={{ mr: 1 }} />
                  ) : (
                    <WarningIcon color="error" sx={{ mr: 1 }} />
                  )}
                  <Typography variant="body2" color={isProfitPositive() ? 'success.main' : 'error.main'}>
                    {isProfitPositive() 
                      ? 'Trade remains profitable after slippage' 
                      : 'Warning: Trade may not be profitable after slippage'}
                  </Typography>
                </Box>
              </Box>
              
              <Box sx={{ mt: 3, display: 'flex', flexDirection: 'column', gap: 1 }}>
                <Button 
                  variant="contained" 
                  color="primary" 
                  fullWidth
                  onClick={submitExecutionPlan}
                  disabled={!isProfitPositive() || isSubmitting}
                >
                  {isSubmitting ? 'Submitting...' : (
                    isProfitPositive() 
                      ? 'Create Execution Plan' 
                      : 'Adjust Plan (Not Profitable)'
                  )}
                </Button>
                
                {executionPlanId && (
                  <Button 
                    variant="outlined" 
                    color="secondary" 
                    fullWidth
                    onClick={executeNow}
                    disabled={isSubmitting}
                  >
                    Execute Now
                  </Button>
                )}
              </Box>
            </Paper>
          </Grid>
        </Grid>
        
        {/* Alert Snackbar */}
        <Snackbar 
          open={showAlert} 
          autoHideDuration={6000} 
          onClose={() => setShowAlert(false)}
        >
          <Alert 
            onClose={() => setShowAlert(false)} 
            severity={alertSeverity} 
            variant="filled" 
            sx={{ width: '100%' }}
          >
            {alertMessage}
          </Alert>
        </Snackbar>
      </Box>
    </ThemeProvider>
  );
};

export default ExecutionPlanner;
