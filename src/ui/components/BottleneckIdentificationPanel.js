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
  Button,
  Tooltip,
  IconButton,
  Badge,
  Accordion,
  AccordionSummary,
  AccordionDetails,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow
} from '@mui/material';
import {
  Warning as WarningIcon,
  Error as ErrorIcon,
  Info as InfoIcon,
  Speed as SpeedIcon,
  Memory as MemoryIcon,
  Storage as StorageIcon,
  NetworkCheck as NetworkCheckIcon,
  Refresh as RefreshIcon,
  AutoFix as AutoFixIcon,
  ExpandMore as ExpandMoreIcon,
  TrendingUp as TrendingUpIcon,
  Build as BuildIcon,
  Assessment as AssessmentIcon
} from '@mui/icons-material';

const BottleneckIdentificationPanel = () => {
  const [bottleneckData, setBottleneckData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [expandedPanel, setExpandedPanel] = useState(false);

  // Fetch bottleneck data
  const fetchBottleneckData = async () => {
    try {
      const response = await fetch('/api/performance/bottlenecks');
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const data = await response.json();
      setBottleneckData(data);
      setError(null);
    } catch (err) {
      setError(`Failed to fetch bottleneck data: ${err.message}`);
      console.error('Error fetching bottleneck data:', err);
    } finally {
      setLoading(false);
    }
  };

  // Auto-refresh effect
  useEffect(() => {
    fetchBottleneckData();
    
    if (autoRefresh) {
      const interval = setInterval(fetchBottleneckData, 10000); // Every 10 seconds
      return () => clearInterval(interval);
    }
  }, [autoRefresh]);

  // Get bottleneck type icon
  const getBottleneckTypeIcon = (type) => {
    switch (type) {
      case 'CPU_BOUND':
        return <SpeedIcon color="warning" />;
      case 'MEMORY_BOUND':
        return <MemoryIcon color="error" />;
      case 'IO_BOUND':
        return <StorageIcon color="info" />;
      case 'NETWORK_BOUND':
        return <NetworkCheckIcon color="primary" />;
      default:
        return <AssessmentIcon color="action" />;
    }
  };

  // Get severity color
  const getSeverityColor = (severity) => {
    switch (severity) {
      case 'CRITICAL':
        return 'error';
      case 'WARNING':
        return 'warning';
      case 'INFO':
        return 'info';
      default:
        return 'default';
    }
  };

  // Get impact score color
  const getImpactScoreColor = (score) => {
    if (score >= 70) return 'error';
    if (score >= 40) return 'warning';
    return 'success';
  };

  // Handle accordion expand
  const handleAccordionChange = (panel) => (event, isExpanded) => {
    setExpandedPanel(isExpanded ? panel : false);
  };

  // Format timestamp
  const formatTimestamp = (timestamp) => {
    const now = Date.now();
    const diffMinutes = Math.floor((now - timestamp) / 60000);
    if (diffMinutes < 1) return 'Just now';
    if (diffMinutes < 60) return `${diffMinutes}m ago`;
    const diffHours = Math.floor(diffMinutes / 60);
    if (diffHours < 24) return `${diffHours}h ago`;
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
          Bottleneck Identification
        </Typography>
        <Box>
          <Tooltip title="Refresh">
            <IconButton onClick={fetchBottleneckData} color="primary">
              <RefreshIcon />
            </IconButton>
          </Tooltip>
        </Box>
      </Box>

      <Grid container spacing={3}>
        {/* Summary Cards */}
        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center">
                <ErrorIcon color="error" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {bottleneckData?.critical_bottlenecks || 0}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Critical Issues
                  </Typography>
                </Box>
              </Box>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center">
                <WarningIcon color="warning" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {bottleneckData?.warning_bottlenecks || 0}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Warning Issues
                  </Typography>
                </Box>
              </Box>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center">
                <InfoIcon color="info" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {bottleneckData?.info_bottlenecks || 0}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Info Issues
                  </Typography>
                </Box>
              </Box>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Box display="flex" alignItems="center">
                <AssessmentIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {bottleneckData?.total_bottlenecks_found || 0}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Total Found
                  </Typography>
                </Box>
              </Box>
            </CardContent>
          </Card>
        </Grid>

        {/* Bottleneck Details */}
        <Grid item xs={12} lg={8}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Detected Bottlenecks
              </Typography>
              
              {bottleneckData?.bottlenecks?.length > 0 ? (
                <Box>
                  {bottleneckData.bottlenecks.map((bottleneck, index) => (
                    <Accordion 
                      key={bottleneck.id}
                      expanded={expandedPanel === bottleneck.id}
                      onChange={handleAccordionChange(bottleneck.id)}
                    >
                      <AccordionSummary expandIcon={<ExpandMoreIcon />}>
                        <Box display="flex" alignItems="center" width="100%">
                          <Box display="flex" alignItems="center" flex={1}>
                            {getBottleneckTypeIcon(bottleneck.type)}
                            <Box ml={2}>
                              <Typography variant="subtitle1">
                                {bottleneck.component}
                              </Typography>
                              <Typography variant="body2" color="text.secondary">
                                {bottleneck.description}
                              </Typography>
                            </Box>
                          </Box>
                          <Box display="flex" alignItems="center" gap={1}>
                            <Chip 
                              label={bottleneck.severity}
                              color={getSeverityColor(bottleneck.severity)}
                              size="small"
                            />
                            <Chip 
                              label={`Impact: ${bottleneck.impact_score}%`}
                              color={getImpactScoreColor(bottleneck.impact_score)}
                              size="small"
                            />
                            {bottleneck.auto_fix_available && (
                              <Tooltip title="Auto-fix available">
                                <AutoFixIcon color="success" fontSize="small" />
                              </Tooltip>
                            )}
                          </Box>
                        </Box>
                      </AccordionSummary>
                      <AccordionDetails>
                        <Grid container spacing={2}>
                          <Grid item xs={12} md={6}>
                            <Typography variant="subtitle2" gutterBottom>
                              Performance Metrics
                            </Typography>
                            <TableContainer>
                              <Table size="small">
                                <TableBody>
                                  {bottleneck.cpu_usage_pct && (
                                    <TableRow>
                                      <TableCell>CPU Usage</TableCell>
                                      <TableCell>{bottleneck.cpu_usage_pct.toFixed(1)}%</TableCell>
                                    </TableRow>
                                  )}
                                  {bottleneck.memory_usage_mb && (
                                    <TableRow>
                                      <TableCell>Memory Usage</TableCell>
                                      <TableCell>{bottleneck.memory_usage_mb} MB</TableCell>
                                    </TableRow>
                                  )}
                                  {bottleneck.queue_size && (
                                    <TableRow>
                                      <TableCell>Queue Size</TableCell>
                                      <TableCell>{bottleneck.queue_size}</TableCell>
                                    </TableRow>
                                  )}
                                  {bottleneck.processing_rate && (
                                    <TableRow>
                                      <TableCell>Processing Rate</TableCell>
                                      <TableCell>{bottleneck.processing_rate}/sec</TableCell>
                                    </TableRow>
                                  )}
                                  {bottleneck.cache_hit_ratio && (
                                    <TableRow>
                                      <TableCell>Cache Hit Ratio</TableCell>
                                      <TableCell>{(bottleneck.cache_hit_ratio * 100).toFixed(1)}%</TableCell>
                                    </TableRow>
                                  )}
                                </TableBody>
                              </Table>
                            </TableContainer>
                          </Grid>
                          <Grid item xs={12} md={6}>
                            <Typography variant="subtitle2" gutterBottom>
                              Recommendation
                            </Typography>
                            <Typography variant="body2" sx={{ mb: 2 }}>
                              {bottleneck.recommendation}
                            </Typography>
                            <Typography variant="body2" color="text.secondary">
                              Detected: {formatTimestamp(bottleneck.detected_at)}
                            </Typography>
                            {bottleneck.auto_fix_available && (
                              <Button
                                variant="outlined"
                                color="success"
                                size="small"
                                startIcon={<AutoFixIcon />}
                                sx={{ mt: 1 }}
                              >
                                Apply Auto-Fix
                              </Button>
                            )}
                          </Grid>
                        </Grid>
                      </AccordionDetails>
                    </Accordion>
                  ))}
                </Box>
              ) : (
                <Typography variant="body2" color="text.secondary" sx={{ textAlign: 'center', py: 4 }}>
                  No bottlenecks detected
                </Typography>
              )}
            </CardContent>
          </Card>
        </Grid>

        {/* Optimization Suggestions */}
        <Grid item xs={12} lg={4}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Optimization Suggestions
              </Typography>
              
              {bottleneckData?.optimization_suggestions?.length > 0 ? (
                <List>
                  {bottleneckData.optimization_suggestions.map((suggestion, index) => (
                    <ListItem key={index} sx={{ pl: 0 }}>
                      <ListItemIcon>
                        <BuildIcon color="primary" />
                      </ListItemIcon>
                      <ListItemText
                        primary={suggestion}
                        primaryTypographyProps={{ variant: 'body2' }}
                      />
                    </ListItem>
                  ))}
                </List>
              ) : (
                <Typography variant="body2" color="text.secondary" sx={{ textAlign: 'center', py: 2 }}>
                  No optimization suggestions available
                </Typography>
              )}
              
              <Divider sx={{ my: 2 }} />
              
              <Typography variant="body2" color="text.secondary">
                Next scan in: {Math.floor((bottleneckData?.next_scan_in_seconds || 0) / 60)}m {(bottleneckData?.next_scan_in_seconds || 0) % 60}s
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Scan Information */}
        <Grid item xs={12}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Scan Information
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Last scan: {new Date(bottleneckData?.scan_timestamp).toLocaleString()}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Total bottlenecks found: {bottleneckData?.total_bottlenecks_found || 0}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Next automatic scan: {Math.floor((bottleneckData?.next_scan_in_seconds || 0) / 60)} minutes
              </Typography>
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  );
};

export default BottleneckIdentificationPanel;
