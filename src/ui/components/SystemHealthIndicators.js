import React, { useState, useEffect } from 'react';
import {
  Box,
  Paper,
  Typography,
  Grid,
  Card,
  CardContent,
  Alert,
  Chip,
  List,
  ListItem,
  ListItemText,
  ListItemIcon,
  LinearProgress,
  Divider,
  Tooltip,
  IconButton,
  Badge
} from '@mui/material';
import {
  CheckCircle as CheckCircleIcon,
  Warning as WarningIcon,
  Error as ErrorIcon,
  InfoOutlined as InfoIcon,
  TrendingUp as TrendingUpIcon,
  Schedule as ScheduleIcon,
  Refresh as RefreshIcon,
  Computer as ComputerIcon,
  Storage as StorageIcon,
  DataUsage as DataUsageIcon,
  NetworkCheck as NetworkCheckIcon,
  Settings as SettingsIcon
} from '@mui/icons-material';

const SystemHealthIndicators = () => {
  const [healthData, setHealthData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [autoRefresh, setAutoRefresh] = useState(true);

  // Fetch health status data
  const fetchHealthData = async () => {
    try {
      const response = await fetch('/api/performance/health-status');
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const data = await response.json();
      setHealthData(data);
      setError(null);
    } catch (err) {
      setError(`Failed to fetch health data: ${err.message}`);
      console.error('Error fetching health data:', err);
    } finally {
      setLoading(false);
    }
  };

  // Auto-refresh effect
  useEffect(() => {
    fetchHealthData();
    
    if (autoRefresh) {
      const interval = setInterval(fetchHealthData, 5000); // Every 5 seconds
      return () => clearInterval(interval);
    }
  }, [autoRefresh]);

  // Get status icon and color
  const getStatusIcon = (status) => {
    switch (status) {
      case 'HEALTHY':
      case 'EXCELLENT':
      case 'GOOD':
        return <CheckCircleIcon color="success" />;
      case 'WARNING':
        return <WarningIcon color="warning" />;
      case 'CRITICAL':
      case 'ERROR':
        return <ErrorIcon color="error" />;
      default:
        return <InfoIcon color="info" />;
    }
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'HEALTHY':
      case 'EXCELLENT':
      case 'GOOD':
        return 'success';
      case 'WARNING':
        return 'warning';
      case 'CRITICAL':
      case 'ERROR':
        return 'error';
      default:
        return 'info';
    }
  };

  const getSeverityColor = (severity) => {
    switch (severity) {
      case 'HIGH':
        return 'error';
      case 'MEDIUM':
        return 'warning';
      case 'LOW':
        return 'info';
      default:
        return 'default';
    }
  };

  const formatUptime = (seconds) => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hours}h ${minutes}m ${secs}s`;
  };

  const formatTimestamp = (timestamp) => {
    return new Date(timestamp).toLocaleString();
  };

  if (loading) {
    return (
      <Box display="flex" justifyContent="center" alignItems="center" minHeight="300px">
        <LinearProgress sx={{ width: '50%' }} />
      </Box>
    );
  }

  if (error) {
    return (
      <Alert severity="error" sx={{ m: 2 }}>
        {error}
      </Alert>
    );
  }

  return (
    <Box sx={{ p: 2 }}>
      {/* Header */}
      <Box display="flex" justifyContent="space-between" alignItems="center" mb={3}>
        <Typography variant="h5" component="h2" gutterBottom>
          System Health Indicators
        </Typography>
        <Box>
          <Tooltip title="Refresh">
            <IconButton onClick={fetchHealthData} color="primary">
              <RefreshIcon />
            </IconButton>
          </Tooltip>
        </Box>
      </Box>

      <Grid container spacing={3}>
        {/* Overall Health Status */}
        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center" mb={2}>
                {getStatusIcon(healthData?.overall_status)}
                <Box ml={2}>
                  <Typography variant="h6">Overall System Status</Typography>
                  <Chip 
                    label={healthData?.overall_status || 'UNKNOWN'}
                    color={getStatusColor(healthData?.overall_status)}
                    size="small"
                  />
                </Box>
              </Box>
              
              <Box mb={2}>
                <Typography variant="body2" color="text.secondary">
                  Health Score: {healthData?.health_score || 0}/100
                </Typography>
                <LinearProgress 
                  variant="determinate" 
                  value={healthData?.health_score || 0} 
                  sx={{ mt: 1 }}
                  color={getStatusColor(healthData?.overall_status)}
                />
              </Box>

              <Typography variant="body2" color="text.secondary">
                Uptime: {formatUptime(healthData?.uptime_seconds || 0)}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Last Restart: {formatTimestamp(healthData?.last_restart)}
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Performance Trend */}
        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center" mb={2}>
                <TrendingUpIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">Performance Trend</Typography>
                  <Chip 
                    label={healthData?.performance_trend || 'UNKNOWN'}
                    color={healthData?.performance_trend === 'STABLE' ? 'success' : 'warning'}
                    size="small"
                  />
                </Box>
              </Box>
              
              <Typography variant="body2" color="text.secondary">
                System performance has been {healthData?.performance_trend?.toLowerCase() || 'unknown'} 
                over the last monitoring period.
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Component Health */}
        <Grid item xs={12} md={8}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Component Health Status
              </Typography>
              <List>
                {healthData?.components?.map((component, index) => (
                  <React.Fragment key={index}>
                    <ListItem>
                      <ListItemIcon>
                        {getStatusIcon(component.status)}
                      </ListItemIcon>
                      <ListItemText
                        primary={component.name}
                        secondary={`Score: ${component.score}/100 - Last Check: ${formatTimestamp(component.last_check)}`}
                      />
                      <Box>
                        <Chip 
                          label={component.status}
                          color={getStatusColor(component.status)}
                          size="small"
                        />
                      </Box>
                    </ListItem>
                    {index < healthData.components.length - 1 && <Divider />}
                  </React.Fragment>
                ))}
              </List>
            </CardContent>
          </Card>
        </Grid>

        {/* Active Alerts */}
        <Grid item xs={12} md={4}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Active Alerts
              </Typography>
              {healthData?.active_alerts?.length > 0 ? (
                <List>
                  {healthData.active_alerts.map((alert, index) => (
                    <ListItem key={index} sx={{ pl: 0 }}>
                      <ListItemIcon>
                        <ErrorIcon color="error" />
                      </ListItemIcon>
                      <ListItemText
                        primary={alert.message}
                        secondary={`Severity: ${alert.severity}`}
                      />
                    </ListItem>
                  ))}
                </List>
              ) : (
                <Typography variant="body2" color="text.secondary" sx={{ textAlign: 'center', py: 2 }}>
                  No active alerts
                </Typography>
              )}
            </CardContent>
          </Card>
        </Grid>

        {/* Predictive Warnings */}
        <Grid item xs={12}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Predictive Warnings
              </Typography>
              {healthData?.predictive_warnings?.length > 0 ? (
                <Grid container spacing={2}>
                  {healthData.predictive_warnings.map((warning, index) => (
                    <Grid item xs={12} md={6} key={index}>
                      <Alert 
                        severity={getSeverityColor(warning.severity)}
                        icon={<ScheduleIcon />}
                      >
                        <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                          {warning.type?.replace('_', ' ').toUpperCase()}
                        </Typography>
                        <Typography variant="body2">
                          {warning.message}
                        </Typography>
                        <Typography variant="caption" color="text.secondary">
                          ETA: {warning.eta_hours}h
                        </Typography>
                      </Alert>
                    </Grid>
                  ))}
                </Grid>
              ) : (
                <Typography variant="body2" color="text.secondary" sx={{ textAlign: 'center', py: 2 }}>
                  No predictive warnings
                </Typography>
              )}
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  );
};

export default SystemHealthIndicators;
