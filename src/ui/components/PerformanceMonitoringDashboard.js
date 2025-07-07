import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  Box,
  Paper,
  Typography,
  Grid,
  Card,
  CardContent,
  LinearProgress,
  Alert,
  Chip,
  Tooltip,
  IconButton,
  Select,
  MenuItem,
  FormControl,
  InputLabel,
  Switch,
  FormControlLabel,
  Button,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow
} from '@mui/material';
import {
  Speed as SpeedIcon,
  Memory as MemoryIcon,
  NetworkCheck as NetworkIcon,
  Timer as TimerIcon,
  TrendingUp as TrendingUpIcon,
  Warning as WarningIcon,
  Error as ErrorIcon,
  CheckCircle as CheckCircleIcon,
  Settings as SettingsIcon,
  Refresh as RefreshIcon,
  GetApp as DownloadIcon
} from '@mui/icons-material';
import {
  LineChart,
  Line,
  AreaChart,
  Area,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip as RechartsTooltip,
  Legend,
  ResponsiveContainer,
  PieChart,
  Pie,
  Cell
} from 'recharts';

const PerformanceMonitoringDashboard = () => {
  // State management
  const [performanceData, setPerformanceData] = useState({
    systemMetrics: {
      cpuUsage: 0,
      memoryUsage: 0,
      networkUtilization: 0,
      diskUsage: 0
    },
    latencyMetrics: {
      average: 0,
      p50: 0,
      p95: 0,
      p99: 0,
      p999: 0
    },
    throughputMetrics: {
      current: 0,
      peak: 0,
      average: 0,
      target: 2000
    },
    healthStatus: 'HEALTHY',
    activeAlerts: [],
    bottlenecks: [],
    trends: []
  });

  const [historicalData, setHistoricalData] = useState([]);
  const [selectedTimeRange, setSelectedTimeRange] = useState('1h');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5); // seconds
  const [showSettings, setShowSettings] = useState(false);
  const [loading, setLoading] = useState(false);

  // WebSocket connection for real-time updates
  const [ws, setWs] = useState(null);

  // Fetch performance data
  const fetchPerformanceData = useCallback(async () => {
    try {
      setLoading(true);
      
      // Fetch current metrics
      const [
        systemResponse,
        latencyResponse,
        throughputResponse,
        healthResponse,
        alertsResponse,
        bottlenecksResponse
      ] = await Promise.all([
        fetch('/api/performance/system-metrics'),
        fetch('/api/performance/latency-metrics'),
        fetch('/api/performance/throughput-metrics'),
        fetch('/api/performance/health-status'),
        fetch('/api/performance/alerts'),
        fetch('/api/performance/bottlenecks')
      ]);

      const systemData = await systemResponse.json();
      const latencyData = await latencyResponse.json();
      const throughputData = await throughputResponse.json();
      const healthData = await healthResponse.json();
      const alertsData = await alertsResponse.json();
      const bottlenecksData = await bottlenecksResponse.json();

      setPerformanceData({
        systemMetrics: systemData,
        latencyMetrics: latencyData,
        throughputMetrics: throughputData,
        healthStatus: healthData.overall_status,
        activeAlerts: alertsData.alerts || [],
        bottlenecks: bottlenecksData.bottlenecks || [],
        trends: systemData.trends || []
      });

      // Fetch historical data
      const historyResponse = await fetch(`/api/performance/history?timeRange=${selectedTimeRange}`);
      const historyData = await historyResponse.json();
      setHistoricalData(historyData.history || []);

    } catch (error) {
      console.error('Error fetching performance data:', error);
    } finally {
      setLoading(false);
    }
  }, [selectedTimeRange]);

  // Setup WebSocket connection
  useEffect(() => {
    if (autoRefresh) {
      const websocket = new WebSocket('ws://localhost:8080/ws/performance');
      
      websocket.onopen = () => {
        console.log('Performance monitoring WebSocket connected');
        setWs(websocket);
      };

      websocket.onmessage = (event) => {
        const data = JSON.parse(event.data);
        
        if (data.type === 'performance_update') {
          setPerformanceData(prev => ({
            ...prev,
            ...data.metrics
          }));
        } else if (data.type === 'alert') {
          setPerformanceData(prev => ({
            ...prev,
            activeAlerts: [...prev.activeAlerts, data.alert]
          }));
        } else if (data.type === 'bottleneck_detected') {
          setPerformanceData(prev => ({
            ...prev,
            bottlenecks: [...prev.bottlenecks, data.bottleneck]
          }));
        }
      };

      websocket.onclose = () => {
        console.log('Performance monitoring WebSocket disconnected');
        setWs(null);
      };

      websocket.onerror = (error) => {
        console.error('Performance monitoring WebSocket error:', error);
      };

      return () => {
        if (websocket.readyState === WebSocket.OPEN) {
          websocket.close();
        }
      };
    }
  }, [autoRefresh]);

  // Auto-refresh timer
  useEffect(() => {
    if (autoRefresh && !ws) {
      const interval = setInterval(fetchPerformanceData, refreshInterval * 1000);
      return () => clearInterval(interval);
    }
  }, [autoRefresh, refreshInterval, fetchPerformanceData, ws]);

  // Initial data fetch
  useEffect(() => {
    fetchPerformanceData();
  }, [fetchPerformanceData]);

  // Health status color mapping
  const getHealthStatusColor = (status) => {
    switch (status) {
      case 'HEALTHY': return 'success';
      case 'WARNING': return 'warning';
      case 'CRITICAL': return 'error';
      default: return 'default';
    }
  };

  // Metric cards data
  const metricCards = useMemo(() => [
    {
      title: 'CPU Usage',
      value: `${performanceData.systemMetrics.cpuUsage?.toFixed(1)}%`,
      icon: <SpeedIcon />,
      color: performanceData.systemMetrics.cpuUsage > 80 ? 'error' : 
             performanceData.systemMetrics.cpuUsage > 60 ? 'warning' : 'success',
      progress: performanceData.systemMetrics.cpuUsage,
      threshold: 80
    },
    {
      title: 'Memory Usage',
      value: `${performanceData.systemMetrics.memoryUsage?.toFixed(1)}%`,
      icon: <MemoryIcon />,
      color: performanceData.systemMetrics.memoryUsage > 85 ? 'error' : 
             performanceData.systemMetrics.memoryUsage > 70 ? 'warning' : 'success',
      progress: performanceData.systemMetrics.memoryUsage,
      threshold: 85
    },
    {
      title: 'Network Utilization',
      value: `${performanceData.systemMetrics.networkUtilization?.toFixed(1)}%`,
      icon: <NetworkIcon />,
      color: performanceData.systemMetrics.networkUtilization > 90 ? 'error' : 
             performanceData.systemMetrics.networkUtilization > 75 ? 'warning' : 'success',
      progress: performanceData.systemMetrics.networkUtilization,
      threshold: 90
    },
    {
      title: 'Average Latency',
      value: `${performanceData.latencyMetrics.average?.toFixed(2)}ms`,
      icon: <TimerIcon />,
      color: performanceData.latencyMetrics.average > 10 ? 'error' : 
             performanceData.latencyMetrics.average > 5 ? 'warning' : 'success',
      progress: Math.min((performanceData.latencyMetrics.average / 20) * 100, 100),
      threshold: 10
    }
  ], [performanceData]);

  // Latency percentiles chart data
  const latencyData = useMemo(() => [
    { name: 'P50', value: performanceData.latencyMetrics.p50 },
    { name: 'P95', value: performanceData.latencyMetrics.p95 },
    { name: 'P99', value: performanceData.latencyMetrics.p99 },
    { name: 'P99.9', value: performanceData.latencyMetrics.p999 }
  ], [performanceData.latencyMetrics]);

  // Throughput validation
  const throughputValidation = useMemo(() => {
    const { current, target } = performanceData.throughputMetrics;
    const percentage = target > 0 ? (current / target) * 100 : 0;
    
    return {
      percentage,
      status: percentage >= 95 ? 'success' : percentage >= 80 ? 'warning' : 'error',
      message: percentage >= 95 ? 'Target met' : 
               percentage >= 80 ? 'Below target' : 'Critical performance'
    };
  }, [performanceData.throughputMetrics]);

  // Handle manual refresh
  const handleManualRefresh = () => {
    fetchPerformanceData();
  };

  // Handle export report
  const handleExportReport = async () => {
    try {
      const response = await fetch('/api/performance/export-report', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          timeRange: selectedTimeRange,
          includeHistorical: true,
          format: 'pdf'
        })
      });

      if (response.ok) {
        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.style.display = 'none';
        a.href = url;
        a.download = `performance-report-${new Date().toISOString()}.pdf`;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
      }
    } catch (error) {
      console.error('Error exporting report:', error);
    }
  };

  return (
    <Box sx={{ flexGrow: 1, p: 3 }}>
      {/* Header */}
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
        <Typography variant="h4" component="h1">
          Performance Monitoring Dashboard
        </Typography>
        
        <Box sx={{ display: 'flex', gap: 2, alignItems: 'center' }}>
          <Chip
            icon={performanceData.healthStatus === 'HEALTHY' ? <CheckCircleIcon /> : 
                  performanceData.healthStatus === 'WARNING' ? <WarningIcon /> : <ErrorIcon />}
            label={`System ${performanceData.healthStatus}`}
            color={getHealthStatusColor(performanceData.healthStatus)}
            variant="outlined"
          />
          
          <FormControl size="small" sx={{ minWidth: 120 }}>
            <InputLabel>Time Range</InputLabel>
            <Select
              value={selectedTimeRange}
              label="Time Range"
              onChange={(e) => setSelectedTimeRange(e.target.value)}
            >
              <MenuItem value="15m">15 minutes</MenuItem>
              <MenuItem value="1h">1 hour</MenuItem>
              <MenuItem value="4h">4 hours</MenuItem>
              <MenuItem value="24h">24 hours</MenuItem>
              <MenuItem value="7d">7 days</MenuItem>
            </Select>
          </FormControl>

          <Tooltip title="Settings">
            <IconButton onClick={() => setShowSettings(true)}>
              <SettingsIcon />
            </IconButton>
          </Tooltip>

          <Tooltip title="Refresh">
            <IconButton onClick={handleManualRefresh} disabled={loading}>
              <RefreshIcon />
            </IconButton>
          </Tooltip>

          <Tooltip title="Export Report">
            <IconButton onClick={handleExportReport}>
              <DownloadIcon />
            </IconButton>
          </Tooltip>
        </Box>
      </Box>

      {/* Active Alerts */}
      {performanceData.activeAlerts.length > 0 && (
        <Alert severity="warning" sx={{ mb: 3 }}>
          <Typography variant="h6">Active Alerts ({performanceData.activeAlerts.length})</Typography>
          {performanceData.activeAlerts.slice(0, 3).map((alert, index) => (
            <Typography key={index} variant="body2">
              • {alert.title}: {alert.description}
            </Typography>
          ))}
          {performanceData.activeAlerts.length > 3 && (
            <Typography variant="body2">
              + {performanceData.activeAlerts.length - 3} more alerts
            </Typography>
          )}
        </Alert>
      )}

      {/* System Metrics Cards */}
      <Grid container spacing={3} sx={{ mb: 3 }}>
        {metricCards.map((metric, index) => (
          <Grid item xs={12} sm={6} md={3} key={index}>
            <Card>
              <CardContent>
                <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
                  <Box sx={{ color: `${metric.color}.main`, mr: 1 }}>
                    {metric.icon}
                  </Box>
                  <Typography variant="h6" component="div">
                    {metric.title}
                  </Typography>
                </Box>
                
                <Typography variant="h4" sx={{ mb: 2, color: `${metric.color}.main` }}>
                  {metric.value}
                </Typography>
                
                <LinearProgress
                  variant="determinate"
                  value={metric.progress}
                  color={metric.color}
                  sx={{ height: 8, borderRadius: 4 }}
                />
                
                <Typography variant="caption" sx={{ mt: 1, display: 'block' }}>
                  Threshold: {metric.threshold}%
                </Typography>
              </CardContent>
            </Card>
          </Grid>
        ))}
      </Grid>

      {/* Charts Row */}
      <Grid container spacing={3} sx={{ mb: 3 }}>
        {/* Historical Performance Chart */}
        <Grid item xs={12} md={8}>
          <Paper sx={{ p: 2, height: 400 }}>
            <Typography variant="h6" sx={{ mb: 2 }}>
              Performance Trends
            </Typography>
            <ResponsiveContainer width="100%" height="90%">
              <LineChart data={historicalData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis 
                  dataKey="timestamp" 
                  tickFormatter={(value) => new Date(value).toLocaleTimeString()}
                />
                <YAxis />
                <RechartsTooltip 
                  labelFormatter={(value) => new Date(value).toLocaleString()}
                />
                <Legend />
                <Line 
                  type="monotone" 
                  dataKey="cpuUsage" 
                  stroke="#8884d8" 
                  strokeWidth={2}
                  name="CPU Usage (%)"
                />
                <Line 
                  type="monotone" 
                  dataKey="memoryUsage" 
                  stroke="#82ca9d" 
                  strokeWidth={2}
                  name="Memory Usage (%)"
                />
                <Line 
                  type="monotone" 
                  dataKey="latency" 
                  stroke="#ffc658" 
                  strokeWidth={2}
                  name="Latency (ms)"
                />
                <Line 
                  type="monotone" 
                  dataKey="throughput" 
                  stroke="#ff7300" 
                  strokeWidth={2}
                  name="Throughput (ops/sec)"
                />
              </LineChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        {/* Latency Percentiles */}
        <Grid item xs={12} md={4}>
          <Paper sx={{ p: 2, height: 400 }}>
            <Typography variant="h6" sx={{ mb: 2 }}>
              Latency Percentiles
            </Typography>
            <ResponsiveContainer width="100%" height="90%">
              <BarChart data={latencyData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="name" />
                <YAxis />
                <RechartsTooltip formatter={(value) => [`${value.toFixed(2)}ms`, 'Latency']} />
                <Bar dataKey="value" fill="#8884d8" />
              </BarChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>
      </Grid>

      {/* Throughput and Bottlenecks Row */}
      <Grid container spacing={3} sx={{ mb: 3 }}>
        {/* Throughput Validation */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" sx={{ mb: 2 }}>
              Throughput Validation
            </Typography>
            
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
              <Typography variant="body1">
                Current: {performanceData.throughputMetrics.current?.toFixed(0)} ops/sec
              </Typography>
              <Typography variant="body1">
                Target: {performanceData.throughputMetrics.target} ops/sec
              </Typography>
            </Box>
            
            <LinearProgress
              variant="determinate"
              value={Math.min(throughputValidation.percentage, 100)}
              color={throughputValidation.status}
              sx={{ height: 12, borderRadius: 6, mb: 2 }}
            />
            
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <Chip
                label={throughputValidation.message}
                color={throughputValidation.status}
                size="small"
              />
              <Typography variant="caption">
                {throughputValidation.percentage.toFixed(1)}% of target
              </Typography>
            </Box>
            
            <Typography variant="body2" sx={{ mt: 2 }}>
              Peak: {performanceData.throughputMetrics.peak?.toFixed(0)} ops/sec
            </Typography>
          </Paper>
        </Grid>

        {/* Detected Bottlenecks */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" sx={{ mb: 2 }}>
              Detected Bottlenecks
            </Typography>
            
            {performanceData.bottlenecks.length === 0 ? (
              <Typography variant="body2" color="text.secondary">
                No bottlenecks detected
              </Typography>
            ) : (
              <Box>
                {performanceData.bottlenecks.slice(0, 3).map((bottleneck, index) => (
                  <Alert key={index} severity="warning" sx={{ mb: 1 }}>
                    <Typography variant="subtitle2">
                      {bottleneck.component_name}: {bottleneck.type}
                    </Typography>
                    <Typography variant="body2">
                      {bottleneck.description}
                    </Typography>
                    <Typography variant="caption">
                      Severity: {(bottleneck.severity_score * 100).toFixed(0)}%
                    </Typography>
                  </Alert>
                ))}
                
                {performanceData.bottlenecks.length > 3 && (
                  <Typography variant="caption">
                    + {performanceData.bottlenecks.length - 3} more bottlenecks
                  </Typography>
                )}
              </Box>
            )}
          </Paper>
        </Grid>
      </Grid>

      {/* Settings Dialog */}
      <Dialog open={showSettings} onClose={() => setShowSettings(false)} maxWidth="sm" fullWidth>
        <DialogTitle>Performance Monitoring Settings</DialogTitle>
        <DialogContent>
          <Box sx={{ mt: 2 }}>
            <FormControlLabel
              control={
                <Switch
                  checked={autoRefresh}
                  onChange={(e) => setAutoRefresh(e.target.checked)}
                />
              }
              label="Auto Refresh"
            />
            
            {autoRefresh && (
              <FormControl fullWidth sx={{ mt: 2 }}>
                <InputLabel>Refresh Interval</InputLabel>
                <Select
                  value={refreshInterval}
                  label="Refresh Interval"
                  onChange={(e) => setRefreshInterval(e.target.value)}
                >
                  <MenuItem value={1}>1 second</MenuItem>
                  <MenuItem value={5}>5 seconds</MenuItem>
                  <MenuItem value={10}>10 seconds</MenuItem>
                  <MenuItem value={30}>30 seconds</MenuItem>
                  <MenuItem value={60}>1 minute</MenuItem>
                </Select>
              </FormControl>
            )}
          </Box>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowSettings(false)}>Close</Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
};

export default PerformanceMonitoringDashboard;
