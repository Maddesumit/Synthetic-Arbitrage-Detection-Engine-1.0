import React, { useState, useEffect } from 'react';
import {
  Box,
  Paper,
  Typography,
  Grid,
  Card,
  CardContent,
  Alert,
  Button,
  ButtonGroup,
  Tooltip,
  IconButton,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  LinearProgress,
  Chip,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow
} from '@mui/material';
import {
  TrendingUp as TrendingUpIcon,
  TrendingDown as TrendingDownIcon,
  TrendingFlat as TrendingFlatIcon,
  Refresh as RefreshIcon,
  GetApp as DownloadIcon,
  Assessment as AssessmentIcon,
  Speed as SpeedIcon,
  Memory as MemoryIcon,
  NetworkCheck as NetworkCheckIcon,
  Timeline as TimelineIcon
} from '@mui/icons-material';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip as RechartsTooltip,
  ResponsiveContainer,
  AreaChart,
  Area,
  BarChart,
  Bar,
  Legend
} from 'recharts';

const PerformanceTrendAnalysis = () => {
  const [trendData, setTrendData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [timeRange, setTimeRange] = useState('1h');
  const [selectedMetric, setSelectedMetric] = useState('cpu_usage_pct');

  // Fetch performance history data
  const fetchTrendData = async (range = timeRange) => {
    try {
      setLoading(true);
      const response = await fetch(`/api/performance/history?range=${range}`);
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const data = await response.json();
      setTrendData(data);
      setError(null);
    } catch (err) {
      setError(`Failed to fetch trend data: ${err.message}`);
      console.error('Error fetching trend data:', err);
    } finally {
      setLoading(false);
    }
  };

  // Effect to fetch data when time range changes
  useEffect(() => {
    fetchTrendData();
  }, [timeRange]);

  // Handle time range change
  const handleTimeRangeChange = (event) => {
    setTimeRange(event.target.value);
  };

  // Handle metric selection
  const handleMetricChange = (event) => {
    setSelectedMetric(event.target.value);
  };

  // Format timestamp for chart
  const formatTimestamp = (timestamp) => {
    const date = new Date(timestamp);
    if (timeRange === '1h') {
      return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    } else if (timeRange === '1d') {
      return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    } else {
      return date.toLocaleDateString([], { month: 'short', day: 'numeric' });
    }
  };

  // Get trend direction
  const getTrendDirection = (data, metric) => {
    if (!data || data.length < 2) return 'flat';
    
    const recent = data.slice(-10);
    const older = data.slice(0, 10);
    
    const recentAvg = recent.reduce((sum, item) => sum + item[metric], 0) / recent.length;
    const olderAvg = older.reduce((sum, item) => sum + item[metric], 0) / older.length;
    
    const percentChange = ((recentAvg - olderAvg) / olderAvg) * 100;
    
    if (percentChange > 5) return 'up';
    if (percentChange < -5) return 'down';
    return 'flat';
  };

  // Get trend icon
  const getTrendIcon = (direction) => {
    switch (direction) {
      case 'up':
        return <TrendingUpIcon color="success" />;
      case 'down':
        return <TrendingDownIcon color="error" />;
      default:
        return <TrendingFlatIcon color="primary" />;
    }
  };

  // Get metric display name
  const getMetricDisplayName = (metric) => {
    switch (metric) {
      case 'cpu_usage_pct':
        return 'CPU Usage (%)';
      case 'memory_usage_pct':
        return 'Memory Usage (%)';
      case 'average_latency_ms':
        return 'Average Latency (ms)';
      case 'throughput_per_sec':
        return 'Throughput (per sec)';
      case 'health_score':
        return 'Health Score';
      default:
        return metric;
    }
  };

  // Get metric color
  const getMetricColor = (metric) => {
    switch (metric) {
      case 'cpu_usage_pct':
        return '#ff7300';
      case 'memory_usage_pct':
        return '#8884d8';
      case 'average_latency_ms':
        return '#82ca9d';
      case 'throughput_per_sec':
        return '#ffc658';
      case 'health_score':
        return '#00ff00';
      default:
        return '#8884d8';
    }
  };

  // Export data
  const handleExport = async () => {
    try {
      const response = await fetch('/api/performance/export-report?format=csv');
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `performance_report_${Date.now()}.csv`;
      a.click();
      window.URL.revokeObjectURL(url);
    } catch (err) {
      console.error('Error exporting data:', err);
    }
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

  const trendDirection = getTrendDirection(trendData?.data, selectedMetric);

  return (
    <Box sx={{ p: 2 }}>
      {/* Header */}
      <Box display="flex" justifyContent="space-between" alignItems="center" mb={3}>
        <Typography variant="h5" component="h2" gutterBottom>
          Performance Trend Analysis
        </Typography>
        <Box display="flex" gap={2}>
          <FormControl size="small" sx={{ minWidth: 120 }}>
            <InputLabel>Time Range</InputLabel>
            <Select value={timeRange} onChange={handleTimeRangeChange} label="Time Range">
              <MenuItem value="1h">1 Hour</MenuItem>
              <MenuItem value="1d">1 Day</MenuItem>
              <MenuItem value="1w">1 Week</MenuItem>
            </Select>
          </FormControl>
          <FormControl size="small" sx={{ minWidth: 150 }}>
            <InputLabel>Metric</InputLabel>
            <Select value={selectedMetric} onChange={handleMetricChange} label="Metric">
              <MenuItem value="cpu_usage_pct">CPU Usage</MenuItem>
              <MenuItem value="memory_usage_pct">Memory Usage</MenuItem>
              <MenuItem value="average_latency_ms">Latency</MenuItem>
              <MenuItem value="throughput_per_sec">Throughput</MenuItem>
              <MenuItem value="health_score">Health Score</MenuItem>
            </Select>
          </FormControl>
          <Tooltip title="Export Data">
            <IconButton onClick={handleExport} color="primary">
              <DownloadIcon />
            </IconButton>
          </Tooltip>
          <Tooltip title="Refresh">
            <IconButton onClick={() => fetchTrendData()} color="primary">
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
                <SpeedIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {trendData?.summary?.avg_cpu_usage?.toFixed(1)}%
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Avg CPU Usage
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
                <MemoryIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {trendData?.summary?.avg_memory_usage?.toFixed(1)}%
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Avg Memory Usage
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
                <TimelineIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {trendData?.summary?.avg_latency_ms?.toFixed(2)}ms
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Avg Latency
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
                <NetworkCheckIcon color="primary" />
                <Box ml={2}>
                  <Typography variant="h6">
                    {trendData?.summary?.avg_throughput?.toFixed(0)}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Avg Throughput/sec
                  </Typography>
                </Box>
              </Box>
            </CardContent>
          </Card>
        </Grid>

        {/* Trend Chart */}
        <Grid item xs={12} lg={8}>
          <Card>
            <CardContent>
              <Box display="flex" justifyContent="space-between" alignItems="center" mb={2}>
                <Typography variant="h6">
                  {getMetricDisplayName(selectedMetric)} Trend
                </Typography>
                <Box display="flex" alignItems="center">
                  {getTrendIcon(trendDirection)}
                  <Chip 
                    label={trendDirection.toUpperCase()}
                    color={trendDirection === 'up' ? 'success' : trendDirection === 'down' ? 'error' : 'primary'}
                    size="small"
                    sx={{ ml: 1 }}
                  />
                </Box>
              </Box>
              
              <ResponsiveContainer width="100%" height={300}>
                <AreaChart data={trendData?.data || []}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis 
                    dataKey="timestamp" 
                    tickFormatter={formatTimestamp}
                    tick={{ fontSize: 12 }}
                  />
                  <YAxis tick={{ fontSize: 12 }} />
                  <RechartsTooltip 
                    labelFormatter={(value) => formatTimestamp(value)}
                    formatter={(value) => [value.toFixed(2), getMetricDisplayName(selectedMetric)]}
                  />
                  <Area 
                    type="monotone" 
                    dataKey={selectedMetric} 
                    stroke={getMetricColor(selectedMetric)}
                    fill={getMetricColor(selectedMetric)}
                    fillOpacity={0.6}
                  />
                </AreaChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </Grid>

        {/* Performance Summary */}
        <Grid item xs={12} lg={4}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Performance Summary
              </Typography>
              
              <TableContainer>
                <Table size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell>Metric</TableCell>
                      <TableCell align="right">Average</TableCell>
                      <TableCell align="right">Peak</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    <TableRow>
                      <TableCell>CPU Usage</TableCell>
                      <TableCell align="right">{trendData?.summary?.avg_cpu_usage?.toFixed(1)}%</TableCell>
                      <TableCell align="right">{trendData?.summary?.peak_cpu_usage?.toFixed(1)}%</TableCell>
                    </TableRow>
                    <TableRow>
                      <TableCell>Memory Usage</TableCell>
                      <TableCell align="right">{trendData?.summary?.avg_memory_usage?.toFixed(1)}%</TableCell>
                      <TableCell align="right">{trendData?.summary?.peak_memory_usage?.toFixed(1)}%</TableCell>
                    </TableRow>
                    <TableRow>
                      <TableCell>Latency</TableCell>
                      <TableCell align="right">{trendData?.summary?.avg_latency_ms?.toFixed(2)}ms</TableCell>
                      <TableCell align="right">{trendData?.summary?.max_latency_ms?.toFixed(2)}ms</TableCell>
                    </TableRow>
                    <TableRow>
                      <TableCell>Throughput</TableCell>
                      <TableCell align="right">{trendData?.summary?.avg_throughput?.toFixed(0)}/s</TableCell>
                      <TableCell align="right">{trendData?.summary?.peak_throughput?.toFixed(0)}/s</TableCell>
                    </TableRow>
                  </TableBody>
                </Table>
              </TableContainer>
            </CardContent>
          </Card>
        </Grid>

        {/* Multi-metric Overview */}
        <Grid item xs={12}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Multi-metric Overview
              </Typography>
              
              <ResponsiveContainer width="100%" height={400}>
                <LineChart data={trendData?.data || []}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis 
                    dataKey="timestamp" 
                    tickFormatter={formatTimestamp}
                    tick={{ fontSize: 12 }}
                  />
                  <YAxis yAxisId="left" tick={{ fontSize: 12 }} />
                  <YAxis yAxisId="right" orientation="right" tick={{ fontSize: 12 }} />
                  <RechartsTooltip 
                    labelFormatter={(value) => formatTimestamp(value)}
                    formatter={(value, name) => [value.toFixed(2), getMetricDisplayName(name)]}
                  />
                  <Legend />
                  <Line 
                    yAxisId="left"
                    type="monotone" 
                    dataKey="cpu_usage_pct" 
                    stroke="#ff7300"
                    name="CPU Usage (%)"
                    strokeWidth={2}
                  />
                  <Line 
                    yAxisId="left"
                    type="monotone" 
                    dataKey="memory_usage_pct" 
                    stroke="#8884d8"
                    name="Memory Usage (%)"
                    strokeWidth={2}
                  />
                  <Line 
                    yAxisId="right"
                    type="monotone" 
                    dataKey="average_latency_ms" 
                    stroke="#82ca9d"
                    name="Latency (ms)"
                    strokeWidth={2}
                  />
                  <Line 
                    yAxisId="right"
                    type="monotone" 
                    dataKey="throughput_per_sec" 
                    stroke="#ffc658"
                    name="Throughput/sec"
                    strokeWidth={2}
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </Grid>

        {/* Data Information */}
        <Grid item xs={12}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Data Information
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Time Range: {trendData?.time_range} ({trendData?.data_points_count} data points)
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Interval: {Math.floor((trendData?.interval_ms || 0) / 1000)} seconds
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Period: {new Date(trendData?.start_time).toLocaleString()} - {new Date(trendData?.end_time).toLocaleString()}
              </Typography>
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  );
};

export default PerformanceTrendAnalysis;
