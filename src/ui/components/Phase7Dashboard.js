import React, { useState, useEffect, useCallback } from 'react';
import {
  Box,
  Paper,
  Typography,
  Grid,
  Card,
  CardContent,
  Tabs,
  Tab,
  Switch,
  FormControlLabel,
  Button,
  Alert,
  Chip,
  IconButton,
  Tooltip,
  CircularProgress
} from '@mui/material';
import {
  Speed as SpeedIcon,
  Memory as MemoryIcon,
  NetworkCheck as NetworkIcon,
  Timer as TimerIcon,
  TrendingUp as TrendingUpIcon,
  Warning as WarningIcon,
  Refresh as RefreshIcon,
  GetApp as DownloadIcon,
  Settings as SettingsIcon
} from '@mui/icons-material';

// Import Phase 7 sub-components
import SystemMetricsDashboard from './SystemMetricsDashboard';
import LatencyTrackingInterface from './LatencyTrackingInterface';
import ThroughputMonitoringPanel from './ThroughputMonitoringPanel';
import SystemHealthIndicators from './SystemHealthIndicators';
import BottleneckIdentificationPanel from './BottleneckIdentificationPanel';
import PerformanceTrendAnalysis from './PerformanceTrendAnalysis';

function TabPanel({ children, value, index, ...other }) {
  return (
    <div
      role="tabpanel"
      hidden={value !== index}
      id={`phase7-tabpanel-${index}`}
      aria-labelledby={`phase7-tab-${index}`}
      {...other}
    >
      {value === index && (
        <Box sx={{ p: 3 }}>
          {children}
        </Box>
      )}
    </div>
  );
}

const Phase7Dashboard = () => {
  const [currentTab, setCurrentTab] = useState(0);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5); // seconds
  const [loading, setLoading] = useState(false);
  const [lastUpdate, setLastUpdate] = useState(null);
  const [connectionStatus, setConnectionStatus] = useState('connected');
  
  // Performance summary data
  const [performanceSummary, setPerformanceSummary] = useState({
    cpu_usage: 0,
    memory_usage: 0,
    avg_latency: 0,
    throughput: 0,
    health_score: 0,
    active_alerts: 0
  });

  // Fetch performance summary data
  const fetchPerformanceSummary = useCallback(async () => {
    try {
      setLoading(true);
      
      // Fetch data from multiple endpoints
      const [systemMetrics, latencyMetrics, throughputMetrics, healthStatus] = await Promise.all([
        fetch('/api/performance/system-metrics').then(r => r.json()),
        fetch('/api/performance/latency-metrics').then(r => r.json()),
        fetch('/api/performance/throughput-metrics').then(r => r.json()),
        fetch('/api/performance/health-status').then(r => r.json())
      ]);

      setPerformanceSummary({
        cpu_usage: systemMetrics.cpu_usage_pct || 0,
        memory_usage: systemMetrics.memory_usage_pct || 0,
        avg_latency: latencyMetrics.average_latency_ms || 0,
        throughput: throughputMetrics.current_throughput_per_sec || 0,
        health_score: healthStatus.health_score || 0,
        active_alerts: healthStatus.active_alerts?.length || 0
      });

      setLastUpdate(new Date());
      setConnectionStatus('connected');
    } catch (error) {
      console.error('Error fetching performance summary:', error);
      setConnectionStatus('error');
    } finally {
      setLoading(false);
    }
  }, []);

  // Auto-refresh effect
  useEffect(() => {
    fetchPerformanceSummary();
    
    if (autoRefresh) {
      const interval = setInterval(fetchPerformanceSummary, refreshInterval * 1000);
      return () => clearInterval(interval);
    }
  }, [fetchPerformanceSummary, autoRefresh, refreshInterval]);

  const handleTabChange = (event, newValue) => {
    setCurrentTab(newValue);
  };

  const handleRefresh = () => {
    fetchPerformanceSummary();
  };

  const handleExportReport = async () => {
    try {
      const response = await fetch('/api/performance/export-report');
      const reportData = await response.json();
      
      // Create and download the report
      const blob = new Blob([JSON.stringify(reportData, null, 2)], { type: 'application/json' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `performance-report-${new Date().toISOString().split('T')[0]}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);
    } catch (error) {
      console.error('Error exporting report:', error);
    }
  };

  const getHealthColor = (score) => {
    if (score >= 90) return 'success';
    if (score >= 75) return 'warning';
    return 'error';
  };

  const getConnectionStatusColor = () => {
    switch (connectionStatus) {
      case 'connected': return 'success';
      case 'connecting': return 'warning';
      case 'error': return 'error';
      default: return 'default';
    }
  };

  return (
    <Box sx={{ width: '100%', p: 3 }}>
      {/* Header */}
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
        <Typography variant="h4" component="h1" sx={{ fontWeight: 'bold', color: 'primary.main' }}>
          Phase 7: Performance Monitoring & Optimization
        </Typography>
        
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
          <Chip 
            label={`Health: ${performanceSummary.health_score}%`}
            color={getHealthColor(performanceSummary.health_score)}
            icon={<SpeedIcon />}
          />
          <Chip 
            label={connectionStatus === 'connected' ? 'Connected' : 'Disconnected'}
            color={getConnectionStatusColor()}
            variant="outlined"
          />
          <FormControlLabel
            control={
              <Switch 
                checked={autoRefresh} 
                onChange={(e) => setAutoRefresh(e.target.checked)}
                color="primary"
              />
            }
            label="Auto Refresh"
          />
          <Tooltip title="Refresh Now">
            <IconButton onClick={handleRefresh} disabled={loading}>
              <RefreshIcon />
            </IconButton>
          </Tooltip>
          <Tooltip title="Export Performance Report">
            <IconButton onClick={handleExportReport}>
              <DownloadIcon />
            </IconButton>
          </Tooltip>
        </Box>
      </Box>

      {/* Quick Stats Cards */}
      <Grid container spacing={3} sx={{ mb: 3 }}>
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <MemoryIcon color="primary" sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {performanceSummary.cpu_usage.toFixed(1)}%
              </Typography>
              <Typography color="text.secondary" variant="body2">
                CPU Usage
              </Typography>
            </CardContent>
          </Card>
        </Grid>
        
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <MemoryIcon color="secondary" sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {performanceSummary.memory_usage.toFixed(1)}%
              </Typography>
              <Typography color="text.secondary" variant="body2">
                Memory Usage
              </Typography>
            </CardContent>
          </Card>
        </Grid>
        
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <TimerIcon color="info" sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {performanceSummary.avg_latency.toFixed(2)}ms
              </Typography>
              <Typography color="text.secondary" variant="body2">
                Avg Latency
              </Typography>
            </CardContent>
          </Card>
        </Grid>
        
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <TrendingUpIcon color="success" sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {Math.round(performanceSummary.throughput)}
              </Typography>
              <Typography color="text.secondary" variant="body2">
                Throughput/sec
              </Typography>
            </CardContent>
          </Card>
        </Grid>
        
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <SpeedIcon color={getHealthColor(performanceSummary.health_score)} sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {performanceSummary.health_score}%
              </Typography>
              <Typography color="text.secondary" variant="body2">
                Health Score
              </Typography>
            </CardContent>
          </Card>
        </Grid>
        
        <Grid item xs={12} sm={6} md={2}>
          <Card>
            <CardContent sx={{ textAlign: 'center', py: 2 }}>
              <WarningIcon color={performanceSummary.active_alerts > 0 ? 'warning' : 'disabled'} sx={{ fontSize: 40, mb: 1 }} />
              <Typography variant="h6" component="div">
                {performanceSummary.active_alerts}
              </Typography>
              <Typography color="text.secondary" variant="body2">
                Active Alerts
              </Typography>
            </CardContent>
          </Card>
        </Grid>
      </Grid>

      {/* Status Alert */}
      {connectionStatus === 'error' && (
        <Alert severity="error" sx={{ mb: 3 }}>
          Connection lost. Attempting to reconnect...
        </Alert>
      )}
      
      {lastUpdate && (
        <Typography variant="caption" color="text.secondary" sx={{ mb: 2, display: 'block' }}>
          Last updated: {lastUpdate.toLocaleTimeString()}
        </Typography>
      )}

      {/* Main Dashboard Tabs */}
      <Paper sx={{ width: '100%' }}>
        <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
          <Tabs value={currentTab} onChange={handleTabChange} aria-label="Performance monitoring tabs">
            <Tab 
              label="System Metrics" 
              icon={<MemoryIcon />}
              iconPosition="start"
            />
            <Tab 
              label="Latency Tracking" 
              icon={<TimerIcon />}
              iconPosition="start"
            />
            <Tab 
              label="Throughput Monitoring" 
              icon={<TrendingUpIcon />}
              iconPosition="start"
            />
            <Tab 
              label="System Health" 
              icon={<SpeedIcon />}
              iconPosition="start"
            />
            <Tab 
              label="Bottleneck Analysis" 
              icon={<WarningIcon />}
              iconPosition="start"
            />
            <Tab 
              label="Performance Trends" 
              icon={<NetworkIcon />}
              iconPosition="start"
            />
          </Tabs>
        </Box>

        <TabPanel value={currentTab} index={0}>
          <SystemMetricsDashboard autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>

        <TabPanel value={currentTab} index={1}>
          <LatencyTrackingInterface autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>

        <TabPanel value={currentTab} index={2}>
          <ThroughputMonitoringPanel autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>

        <TabPanel value={currentTab} index={3}>
          <SystemHealthIndicators autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>

        <TabPanel value={currentTab} index={4}>
          <BottleneckIdentificationPanel autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>

        <TabPanel value={currentTab} index={5}>
          <PerformanceTrendAnalysis autoRefresh={autoRefresh} refreshInterval={refreshInterval} />
        </TabPanel>
      </Paper>
    </Box>
  );
};

export default Phase7Dashboard;
