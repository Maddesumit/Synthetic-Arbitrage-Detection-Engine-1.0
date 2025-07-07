import React, { useState, useEffect, useCallback } from 'react';
import {
  Box,
  Grid,
  Card,
  CardContent,
  Typography,
  Paper,
  Chip,
  Alert,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  LinearProgress
} from '@mui/material';
import {
  ResponsiveContainer,
  LineChart,
  Line,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  AreaChart,
  Area
} from 'recharts';

const LatencyTrackingInterface = ({ autoRefresh, refreshInterval }) => {
  const [latencyMetrics, setLatencyMetrics] = useState({
    average_latency_ms: 0,
    p50_latency_ms: 0,
    p95_latency_ms: 0,
    p99_latency_ms: 0,
    p999_latency_ms: 0,
    min_latency_ms: 0,
    max_latency_ms: 0,
    sla_compliance_pct: 0,
    target_latency_ms: 10,
    histogram: []
  });

  const [historicalLatency, setHistoricalLatency] = useState([]);
  const [loading, setLoading] = useState(false);

  const fetchLatencyMetrics = useCallback(async () => {
    try {
      setLoading(true);
      const response = await fetch('/api/performance/latency-metrics');
      const data = await response.json();
      
      setLatencyMetrics(data);
      
      // Add to historical data (keep last 50 points)
      setHistoricalLatency(prev => {
        const newPoint = {
          time: new Date(data.timestamp).toLocaleTimeString(),
          avg: data.average_latency_ms,
          p50: data.p50_latency_ms,
          p95: data.p95_latency_ms,
          p99: data.p99_latency_ms,
          p999: data.p999_latency_ms
        };
        return [...prev.slice(-49), newPoint];
      });
    } catch (error) {
      console.error('Error fetching latency metrics:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchLatencyMetrics();
    
    if (autoRefresh) {
      const interval = setInterval(fetchLatencyMetrics, refreshInterval * 1000);
      return () => clearInterval(interval);
    }
  }, [fetchLatencyMetrics, autoRefresh, refreshInterval]);

  const getLatencyColor = (latency, target) => {
    if (latency < target * 0.5) return 'success';
    if (latency < target) return 'warning';
    return 'error';
  };

  const getSLAColor = (compliance) => {
    if (compliance >= 99) return 'success';
    if (compliance >= 95) return 'warning';
    return 'error';
  };

  const percentileData = [
    { name: 'P50', value: latencyMetrics.p50_latency_ms, target: latencyMetrics.target_latency_ms * 0.5 },
    { name: 'P95', value: latencyMetrics.p95_latency_ms, target: latencyMetrics.target_latency_ms * 0.8 },
    { name: 'P99', value: latencyMetrics.p99_latency_ms, target: latencyMetrics.target_latency_ms },
    { name: 'P99.9', value: latencyMetrics.p999_latency_ms, target: latencyMetrics.target_latency_ms * 1.5 }
  ];

  return (
    <Box>
      {/* SLA Compliance Alert */}
      {latencyMetrics.sla_compliance_pct < 95 && (
        <Alert severity="error" sx={{ mb: 3 }}>
          SLA Compliance below threshold! Current: {latencyMetrics.sla_compliance_pct.toFixed(1)}% (Target: 95%+)
        </Alert>
      )}

      {latencyMetrics.average_latency_ms > latencyMetrics.target_latency_ms && (
        <Alert severity="warning" sx={{ mb: 3 }}>
          Average latency exceeds target! Current: {latencyMetrics.average_latency_ms.toFixed(2)}ms (Target: {latencyMetrics.target_latency_ms}ms)
        </Alert>
      )}

      <Grid container spacing={3}>
        {/* Latency Summary Cards */}
        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Average Latency
              </Typography>
              <Typography 
                variant="h4" 
                color={getLatencyColor(latencyMetrics.average_latency_ms, latencyMetrics.target_latency_ms)}
              >
                {latencyMetrics.average_latency_ms.toFixed(2)}ms
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Target: {latencyMetrics.target_latency_ms}ms
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                P99 Latency
              </Typography>
              <Typography 
                variant="h4" 
                color={getLatencyColor(latencyMetrics.p99_latency_ms, latencyMetrics.target_latency_ms)}
              >
                {latencyMetrics.p99_latency_ms.toFixed(2)}ms
              </Typography>
              <Typography variant="body2" color="text.secondary">
                99th percentile
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                SLA Compliance
              </Typography>
              <Typography 
                variant="h4" 
                color={getSLAColor(latencyMetrics.sla_compliance_pct)}
              >
                {latencyMetrics.sla_compliance_pct.toFixed(1)}%
              </Typography>
              <LinearProgress 
                variant="determinate" 
                value={latencyMetrics.sla_compliance_pct} 
                color={getSLAColor(latencyMetrics.sla_compliance_pct)}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Latency Range
              </Typography>
              <Typography variant="body1">
                Min: {latencyMetrics.min_latency_ms.toFixed(2)}ms
              </Typography>
              <Typography variant="body1">
                Max: {latencyMetrics.max_latency_ms.toFixed(2)}ms
              </Typography>
              <Typography variant="body2" color="text.secondary">
                {latencyMetrics.measurements_count?.toLocaleString() || '0'} measurements
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Real-time Latency Trends */}
        <Grid item xs={12}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Real-time Latency Trends (Nanosecond Precision)
            </Typography>
            <ResponsiveContainer width="100%" height={300}>
              <LineChart data={historicalLatency}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="time" />
                <YAxis domain={[0, 'dataMax + 2']} label={{ value: 'Latency (ms)', angle: -90, position: 'insideLeft' }} />
                <Tooltip formatter={(value) => [`${value.toFixed(2)}ms`, '']} />
                <Legend />
                <Line 
                  type="monotone" 
                  dataKey="avg" 
                  stroke="#8884d8" 
                  name="Average"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="p50" 
                  stroke="#82ca9d" 
                  name="P50"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="p95" 
                  stroke="#ffc658" 
                  name="P95"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="p99" 
                  stroke="#ff7300" 
                  name="P99"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="p999" 
                  stroke="#ff0000" 
                  name="P99.9"
                  strokeWidth={2}
                  dot={false}
                />
              </LineChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        {/* Latency Percentiles Bar Chart */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Latency Percentiles vs Targets
            </Typography>
            <ResponsiveContainer width="100%" height={250}>
              <BarChart data={percentileData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="name" />
                <YAxis label={{ value: 'Latency (ms)', angle: -90, position: 'insideLeft' }} />
                <Tooltip formatter={(value) => [`${value.toFixed(2)}ms`, '']} />
                <Legend />
                <Bar dataKey="value" fill="#8884d8" name="Actual" />
                <Bar dataKey="target" fill="#82ca9d" name="Target" />
              </BarChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        {/* Latency Distribution Histogram */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Latency Distribution Histogram
            </Typography>
            {latencyMetrics.histogram && latencyMetrics.histogram.length > 0 ? (
              <ResponsiveContainer width="100%" height={250}>
                <BarChart data={latencyMetrics.histogram}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="range" />
                  <YAxis label={{ value: 'Count', angle: -90, position: 'insideLeft' }} />
                  <Tooltip formatter={(value) => [`${value.toLocaleString()}`, 'Count']} />
                  <Bar dataKey="count" fill="#8884d8" />
                </BarChart>
              </ResponsiveContainer>
            ) : (
              <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 250 }}>
                <Typography color="text.secondary">No histogram data available</Typography>
              </Box>
            )}
          </Paper>
        </Grid>

        {/* Detailed Metrics Table */}
        <Grid item xs={12}>
          <TableContainer component={Paper}>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell>Metric</TableCell>
                  <TableCell align="right">Value</TableCell>
                  <TableCell align="right">Target</TableCell>
                  <TableCell align="center">Status</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                <TableRow>
                  <TableCell>Average Latency</TableCell>
                  <TableCell align="right">{latencyMetrics.average_latency_ms.toFixed(3)}ms</TableCell>
                  <TableCell align="right">{latencyMetrics.target_latency_ms}ms</TableCell>
                  <TableCell align="center">
                    <Chip 
                      label={latencyMetrics.average_latency_ms <= latencyMetrics.target_latency_ms ? 'OK' : 'HIGH'}
                      color={getLatencyColor(latencyMetrics.average_latency_ms, latencyMetrics.target_latency_ms)}
                      size="small"
                    />
                  </TableCell>
                </TableRow>
                <TableRow>
                  <TableCell>P50 Latency</TableCell>
                  <TableCell align="right">{latencyMetrics.p50_latency_ms.toFixed(3)}ms</TableCell>
                  <TableCell align="right">{(latencyMetrics.target_latency_ms * 0.5).toFixed(1)}ms</TableCell>
                  <TableCell align="center">
                    <Chip 
                      label={latencyMetrics.p50_latency_ms <= latencyMetrics.target_latency_ms * 0.5 ? 'OK' : 'HIGH'}
                      color={getLatencyColor(latencyMetrics.p50_latency_ms, latencyMetrics.target_latency_ms * 0.5)}
                      size="small"
                    />
                  </TableCell>
                </TableRow>
                <TableRow>
                  <TableCell>P95 Latency</TableCell>
                  <TableCell align="right">{latencyMetrics.p95_latency_ms.toFixed(3)}ms</TableCell>
                  <TableCell align="right">{(latencyMetrics.target_latency_ms * 0.8).toFixed(1)}ms</TableCell>
                  <TableCell align="center">
                    <Chip 
                      label={latencyMetrics.p95_latency_ms <= latencyMetrics.target_latency_ms * 0.8 ? 'OK' : 'HIGH'}
                      color={getLatencyColor(latencyMetrics.p95_latency_ms, latencyMetrics.target_latency_ms * 0.8)}
                      size="small"
                    />
                  </TableCell>
                </TableRow>
                <TableRow>
                  <TableCell>P99 Latency</TableCell>
                  <TableCell align="right">{latencyMetrics.p99_latency_ms.toFixed(3)}ms</TableCell>
                  <TableCell align="right">{latencyMetrics.target_latency_ms}ms</TableCell>
                  <TableCell align="center">
                    <Chip 
                      label={latencyMetrics.p99_latency_ms <= latencyMetrics.target_latency_ms ? 'OK' : 'HIGH'}
                      color={getLatencyColor(latencyMetrics.p99_latency_ms, latencyMetrics.target_latency_ms)}
                      size="small"
                    />
                  </TableCell>
                </TableRow>
                <TableRow>
                  <TableCell>P99.9 Latency</TableCell>
                  <TableCell align="right">{latencyMetrics.p999_latency_ms.toFixed(3)}ms</TableCell>
                  <TableCell align="right">{(latencyMetrics.target_latency_ms * 1.5).toFixed(1)}ms</TableCell>
                  <TableCell align="center">
                    <Chip 
                      label={latencyMetrics.p999_latency_ms <= latencyMetrics.target_latency_ms * 1.5 ? 'OK' : 'HIGH'}
                      color={getLatencyColor(latencyMetrics.p999_latency_ms, latencyMetrics.target_latency_ms * 1.5)}
                      size="small"
                    />
                  </TableCell>
                </TableRow>
              </TableBody>
            </Table>
          </TableContainer>
        </Grid>
      </Grid>
    </Box>
  );
};

export default LatencyTrackingInterface;