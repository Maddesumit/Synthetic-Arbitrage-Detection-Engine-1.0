import React, { useState, useEffect, useCallback } from 'react';
import {
  Box,
  Grid,
  Card,
  CardContent,
  Typography,
  LinearProgress,
  Paper,
  Chip,
  Alert
} from '@mui/material';
import {
  ResponsiveContainer,
  LineChart,
  Line,
  AreaChart,
  Area,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  PieChart,
  Pie,
  Cell
} from 'recharts';

const SystemMetricsDashboard = ({ autoRefresh, refreshInterval }) => {
  const [systemMetrics, setSystemMetrics] = useState({
    cpu_usage_pct: 0,
    memory_usage_pct: 0,
    network_utilization_pct: 0,
    disk_usage_pct: 0,
    cpu_cores: 8,
    total_memory_gb: 32,
    available_memory_gb: 16,
    timestamp: Date.now()
  });

  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(false);

  const fetchSystemMetrics = useCallback(async () => {
    try {
      setLoading(true);
      const response = await fetch('/api/performance/system-metrics');
      const data = await response.json();
      
      setSystemMetrics(data);
      
      // Add to historical data (keep last 50 points)
      setHistoricalData(prev => {
        const newPoint = {
          time: new Date(data.timestamp).toLocaleTimeString(),
          cpu: data.cpu_usage_pct,
          memory: data.memory_usage_pct,
          network: data.network_utilization_pct,
          disk: data.disk_usage_pct
        };
        return [...prev.slice(-49), newPoint];
      });
    } catch (error) {
      console.error('Error fetching system metrics:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchSystemMetrics();
    
    if (autoRefresh) {
      const interval = setInterval(fetchSystemMetrics, refreshInterval * 1000);
      return () => clearInterval(interval);
    }
  }, [fetchSystemMetrics, autoRefresh, refreshInterval]);

  const getUsageColor = (percentage) => {
    if (percentage < 50) return 'success';
    if (percentage < 80) return 'warning';
    return 'error';
  };

  const getProgressColor = (percentage) => {
    if (percentage < 50) return 'success';
    if (percentage < 80) return 'warning';
    return 'error';
  };

  const pieData = [
    { name: 'Used CPU', value: systemMetrics.cpu_usage_pct, fill: '#8884d8' },
    { name: 'Available CPU', value: 100 - systemMetrics.cpu_usage_pct, fill: '#e0e0e0' }
  ];

  const memoryData = [
    { name: 'Used Memory', value: systemMetrics.memory_usage_pct, fill: '#82ca9d' },
    { name: 'Available Memory', value: 100 - systemMetrics.memory_usage_pct, fill: '#e0e0e0' }
  ];

  return (
    <Box>
      {/* Alert for high resource usage */}
      {(systemMetrics.cpu_usage_pct > 80 || systemMetrics.memory_usage_pct > 80) && (
        <Alert severity="warning" sx={{ mb: 3 }}>
          High resource usage detected! CPU: {systemMetrics.cpu_usage_pct.toFixed(1)}%, 
          Memory: {systemMetrics.memory_usage_pct.toFixed(1)}%
        </Alert>
      )}

      <Grid container spacing={3}>
        {/* Resource Usage Cards */}
        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                CPU Usage
              </Typography>
              <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
                <Box sx={{ width: '100%', mr: 1 }}>
                  <LinearProgress 
                    variant="determinate" 
                    value={systemMetrics.cpu_usage_pct} 
                    color={getProgressColor(systemMetrics.cpu_usage_pct)}
                    sx={{ height: 10, borderRadius: 5 }}
                  />
                </Box>
                <Chip 
                  label={`${systemMetrics.cpu_usage_pct.toFixed(1)}%`}
                  color={getUsageColor(systemMetrics.cpu_usage_pct)}
                  size="small"
                />
              </Box>
              <Typography variant="body2" color="text.secondary">
                {systemMetrics.cpu_cores} cores available
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Memory Usage
              </Typography>
              <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
                <Box sx={{ width: '100%', mr: 1 }}>
                  <LinearProgress 
                    variant="determinate" 
                    value={systemMetrics.memory_usage_pct} 
                    color={getProgressColor(systemMetrics.memory_usage_pct)}
                    sx={{ height: 10, borderRadius: 5 }}
                  />
                </Box>
                <Chip 
                  label={`${systemMetrics.memory_usage_pct.toFixed(1)}%`}
                  color={getUsageColor(systemMetrics.memory_usage_pct)}
                  size="small"
                />
              </Box>
              <Typography variant="body2" color="text.secondary">
                {systemMetrics.available_memory_gb.toFixed(1)}GB / {systemMetrics.total_memory_gb}GB available
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Network Utilization
              </Typography>
              <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
                <Box sx={{ width: '100%', mr: 1 }}>
                  <LinearProgress 
                    variant="determinate" 
                    value={systemMetrics.network_utilization_pct} 
                    color={getProgressColor(systemMetrics.network_utilization_pct)}
                    sx={{ height: 10, borderRadius: 5 }}
                  />
                </Box>
                <Chip 
                  label={`${systemMetrics.network_utilization_pct.toFixed(1)}%`}
                  color={getUsageColor(systemMetrics.network_utilization_pct)}
                  size="small"
                />
              </Box>
              <Typography variant="body2" color="text.secondary">
                1 Gbps bandwidth
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={6}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Disk Usage
              </Typography>
              <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
                <Box sx={{ width: '100%', mr: 1 }}>
                  <LinearProgress 
                    variant="determinate" 
                    value={systemMetrics.disk_usage_pct} 
                    color={getProgressColor(systemMetrics.disk_usage_pct)}
                    sx={{ height: 10, borderRadius: 5 }}
                  />
                </Box>
                <Chip 
                  label={`${systemMetrics.disk_usage_pct.toFixed(1)}%`}
                  color={getUsageColor(systemMetrics.disk_usage_pct)}
                  size="small"
                />
              </Box>
              <Typography variant="body2" color="text.secondary">
                {systemMetrics.disk_free_gb?.toFixed(1) || '256'}GB / 512GB available
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        {/* Historical Trends Chart */}
        <Grid item xs={12}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Real-time System Metrics Trends
            </Typography>
            <ResponsiveContainer width="100%" height={300}>
              <LineChart data={historicalData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="time" />
                <YAxis domain={[0, 100]} />
                <Tooltip formatter={(value) => [`${value.toFixed(1)}%`, '']} />
                <Legend />
                <Line 
                  type="monotone" 
                  dataKey="cpu" 
                  stroke="#8884d8" 
                  name="CPU Usage"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="memory" 
                  stroke="#82ca9d" 
                  name="Memory Usage"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="network" 
                  stroke="#ffc658" 
                  name="Network Usage"
                  strokeWidth={2}
                  dot={false}
                />
                <Line 
                  type="monotone" 
                  dataKey="disk" 
                  stroke="#ff7300" 
                  name="Disk Usage"
                  strokeWidth={2}
                  dot={false}
                />
              </LineChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        {/* Resource Distribution Pie Charts */}
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              CPU Utilization Distribution
            </Typography>
            <ResponsiveContainer width="100%" height={200}>
              <PieChart>
                <Pie
                  data={pieData}
                  cx="50%"
                  cy="50%"
                  innerRadius={40}
                  outerRadius={80}
                  paddingAngle={5}
                  dataKey="value"
                >
                  {pieData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.fill} />
                  ))}
                </Pie>
                <Tooltip formatter={(value) => `${value.toFixed(1)}%`} />
                <Legend />
              </PieChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>

        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Memory Utilization Distribution
            </Typography>
            <ResponsiveContainer width="100%" height={200}>
              <PieChart>
                <Pie
                  data={memoryData}
                  cx="50%"
                  cy="50%"
                  innerRadius={40}
                  outerRadius={80}
                  paddingAngle={5}
                  dataKey="value"
                >
                  {memoryData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.fill} />
                  ))}
                </Pie>
                <Tooltip formatter={(value) => `${value.toFixed(1)}%`} />
                <Legend />
              </PieChart>
            </ResponsiveContainer>
          </Paper>
        </Grid>
      </Grid>
    </Box>
  );
};

export default SystemMetricsDashboard;
