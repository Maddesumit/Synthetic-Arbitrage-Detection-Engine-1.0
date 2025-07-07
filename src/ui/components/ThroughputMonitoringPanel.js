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
  LinearProgress,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow
} from '@mui/material';
import {
  ResponsiveContainer,
  LineChart,
  Line,
  AreaChart,
  Area,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  PieChart,
  Pie,
  Cell
} from 'recharts';

const ThroughputMonitoringPanel = ({ autoRefresh, refreshInterval }) => {
  const [throughputMetrics, setThroughputMetrics] = useState({
    current_throughput_per_sec: 0,
    peak_throughput_per_sec: 0,
    average_throughput_per_sec: 0,
    target_throughput_per_sec: 2000,
    throughput_efficiency_pct: 0,
    total_processed_today: 0,
    processing_errors_per_hour: 0,
    queue_backlog_size: 0,
    processing_threads: 16,
    active_connections: 3,
    breakdown_by_exchange: []
  });

  const [historicalThroughput, setHistoricalThroughput] = useState([]);
  const [loading, setLoading] = useState(false);

  const fetchThroughputMetrics = useCallback(async () => {
    try {
      setLoading(true);
      const response = await fetch('/api/performance/throughput-metrics');
      const data = await response.json();
      
      setThroughputMetrics(data);
      
      // Add to historical data (keep last 50 points)
      setHistoricalThroughput(prev => {
        const newPoint = {
          time: new Date(data.timestamp).toLocaleTimeString(),
          current: data.current_throughput_per_sec,
          average: data.average_throughput_per_sec,
          target: data.target_throughput_per_sec,
          efficiency: data.throughput_efficiency_pct
        };
        return [...prev.slice(-49), newPoint];
      });
    } catch (error) {
      console.error('Error fetching throughput metrics:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchThroughputMetrics();
    
    if (autoRefresh) {
      const interval = setInterval(fetchThroughputMetrics, refreshInterval * 1000);
      return () => clearInterval(interval);
    }
  }, [fetchThroughputMetrics, autoRefresh, refreshInterval]);

  const getThroughputColor = (current, target) => {
    const ratio = current / target;
    if (ratio >= 0.95) return 'success';
    if (ratio >= 0.8) return 'warning';
    return 'error';
  };

  const getEfficiencyColor = (efficiency) => {
    if (efficiency >= 95) return 'success';
    if (efficiency >= 80) return 'warning';
    return 'error';
  };

  const exchangeColors = ['#8884d8', '#82ca9d', '#ffc658', '#ff7300', '#ff0000'];

  return (
    <Box>
      {/* Performance Alerts */}
      {throughputMetrics.current_throughput_per_sec < throughputMetrics.target_throughput_per_sec * 0.8 && (
        <Alert severity="warning" sx={{ mb: 3 }}>
          Throughput below target! Current: {Math.round(throughputMetrics.current_throughput_per_sec)}/sec 
          (Target: {throughputMetrics.target_throughput_per_sec}/sec)
        </Alert>
      )}

      {throughputMetrics.queue_backlog_size > 1000 && (
        <Alert severity="error" sx={{ mb: 3 }}>
          High queue backlog detected! {throughputMetrics.queue_backlog_size.toLocaleString()} items pending
        </Alert>
      )}

      <Grid container spacing={3}>
        {/* Throughput Summary Cards */}
        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Current Throughput
              </Typography>
              <Typography 
                variant="h4" 
                color={getThroughputColor(throughputMetrics.current_throughput_per_sec, throughputMetrics.target_throughput_per_sec)}
              >
                {Math.round(throughputMetrics.current_throughput_per_sec)}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                per second (Target: {throughputMetrics.target_throughput_per_sec})
              </Typography>
              <LinearProgress 
                variant="determinate" 
                value={(throughputMetrics.current_throughput_per_sec / throughputMetrics.target_throughput_per_sec) * 100} 
                color={getThroughputColor(throughputMetrics.current_throughput_per_sec, throughputMetrics.target_throughput_per_sec)}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Peak Throughput
              </Typography>
              <Typography variant="h4" color="primary">
                {Math.round(throughputMetrics.peak_throughput_per_sec)}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                per second (Today's Peak)
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Efficiency
              </Typography>
              <Typography 
                variant="h4" 
                color={getEfficiencyColor(throughputMetrics.throughput_efficiency_pct)}
              >
                {throughputMetrics.throughput_efficiency_pct.toFixed(1)}%
              </Typography>
              <Typography variant="body2" color="text.secondary">
                vs Target Performance
              </Typography>
              <LinearProgress 
                variant="determinate" 
                value={throughputMetrics.throughput_efficiency_pct} 
                color={getEfficiencyColor(throughputMetrics.throughput_efficiency_pct)}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={3}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Total Processed
              </Typography>
              <Typography variant="h4" color="info">
                {(throughputMetrics.total_processed_today / 1000000).toFixed(1)}M
              </Typography>
              <Typography variant="body2" color="text.secondary">
                messages today
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Real-time Throughput Trends */}
        <Grid item xs={12}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Real-time Throughput Monitoring ({'>'}2000 updates/sec validation)
            </Typography>
            <ResponsiveContainer width="100%" height={300}>
              <AreaChart data={historicalThroughput}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="time" />
                <YAxis label={{ value: 'Throughput (per sec)', angle: -90, position: 'insideLeft' }} />
                <Tooltip formatter={(value) => [`${Math.round(value)}`, '']} />
                <Legend />
                <Area
                  type="monotone"
                  dataKey="current"
                  stroke="#8884d8"
                  fill="#8884d8"
                  fillOpacity={0.3}
                  name="Current Throughput"
                />
                <Line 
                  type="monotone" 
                  dataKey="target" 
                  stroke="#ff0000" 
                  strokeDasharray="5 5"
                  name="Target"
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="average" 
                  stroke="#82ca9d" 
                  name="Average"
                  dot={false}
                />
              </AreaChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        {/* Exchange Breakdown */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Throughput by Exchange
            </Typography>
            {throughputMetrics.breakdown_by_exchange && throughputMetrics.breakdown_by_exchange.length > 0 ? (
              <ResponsiveContainer width="100%" height={250}>
                <PieChart>
                  <Pie
                    data={throughputMetrics.breakdown_by_exchange.map((item, index) => ({
                      name: item.exchange,
                      value: item.throughput,
                      fill: exchangeColors[index % exchangeColors.length]
                    }))}
                    cx="50%"
                    cy="50%"
                    innerRadius={40}
                    outerRadius={80}
                    paddingAngle={5}
                    dataKey="value"
                    label={({ name, value }) => `${name}: ${Math.round(value)}`}
                  >
                    {throughputMetrics.breakdown_by_exchange.map((entry, index) => (
                      <Cell key={`cell-${index}`} fill={exchangeColors[index % exchangeColors.length]} />
                    ))}
                  </Pie>
                  <Tooltip formatter={(value) => [`${Math.round(value)}/sec`, '']} />
                  <Legend />
                </PieChart>
              </ResponsiveContainer>
            ) : (
              <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 250 }}>
                <Typography color="text.secondary">No exchange data available</Typography>
              </Box>
            )}
          </Paper>
        </Grid>

        {/* System Status */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Processing System Status
            </Typography>
            <Grid container spacing={2}>
              <Grid item xs={6}>
                <Card variant="outlined">
                  <CardContent sx={{ textAlign: 'center', py: 2 }}>
                    <Typography variant="h4" color="primary">
                      {throughputMetrics.processing_threads}
                    </Typography>
                    <Typography variant="body2" color="text.secondary">
                      Processing Threads
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              <Grid item xs={6}>
                <Card variant="outlined">
                  <CardContent sx={{ textAlign: 'center', py: 2 }}>
                    <Typography variant="h4" color="success.main">
                      {throughputMetrics.active_connections}
                    </Typography>
                    <Typography variant="body2" color="text.secondary">
                      Active Connections
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              <Grid item xs={6}>
                <Card variant="outlined">
                  <CardContent sx={{ textAlign: 'center', py: 2 }}>
                    <Typography 
                      variant="h4" 
                      color={throughputMetrics.queue_backlog_size > 500 ? 'error' : 'info'}
                    >
                      {throughputMetrics.queue_backlog_size.toLocaleString()}
                    </Typography>
                    <Typography variant="body2" color="text.secondary">
                      Queue Backlog
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              <Grid item xs={6}>
                <Card variant="outlined">
                  <CardContent sx={{ textAlign: 'center', py: 2 }}>
                    <Typography 
                      variant="h4" 
                      color={throughputMetrics.processing_errors_per_hour > 10 ? 'error' : 'success'}
                    >
                      {throughputMetrics.processing_errors_per_hour}
                    </Typography>
                    <Typography variant="body2" color="text.secondary">
                      Errors/Hour
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
            </Grid>
          </Paper>
        </Grid>

        {/* Exchange Details Table */}
        <Grid item xs={12}>
          <TableContainer component={Paper}>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell>Exchange</TableCell>
                  <TableCell align="right">Throughput (per sec)</TableCell>
                  <TableCell align="right">Percentage</TableCell>
                  <TableCell align="center">Status</TableCell>
                  <TableCell align="center">Health</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {throughputMetrics.breakdown_by_exchange && throughputMetrics.breakdown_by_exchange.map((exchange, index) => {
                  const percentage = (exchange.throughput / throughputMetrics.current_throughput_per_sec) * 100;
                  return (
                    <TableRow key={exchange.exchange}>
                      <TableCell>{exchange.exchange}</TableCell>
                      <TableCell align="right">{Math.round(exchange.throughput)}</TableCell>
                      <TableCell align="right">{percentage.toFixed(1)}%</TableCell>
                      <TableCell align="center">
                        <Chip 
                          label={exchange.status?.toUpperCase() || 'UNKNOWN'}
                          color={exchange.status === 'healthy' ? 'success' : 'error'}
                          size="small"
                        />
                      </TableCell>
                      <TableCell align="center">
                        <LinearProgress 
                          variant="determinate" 
                          value={percentage} 
                          color="primary"
                          sx={{ width: '100px' }}
                        />
                      </TableCell>
                    </TableRow>
                  );
                })}
              </TableBody>
            </Table>
          </TableContainer>
        </Grid>
      </Grid>
    </Box>
  );
};

export default ThroughputMonitoringPanel;
