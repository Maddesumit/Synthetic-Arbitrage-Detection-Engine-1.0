import React, { useState } from 'react';
import { 
  Box, Typography, Paper, Tabs, Tab, AppBar, CssBaseline,
  Container, Drawer, Divider, List, ListItem, ListItemIcon, 
  ListItemText, IconButton
} from '@mui/material';
import { createTheme, ThemeProvider } from '@mui/material/styles';
import MenuIcon from '@mui/icons-material/Menu';
import DashboardIcon from '@mui/icons-material/Dashboard';
import TrendingUpIcon from '@mui/icons-material/TrendingUp';
import SettingsIcon from '@mui/icons-material/Settings';
import AccountBalanceWalletIcon from '@mui/icons-material/AccountBalanceWallet';
import TimelineIcon from '@mui/icons-material/Timeline';
import PlayCircleFilledIcon from '@mui/icons-material/PlayCircleFilled';
import ShowChartIcon from '@mui/icons-material/ShowChart';
import ChevronLeftIcon from '@mui/icons-material/ChevronLeft';
import ChevronRightIcon from '@mui/icons-material/ChevronRight';

// Import Phase 6 components
import OpportunityRanking from './OpportunityRanking';
import ExecutionPlanner from './ExecutionPlanner';
import PnLTracker from './PnLTracker';
import PaperTradingInterface from './PaperTradingInterface';
import TradeExecutionVisualization from './TradeExecutionVisualization';

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

// Tab panel component
function TabPanel(props) {
  const { children, value, index, ...other } = props;

  return (
    <div
      role="tabpanel"
      hidden={value !== index}
      id={`phase6-tabpanel-${index}`}
      aria-labelledby={`phase6-tab-${index}`}
      {...other}
    >
      {value === index && (
        <Box sx={{ p: 2 }}>
          {children}
        </Box>
      )}
    </div>
  );
}

// Dashboard component for Phase 6
const Phase6Dashboard = () => {
  const [activeTab, setActiveTab] = useState(0);
  const [drawerOpen, setDrawerOpen] = useState(true);
  const [selectedOpportunityId, setSelectedOpportunityId] = useState(null);
  const [selectedExecutionPlanId, setSelectedExecutionPlanId] = useState(null);

  // Handle tab change
  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
  };

  // Handle opportunity selection for execution planning
  const handleOpportunitySelect = (opportunityId) => {
    setSelectedOpportunityId(opportunityId);
    setActiveTab(1); // Switch to execution planner tab
  };

  // Handle execution plan selection for visualization
  const handleExecutionPlanSelect = (executionPlanId) => {
    setSelectedExecutionPlanId(executionPlanId);
    setActiveTab(3); // Switch to execution visualization tab
  };

  // Toggle drawer
  const toggleDrawer = () => {
    setDrawerOpen(!drawerOpen);
  };

  // Drawer width
  const drawerWidth = 240;

  return (
    <ThemeProvider theme={darkTheme}>
      <Box sx={{ display: 'flex' }}>
        <CssBaseline />
        
        {/* Top app bar */}
        <AppBar 
          position="fixed" 
          sx={{ 
            zIndex: (theme) => theme.zIndex.drawer + 1,
            bgcolor: 'background.paper',
            borderBottom: '1px solid rgba(255, 255, 255, 0.12)'
          }}
        >
          <Box sx={{ display: 'flex', alignItems: 'center', px: 2, py: 1 }}>
            <IconButton
              color="inherit"
              aria-label="toggle drawer"
              onClick={toggleDrawer}
              edge="start"
              sx={{ mr: 2 }}
            >
              {drawerOpen ? <ChevronLeftIcon /> : <MenuIcon />}
            </IconButton>
            <Typography variant="h6" noWrap component="div" sx={{ flexGrow: 1 }}>
              Synthetic Arbitrage Detection Engine - Phase 6
            </Typography>
            <Tabs 
              value={activeTab} 
              onChange={handleTabChange}
              textColor="primary"
              indicatorColor="primary"
              sx={{ mr: 2 }}
            >
              <Tab label="Opportunities" icon={<TrendingUpIcon />} iconPosition="start" />
              <Tab label="Execution Planner" icon={<TimelineIcon />} iconPosition="start" />
              <Tab label="P&L Tracker" icon={<AccountBalanceWalletIcon />} iconPosition="start" />
              <Tab label="Execution Visualization" icon={<ShowChartIcon />} iconPosition="start" />
              <Tab label="Paper Trading" icon={<PlayCircleFilledIcon />} iconPosition="start" />
            </Tabs>
            <IconButton color="inherit">
              <SettingsIcon />
            </IconButton>
          </Box>
        </AppBar>
        
        {/* Side drawer */}
        <Drawer
          variant="persistent"
          anchor="left"
          open={drawerOpen}
          sx={{
            width: drawerWidth,
            flexShrink: 0,
            '& .MuiDrawer-paper': {
              width: drawerWidth,
              boxSizing: 'border-box',
              top: '64px',
              height: 'calc(100% - 64px)'
            },
          }}
        >
          <List>
            <ListItem button onClick={() => setActiveTab(0)}>
              <ListItemIcon>
                <TrendingUpIcon color={activeTab === 0 ? 'primary' : 'inherit'} />
              </ListItemIcon>
              <ListItemText primary="Opportunity Ranking" />
            </ListItem>
            <ListItem button onClick={() => setActiveTab(1)} disabled={!selectedOpportunityId}>
              <ListItemIcon>
                <TimelineIcon color={activeTab === 1 ? 'primary' : 'inherit'} />
              </ListItemIcon>
              <ListItemText primary="Execution Planner" />
            </ListItem>
            <ListItem button onClick={() => setActiveTab(2)}>
              <ListItemIcon>
                <AccountBalanceWalletIcon color={activeTab === 2 ? 'primary' : 'inherit'} />
              </ListItemIcon>
              <ListItemText primary="P&L Tracker" />
            </ListItem>
            <ListItem button onClick={() => setActiveTab(3)} disabled={!selectedExecutionPlanId}>
              <ListItemIcon>
                <ShowChartIcon color={activeTab === 3 ? 'primary' : 'inherit'} />
              </ListItemIcon>
              <ListItemText primary="Execution Visualization" />
            </ListItem>
            <ListItem button onClick={() => setActiveTab(4)}>
              <ListItemIcon>
                <PlayCircleFilledIcon color={activeTab === 4 ? 'primary' : 'inherit'} />
              </ListItemIcon>
              <ListItemText primary="Paper Trading" />
            </ListItem>
          </List>
          <Divider />
          <List>
            <ListItem button>
              <ListItemIcon>
                <DashboardIcon />
              </ListItemIcon>
              <ListItemText primary="Main Dashboard" />
            </ListItem>
            <ListItem button>
              <ListItemIcon>
                <SettingsIcon />
              </ListItemIcon>
              <ListItemText primary="Settings" />
            </ListItem>
          </List>
        </Drawer>
        
        {/* Main content */}
        <Box
          component="main"
          sx={{
            flexGrow: 1,
            pt: 10,
            pl: drawerOpen ? `${drawerWidth}px` : 0,
            transition: theme => theme.transitions.create('padding', {
              easing: theme.transitions.easing.sharp,
              duration: theme.transitions.duration.leavingScreen,
            }),
          }}
        >
          <Container maxWidth="xl">
            {/* Tab panels */}
            <TabPanel value={activeTab} index={0}>
              <OpportunityRanking onOpportunitySelect={handleOpportunitySelect} />
            </TabPanel>
            
            <TabPanel value={activeTab} index={1}>
              {selectedOpportunityId ? (
                <ExecutionPlanner 
                  opportunityId={selectedOpportunityId} 
                  onExecutionPlanCreated={handleExecutionPlanSelect}
                />
              ) : (
                <Paper sx={{ p: 3, textAlign: 'center' }}>
                  <Typography variant="h5" gutterBottom>
                    No Opportunity Selected
                  </Typography>
                  <Typography variant="body1">
                    Please select an opportunity from the ranking table to create an execution plan.
                  </Typography>
                </Paper>
              )}
            </TabPanel>
            
            <TabPanel value={activeTab} index={2}>
              <PnLTracker />
            </TabPanel>
            
            <TabPanel value={activeTab} index={3}>
              {selectedExecutionPlanId ? (
                <TradeExecutionVisualization 
                  opportunityId={selectedOpportunityId}
                  executionPlanId={selectedExecutionPlanId} 
                />
              ) : (
                <Paper sx={{ p: 3, textAlign: 'center' }}>
                  <Typography variant="h5" gutterBottom>
                    No Execution Plan Selected
                  </Typography>
                  <Typography variant="body1">
                    Please create an execution plan first to visualize the execution process.
                  </Typography>
                </Paper>
              )}
            </TabPanel>
            
            <TabPanel value={activeTab} index={4}>
              <PaperTradingInterface />
            </TabPanel>
          </Container>
        </Box>
      </Box>
    </ThemeProvider>
  );
};

export default Phase6Dashboard;
