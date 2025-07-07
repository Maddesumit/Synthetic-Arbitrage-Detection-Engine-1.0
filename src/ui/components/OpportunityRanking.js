import React, { useState, useEffect } from 'react';
import { 
  Table, TableBody, TableCell, TableContainer, TableHead, TableRow, 
  Paper, Typography, Button, Chip, Box, TextField, MenuItem, Select, 
  FormControl, InputLabel
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import { ToggleButton, ToggleButtonGroup } from '@mui/material';
import ArrowUpwardIcon from '@mui/icons-material/ArrowUpward';
import ArrowDownwardIcon from '@mui/icons-material/ArrowDownward';
import TrendingUpIcon from '@mui/icons-material/TrendingUp';
import FilterListIcon from '@mui/icons-material/FilterList';
import EqualizerIcon from '@mui/icons-material/Equalizer';
import MonetizationOnIcon from '@mui/icons-material/MonetizationOn';

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

// Profit color scale function
const getProfitColor = (profit) => {
  if (profit > 0.01) return '#4caf50'; // High profit
  if (profit > 0.005) return '#8bc34a'; // Medium profit
  if (profit > 0.002) return '#cddc39'; // Low profit
  return '#ff9800'; // Marginal profit
};

// Risk color scale function
const getRiskColor = (risk) => {
  if (risk > 0.5) return '#f44336'; // High risk
  if (risk > 0.3) return '#ff9800'; // Medium risk
  if (risk > 0.1) return '#ffeb3b'; // Low risk
  return '#4caf50'; // Very low risk
};

// Confidence color scale function
const getConfidenceColor = (confidence) => {
  if (confidence > 0.9) return '#4caf50'; // High confidence
  if (confidence > 0.7) return '#8bc34a'; // Medium confidence
  if (confidence > 0.5) return '#ffeb3b'; // Low confidence
  return '#f44336'; // Very low confidence
};

// Format timestamp function
const formatTimestamp = (timestamp) => {
  const date = new Date(timestamp);
  return date.toLocaleTimeString();
};

const OpportunityRanking = () => {
  // State for opportunities data
  const [opportunities, setOpportunities] = useState([]);
  const [filteredOpportunities, setFilteredOpportunities] = useState([]);
  
  // State for sorting and filtering
  const [sortBy, setSortBy] = useState('expected_profit_pct');
  const [sortDirection, setSortDirection] = useState('desc');
  const [filters, setFilters] = useState({
    exchange: 'all',
    instrumentType: 'all',
    minProfit: 0.001,
    maxRisk: 0.5,
    minConfidence: 0.6
  });
  const [showFilters, setShowFilters] = useState(false);

  // Fetch opportunities data
  useEffect(() => {
    const fetchOpportunities = async () => {
      try {
        const response = await fetch('/api/opportunity-ranking');
        const data = await response.json();
        setOpportunities(data.opportunities);
        applyFiltersAndSort(data.opportunities);
      } catch (error) {
        console.error('Error fetching opportunities:', error);
      }
    };

    fetchOpportunities();
    
    // Set up WebSocket connection for real-time updates
    const ws = new WebSocket('ws://localhost:8080/ws/opportunities');
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      setOpportunities(data.opportunities);
      applyFiltersAndSort(data.opportunities);
    };
    
    return () => {
      ws.close();
    };
  }, []);

  // Apply filters and sorting
  const applyFiltersAndSort = (opportunitiesData) => {
    let filtered = opportunitiesData.filter(opp => {
      if (filters.exchange !== 'all' && 
          !(opp.legs.some(leg => leg.exchange.toLowerCase() === filters.exchange))) {
        return false;
      }
      
      if (filters.instrumentType !== 'all' && 
          !(opp.legs.some(leg => leg.type.toLowerCase() === filters.instrumentType))) {
        return false;
      }
      
      if (opp.expected_profit_pct < filters.minProfit) {
        return false;
      }
      
      if (opp.risk_score > filters.maxRisk) {
        return false;
      }
      
      if (opp.confidence < filters.minConfidence) {
        return false;
      }
      
      return true;
    });
    
    // Sort the filtered data
    filtered.sort((a, b) => {
      if (sortDirection === 'asc') {
        return a[sortBy] - b[sortBy];
      } else {
        return b[sortBy] - a[sortBy];
      }
    });
    
    setFilteredOpportunities(filtered);
  };

  // Update sorting
  const handleSortChange = (field) => {
    if (sortBy === field) {
      setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(field);
      setSortDirection('desc');
    }
  };

  // Update filters
  const handleFilterChange = (name, value) => {
    setFilters({
      ...filters,
      [name]: value
    });
  };

  // Apply filters on change
  useEffect(() => {
    applyFiltersAndSort(opportunities);
  }, [filters, sortBy, sortDirection]);

  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ padding: 2, backgroundColor: '#121212', borderRadius: 1, minHeight: '100vh' }}>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h5" component="h1" sx={{ color: '#fff', display: 'flex', alignItems: 'center' }}>
            <EqualizerIcon sx={{ mr: 1 }} /> Opportunity Ranking
          </Typography>
          
          <Box>
            <Button 
              variant="outlined" 
              startIcon={<FilterListIcon />}
              onClick={() => setShowFilters(!showFilters)}
              sx={{ mr: 1 }}
            >
              Filters
            </Button>
            
            <ToggleButtonGroup
              value={sortBy}
              exclusive
              onChange={(e, value) => value && handleSortChange(value)}
              aria-label="sort by"
              size="small"
            >
              <ToggleButton value="expected_profit_pct" aria-label="profit">
                <MonetizationOnIcon />
              </ToggleButton>
              <ToggleButton value="risk_score" aria-label="risk">
                <TrendingUpIcon />
              </ToggleButton>
              <ToggleButton value="confidence" aria-label="confidence">
                <EqualizerIcon />
              </ToggleButton>
            </ToggleButtonGroup>
            
            <ToggleButton
              value={sortDirection}
              selected={true}
              onChange={() => setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc')}
              aria-label="sort direction"
              size="small"
              sx={{ ml: 1 }}
            >
              {sortDirection === 'asc' ? <ArrowUpwardIcon /> : <ArrowDownwardIcon />}
            </ToggleButton>
          </Box>
        </Box>
        
        {/* Filters Panel */}
        {showFilters && (
          <Box sx={{ mb: 2, p: 2, backgroundColor: '#1e1e1e', borderRadius: 1 }}>
            <Typography variant="subtitle1" gutterBottom>
              Filter Options
            </Typography>
            
            <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 2 }}>
              <FormControl sx={{ minWidth: 150 }}>
                <InputLabel>Exchange</InputLabel>
                <Select
                  value={filters.exchange}
                  label="Exchange"
                  onChange={(e) => handleFilterChange('exchange', e.target.value)}
                  size="small"
                >
                  <MenuItem value="all">All Exchanges</MenuItem>
                  <MenuItem value="binance">Binance</MenuItem>
                  <MenuItem value="okx">OKX</MenuItem>
                  <MenuItem value="bybit">Bybit</MenuItem>
                </Select>
              </FormControl>
              
              <FormControl sx={{ minWidth: 150 }}>
                <InputLabel>Instrument Type</InputLabel>
                <Select
                  value={filters.instrumentType}
                  label="Instrument Type"
                  onChange={(e) => handleFilterChange('instrumentType', e.target.value)}
                  size="small"
                >
                  <MenuItem value="all">All Types</MenuItem>
                  <MenuItem value="spot">Spot</MenuItem>
                  <MenuItem value="perpetual_swap">Perpetual</MenuItem>
                  <MenuItem value="futures">Futures</MenuItem>
                  <MenuItem value="options">Options</MenuItem>
                </Select>
              </FormControl>
              
              <TextField
                label="Min Profit %"
                type="number"
                value={filters.minProfit}
                onChange={(e) => handleFilterChange('minProfit', parseFloat(e.target.value))}
                inputProps={{ step: 0.001, min: 0 }}
                size="small"
                sx={{ width: 120 }}
              />
              
              <TextField
                label="Max Risk"
                type="number"
                value={filters.maxRisk}
                onChange={(e) => handleFilterChange('maxRisk', parseFloat(e.target.value))}
                inputProps={{ step: 0.05, min: 0, max: 1 }}
                size="small"
                sx={{ width: 120 }}
              />
              
              <TextField
                label="Min Confidence"
                type="number"
                value={filters.minConfidence}
                onChange={(e) => handleFilterChange('minConfidence', parseFloat(e.target.value))}
                inputProps={{ step: 0.05, min: 0, max: 1 }}
                size="small"
                sx={{ width: 120 }}
              />
            </Box>
          </Box>
        )}
        
        {/* Opportunities Table */}
        <TableContainer component={Paper} sx={{ maxHeight: 650 }}>
          <Table stickyHeader aria-label="opportunities table">
            <TableHead>
              <TableRow>
                <TableCell>Underlying</TableCell>
                <TableCell align="right">
                  <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', cursor: 'pointer' }}
                      onClick={() => handleSortChange('expected_profit_pct')}>
                    Profit %
                    {sortBy === 'expected_profit_pct' && (
                      sortDirection === 'asc' ? <ArrowUpwardIcon fontSize="small" /> : <ArrowDownwardIcon fontSize="small" />
                    )}
                  </Box>
                </TableCell>
                <TableCell align="right">
                  <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', cursor: 'pointer' }}
                      onClick={() => handleSortChange('risk_score')}>
                    Risk Score
                    {sortBy === 'risk_score' && (
                      sortDirection === 'asc' ? <ArrowUpwardIcon fontSize="small" /> : <ArrowDownwardIcon fontSize="small" />
                    )}
                  </Box>
                </TableCell>
                <TableCell align="right">
                  <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', cursor: 'pointer' }}
                      onClick={() => handleSortChange('confidence')}>
                    Confidence
                    {sortBy === 'confidence' && (
                      sortDirection === 'asc' ? <ArrowUpwardIcon fontSize="small" /> : <ArrowDownwardIcon fontSize="small" />
                    )}
                  </Box>
                </TableCell>
                <TableCell align="right">Required Capital</TableCell>
                <TableCell align="right">Detected At</TableCell>
                <TableCell align="center">Actions</TableCell>
              </TableRow>
            </TableHead>
            <TableBody>
              {filteredOpportunities.map((opportunity) => (
                <TableRow 
                  key={opportunity.id}
                  hover
                  sx={{ '&:hover': { backgroundColor: '#2c2c2c' } }}
                >
                  <TableCell component="th" scope="row">
                    <Box sx={{ display: 'flex', flexDirection: 'column' }}>
                      <Typography variant="body1">{opportunity.underlying_symbol}</Typography>
                      <Box sx={{ display: 'flex', gap: 0.5, mt: 0.5 }}>
                        {opportunity.legs.map((leg, index) => (
                          <Chip 
                            key={index} 
                            label={`${leg.exchange} ${leg.type}`} 
                            size="small" 
                            sx={{ fontSize: '0.7rem', height: 20 }}
                          />
                        ))}
                      </Box>
                    </Box>
                  </TableCell>
                  <TableCell align="right">
                    <Typography 
                      sx={{ 
                        color: getProfitColor(opportunity.expected_profit_pct),
                        fontWeight: 'bold' 
                      }}
                    >
                      {(opportunity.expected_profit_pct * 100).toFixed(4)}%
                    </Typography>
                  </TableCell>
                  <TableCell align="right">
                    <Typography 
                      sx={{ 
                        color: getRiskColor(opportunity.risk_score),
                        fontWeight: 'medium' 
                      }}
                    >
                      {(opportunity.risk_score * 100).toFixed(2)}%
                    </Typography>
                  </TableCell>
                  <TableCell align="right">
                    <Typography 
                      sx={{ 
                        color: getConfidenceColor(opportunity.confidence),
                        fontWeight: 'medium' 
                      }}
                    >
                      {(opportunity.confidence * 100).toFixed(1)}%
                    </Typography>
                  </TableCell>
                  <TableCell align="right">
                    ${opportunity.required_capital.toLocaleString()}
                  </TableCell>
                  <TableCell align="right">
                    {formatTimestamp(opportunity.detected_at)}
                  </TableCell>
                  <TableCell align="center">
                    <Button 
                      size="small" 
                      variant="contained" 
                      color="primary"
                      sx={{ mr: 1, fontSize: '0.7rem' }}
                      onClick={() => window.location.href = `/execution-planning/${opportunity.id}`}
                    >
                      Plan
                    </Button>
                    <Button 
                      size="small" 
                      variant="outlined"
                      sx={{ fontSize: '0.7rem' }}
                      onClick={() => window.location.href = `/paper-trading/${opportunity.id}`}
                    >
                      Simulate
                    </Button>
                  </TableCell>
                </TableRow>
              ))}
              
              {filteredOpportunities.length === 0 && (
                <TableRow>
                  <TableCell colSpan={7} align="center" sx={{ py: 3 }}>
                    <Typography variant="body1" color="text.secondary">
                      No opportunities match the current filters
                    </Typography>
                  </TableCell>
                </TableRow>
              )}
            </TableBody>
          </Table>
        </TableContainer>
        
        <Box sx={{ mt: 2, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Typography variant="body2" color="text.secondary">
            Showing {filteredOpportunities.length} of {opportunities.length} opportunities
          </Typography>
          
          <Typography variant="body2" color="text.secondary">
            Last updated: {new Date().toLocaleTimeString()}
          </Typography>
        </Box>
      </Box>
    </ThemeProvider>
  );
};

export default OpportunityRanking;
