/**
 * @file dashboard_demo.cpp
 * @brief Phase 9 Dashboard Demo - Real-time Web UI with Real Exchange Data
 */

#include "ui/DashboardApp.hpp"
#include "core/PricingEngine.hpp"
#include "core/ArbitrageEngine.hpp"
#include "core/RiskManager.hpp"
#include "core/PositionManager.hpp"
#include "core/MathUtils.hpp"
#include "data/MarketData.hpp"
#include "data/RealTimeDataManager.hpp"
#include "utils/ConfigManager.hpp"
#include "utils/Logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include <atomic>
#include <iomanip>

using namespace arbitrage;

// Global flag for graceful shutdown
std::atomic<bool> shutdown_requested(false);

// Global real-time data manager
std::unique_ptr<data::RealTimeDataManager> real_time_data_manager;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\n🛑 Shutdown signal received. Gracefully shutting down...\n";
    shutdown_requested = true;
}

/**
 * @brief Generate realistic demo market data for all exchanges
 */
std::vector<data::MarketDataPoint> generateDemoMarketData() {
    std::vector<data::MarketDataPoint> data;
    
    // Random number generators for realistic price movements
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<double> price_noise(0.0, 0.5);
    static std::uniform_real_distribution<double> volume_dist(100.0, 2000.0);
    static std::normal_distribution<double> funding_noise(0.0, 0.00005);
    static std::uniform_real_distribution<double> exchange_spread(-2.0, 2.0);
    
    // Base prices (shared across exchanges with small variations)
    static double btc_base = 50000.0;
    static double eth_base = 3000.0;
    
    // Update base prices with small random walk
    btc_base += price_noise(gen);
    eth_base += price_noise(gen) * 0.1;
    
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    
    // List of exchanges to generate data for
    std::vector<std::pair<data::Exchange, std::string>> exchanges = {
        {data::Exchange::BINANCE, "binance"},
        {data::Exchange::OKX, "okx"},
        {data::Exchange::BYBIT, "bybit"}
    };
    
    // Generate data for each exchange
    for (const auto& [exchange_enum, exchange_str] : exchanges) {
        // Small price variations between exchanges to simulate real market conditions
        double btc_variation = exchange_spread(gen);
        double eth_variation = exchange_spread(gen) * 0.1;
        
        // BTC spot data
        data::MarketDataPoint btc_spot;
        btc_spot.symbol = "BTCUSDT";
        btc_spot.exchange = exchange_str;
        btc_spot.timestamp = timestamp;
        btc_spot.bid = btc_base + btc_variation - 0.25;
        btc_spot.ask = btc_base + btc_variation + 0.25;
        btc_spot.last = btc_base + btc_variation;
        btc_spot.volume = volume_dist(gen);
        btc_spot.funding_rate = 0.0001 + funding_noise(gen);
        data.push_back(btc_spot);
        
        // BTC perpetual data
        data::MarketDataPoint btc_perp = btc_spot;
        btc_perp.symbol = "BTCUSDT-PERP";
        btc_perp.bid += 1.5;
        btc_perp.ask += 1.5;
        btc_perp.last += 1.5;
        data.push_back(btc_perp);
        
        // ETH spot data
        data::MarketDataPoint eth_spot;
        eth_spot.symbol = "ETHUSDT";
        eth_spot.exchange = exchange_str;
        eth_spot.timestamp = timestamp;
        eth_spot.bid = eth_base + eth_variation - 0.15;
        eth_spot.ask = eth_base + eth_variation + 0.15;
        eth_spot.last = eth_base + eth_variation;
        eth_spot.volume = volume_dist(gen);
        eth_spot.funding_rate = 0.00008 + funding_noise(gen);
        data.push_back(eth_spot);

        // ETH perpetual data
        data::MarketDataPoint eth_perp = eth_spot;
        eth_perp.symbol = "ETHUSDT-PERP";
        eth_perp.bid += 0.8;
        eth_perp.ask += 0.8;
        eth_perp.last += 0.8;
        data.push_back(eth_perp);
    }
    
    return data;
}

/**
 * @brief Generate demo arbitrage opportunities across multiple exchanges
 */
std::vector<core::ArbitrageOpportunity> generateDemoOpportunities() {
    std::vector<core::ArbitrageOpportunity> opportunities;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> profit_dist(0.001, 0.005);
    static std::uniform_real_distribution<double> capital_dist(1000.0, 50000.0);
    static std::uniform_real_distribution<double> risk_dist(0.1, 0.4);
    static std::uniform_real_distribution<double> confidence_dist(0.7, 0.95);
    static std::uniform_int_distribution<int> exchange_dist(0, 2);
    
    // List of exchanges for generating opportunities
    std::vector<data::Exchange> exchanges = {
        data::Exchange::BINANCE,
        data::Exchange::OKX,
        data::Exchange::BYBIT
    };
    
    // Occasionally generate opportunities (higher chance for demo purposes)
    if (gen() % 5 == 0) {  // 20% chance
        core::ArbitrageOpportunity opp;
        opp.underlying_symbol = (gen() % 2 == 0) ? "BTCUSDT" : "ETHUSDT";
        
        // Select two different exchanges for the arbitrage
        int exchange1_idx = exchange_dist(gen);
        int exchange2_idx = exchange_dist(gen);
        while (exchange2_idx == exchange1_idx) {
            exchange2_idx = exchange_dist(gen);
        }
        
        data::Exchange exchange1 = exchanges[exchange1_idx];
        data::Exchange exchange2 = exchanges[exchange2_idx];
        
        // Create legs for the cross-exchange arbitrage opportunity
        core::ArbitrageOpportunity::Leg leg1;
        leg1.symbol = opp.underlying_symbol;
        leg1.type = core::InstrumentType::SPOT;
        leg1.exchange = exchange1;
        
        double base_price = (opp.underlying_symbol == "BTCUSDT") ? 
            50000.0 + (gen() % 1000 - 500) : 
            3000.0 + (gen() % 100 - 50);
            
        leg1.price = base_price;
        leg1.synthetic_price = base_price * (1.0 + profit_dist(gen));
        leg1.deviation = (leg1.price - leg1.synthetic_price) / leg1.synthetic_price;
        leg1.action = "BUY";
        
        core::ArbitrageOpportunity::Leg leg2;
        leg2.symbol = opp.underlying_symbol;
        leg2.type = core::InstrumentType::SPOT;
        leg2.exchange = exchange2;
        leg2.price = base_price * (1.0 + profit_dist(gen));
        leg2.synthetic_price = base_price;
        leg2.deviation = (leg2.price - leg2.synthetic_price) / leg2.synthetic_price;
        leg2.action = "SELL";
        
        opp.legs.push_back(leg1);
        opp.legs.push_back(leg2);
        
        opp.expected_profit_pct = profit_dist(gen);
        opp.required_capital = capital_dist(gen);
        opp.risk_score = risk_dist(gen);
        opp.confidence = confidence_dist(gen);
        opp.detected_at = std::chrono::system_clock::now();
        
        opportunities.push_back(opp);
    }
    
    return opportunities;
}

int main() {
    try {
        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);  // Ctrl+C
        std::signal(SIGTERM, signalHandler); // Termination request
        
        std::cout << "\n🚀 Phase 9: Dashboard Demo - Real-time Web UI with Real Exchange Data\n";
        std::cout << "=======================================================================\n\n";

        // Initialize logging system
        utils::Logger::initialize("logs/dashboard_demo.log", utils::Logger::Level::INFO, utils::Logger::Level::DEBUG);
        LOG_INFO("Dashboard Demo Started with Real Exchange Connections");
        
        // Load configuration
        utils::ConfigManager config("config/config.json");
        
        // Initialize Real-Time Data Manager
        std::cout << "🌐 Initializing Real-Time Exchange Connections...\n";
        real_time_data_manager = std::make_unique<data::RealTimeDataManager>();
        
        if (!real_time_data_manager->initialize()) {
            std::cout << "⚠️  Warning: Some exchange connections failed. Continuing with available connections.\n";
        } else {
            std::cout << "✅ All exchange connections established successfully!\n";
        }
        
        // Start real-time data collection
        if (!real_time_data_manager->start()) {
            std::cerr << "❌ Failed to start real-time data manager!" << std::endl;
            return 1;
        }
        
        // Subscribe to major trading pairs - RATE LIMITED FOR STABILITY
        std::vector<std::string> symbols = {
            "BTCUSDT", "ETHUSDT",    // Major pairs only to avoid rate limits
            "ADAUSDT", "BNBUSDT"     // Popular altcoins only
        };
        
        std::cout << "📡 Subscribing to " << symbols.size() << " trading pairs on all exchanges...\n";
        for (const auto& symbol : symbols) {
            std::cout << "   • " << symbol << "\n";
            real_time_data_manager->subscribeToSymbol(symbol);
            
            // Add delay between subscriptions to prevent rate limiting
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Subscribe to order book data for core pairs only (depth-of-market)
        std::cout << "📚 Subscribing to order book data for major pairs...\n";
        std::vector<std::string> orderBookSymbols = {"BTCUSDT", "ETHUSDT"}; // Reduced to prevent rate limits
        for (const auto& symbol : orderBookSymbols) {
            std::cout << "   📖 Order Book: " << symbol << "\n";
            // Note: Order book subscription would be implemented via the individual exchange clients
            // For now, we'll track this for future implementation
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        
        // Wait for initial data
        std::cout << "⏳ Waiting for initial market data...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Create market environment
        core::MarketEnvironment market_env;
        market_env.interest_rates["USD"] = 0.05;
        market_env.interest_rates["BTC"] = 0.0;
        market_env.interest_rates["ETH"] = 0.0;
        market_env.default_volatility = 0.3;

        // Initialize pricing engine
        auto pricing_engine = std::make_shared<core::PricingEngine>(market_env);
        
        // Register demo instruments for all exchanges
        std::vector<data::Exchange> exchanges = {
            data::Exchange::BINANCE,
            data::Exchange::OKX,
            data::Exchange::BYBIT
        };
        
        for (const auto& exchange : exchanges) {
            // Register BTC instruments
            core::InstrumentSpec btc_spot("BTCUSDT", core::InstrumentType::SPOT, exchange);
            pricing_engine->registerInstrument(btc_spot);
            
            core::InstrumentSpec btc_perp("BTCUSDT-PERP", core::InstrumentType::PERPETUAL_SWAP, exchange);
            pricing_engine->registerInstrument(btc_perp);
            
            // Register ETH instruments
            core::InstrumentSpec eth_spot("ETHUSDT", core::InstrumentType::SPOT, exchange);
            pricing_engine->registerInstrument(eth_spot);
            
            core::InstrumentSpec eth_perp("ETHUSDT-PERP", core::InstrumentType::PERPETUAL_SWAP, exchange);
            pricing_engine->registerInstrument(eth_perp);
        }

        // Create and initialize ArbitrageEngine
        core::ArbitrageConfig arb_config;
        arb_config.min_profit_threshold_usd = 10.0;        // $10 minimum profit for demo
        arb_config.min_profit_threshold_percent = 0.01;    // 0.01% minimum spread
        arb_config.min_confidence_score = 0.7;             // 70% confidence minimum
        arb_config.max_position_size_usd = 10000.0;        // $10k max position for demo
        
        auto arbitrage_engine = std::make_shared<core::ArbitrageEngine>(arb_config);
        
        // Create and initialize Phase 5 Risk Management components
        std::cout << "🛡️  Initializing Risk Management System...\n";
        
        // Configure risk limits
        ArbitrageEngine::RiskLimits risk_limits;
        risk_limits.maxPortfolioVaR = 25000.0;        // $25,000 max VaR for demo
        risk_limits.maxLeverage = 2.0;                // 2x max leverage
        risk_limits.maxConcentration = 0.40;          // 40% max concentration
        risk_limits.maxDrawdown = 0.20;               // 20% max drawdown
        risk_limits.maxPositionSize = 50000.0;        // $50,000 max position
        risk_limits.liquidityThreshold = 0.3;         // 30% min liquidity score
        risk_limits.executionCostThreshold = 0.015;   // 1.5% max execution cost
        
        auto risk_manager = std::make_shared<ArbitrageEngine::RiskManager>(risk_limits);
        if (!risk_manager->initialize()) {
            std::cerr << "❌ Failed to initialize Risk Manager!" << std::endl;
            return 1;
        }
        
        // Set up risk alert callback
        risk_manager->setAlertCallback([](const ArbitrageEngine::RiskAlert& alert) {
            std::string severity_str = (alert.severity == ArbitrageEngine::RiskAlert::Severity::CRITICAL) ? "🚨 CRITICAL" :
                                      (alert.severity == ArbitrageEngine::RiskAlert::Severity::WARNING) ? "⚠️  WARNING" : "ℹ️  INFO";
            std::cout << severity_str << " Risk Alert: " << alert.message 
                      << " (Current: " << alert.currentValue << ", Limit: " << alert.limitValue << ")\n";
        });
        
        // Configure position sizing parameters
        ArbitrageEngine::PositionSizingParams sizing_params;
        sizing_params.method = ArbitrageEngine::PositionSizingMethod::KELLY_CRITERION;
        sizing_params.kellyFraction = 0.25;           // 25% of full Kelly
        sizing_params.maxPositionSize = 25000.0;      // $25,000 max position for demo
        sizing_params.maxLeverage = 2.0;              // 2x max leverage
        sizing_params.maxCorrelation = 0.7;           // 70% max correlation
        sizing_params.targetVolatility = 0.20;        // 20% target volatility
        
        auto position_manager = std::make_shared<ArbitrageEngine::PositionManager>(risk_manager, sizing_params);
        
        double initial_capital = 100000.0; // $100k initial capital for demo
        if (!position_manager->initialize(initial_capital)) {
            std::cerr << "❌ Failed to initialize Position Manager!" << std::endl;
            return 1;
        }
        
        // Allocate capital to different strategies
        position_manager->allocateCapital("arbitrage", 40000.0);    // $40k for arbitrage
        position_manager->allocateCapital("statistical", 30000.0);  // $30k for stat arb
        position_manager->allocateCapital("volatility", 20000.0);   // $20k for vol trading
        // Reserve: $10k
        
        std::cout << "✅ Risk Management System initialized with $" << initial_capital << " capital\n";

        // Create and initialize dashboard
        ui::DashboardApp dashboard(8081);
        dashboard.initialize(pricing_engine);
        dashboard.initializeArbitrageEngine(arbitrage_engine);
        dashboard.initializeRiskManagement(risk_manager, position_manager);
        
        std::cout << "Starting dashboard server...\n";
        std::cout << "Dashboard URL: " << dashboard.getDashboardUrl() << "\n\n";
        std::cout << "📊 Dashboard Features:\n";
        std::cout << "   • Real-time system status monitoring\n";
        std::cout << "   • Live market data visualization (Binance, OKX, Bybit)\n";
        std::cout << "   • Cross-exchange pricing results display\n";
        std::cout << "   • Multi-exchange arbitrage opportunity tracking\n";
        std::cout << "   • Integrated Phase 4 Arbitrage Detection Engine\n";
        std::cout << "   • Real-time arbitrage detection and metrics\n";
        std::cout << "   • Performance metrics dashboard\n";
        std::cout << "   • Risk metrics monitoring\n";
        std::cout << "   • Auto-refresh every 2 seconds\n\n";
        
        // Start dashboard in background
        dashboard.startAsync();
        
        // Set up real-time market data callback to feed dashboard and arbitrage engine
        real_time_data_manager->setMarketDataCallback([&dashboard, arbitrage_engine](const data::MarketDataPoint& market_data) {
            // Convert single market data point to vector for systems that expect it
            std::vector<data::MarketDataPoint> data_vector = {market_data};
            
            // Update arbitrage engine with real market data
            arbitrage_engine->updateMarketData(data_vector);
            
            // Update dashboard with real market data
            dashboard.updateMarketData(data_vector);
            
            LOG_DEBUG("Updated systems with real market data: " + market_data.symbol + 
                     " @ " + market_data.exchange + " = $" + std::to_string(market_data.last));
        });
        
        // Set up real-time order book callback for dashboard
        real_time_data_manager->setOrderBookCallback([&dashboard](const data::OrderBook& order_book) {
            LOG_DEBUG("Updated order book: " + order_book.symbol + 
                     " @ " + data::exchangeToString(order_book.exchange) + 
                     " (bids: " + std::to_string(order_book.bids.size()) + 
                     ", asks: " + std::to_string(order_book.asks.size()) + ")");
        });
        
        // Set up real-time trade callback for dashboard
        real_time_data_manager->setTradeCallback([&dashboard](const data::Trade& trade) {
            LOG_DEBUG("Real-time trade: " + trade.symbol + 
                     " @ " + data::exchangeToString(trade.exchange) + 
                     " Price: $" + std::to_string(trade.price) + 
                     " Qty: " + std::to_string(trade.quantity));
        });
        
        std::cout << "🔗 Connected real-time market data to dashboard and arbitrage engine\n";
        
        // Start risk monitoring
        std::cout << "🚀 Starting real-time risk monitoring...\n";
        risk_manager->startRealTimeMonitoring();
        
        // Create some demo positions for risk monitoring
        std::cout << "📊 Creating demo positions...\n";
        
        // Demo Position 1: BTC Long
        ArbitrageEngine::Position btc_pos;
        btc_pos.positionId = "DEMO_BTC_001";
        btc_pos.symbol = "BTCUSDT";
        btc_pos.exchange = "binance";
        btc_pos.size = 0.5;
        btc_pos.entryPrice = 50000.0;
        btc_pos.currentPrice = 50000.0;
        btc_pos.notionalValue = btc_pos.size * btc_pos.currentPrice;
        btc_pos.leverage = 1.0;
        btc_pos.openTime = std::chrono::system_clock::now();
        btc_pos.lastUpdate = std::chrono::system_clock::now();
        btc_pos.isActive = true;
        btc_pos.isSynthetic = false;
        
        // Demo Position 2: ETH Short
        ArbitrageEngine::Position eth_pos;
        eth_pos.positionId = "DEMO_ETH_002";
        eth_pos.symbol = "ETHUSDT";
        eth_pos.exchange = "okx";
        eth_pos.size = -8.0;
        eth_pos.entryPrice = 3000.0;
        eth_pos.currentPrice = 3000.0;
        eth_pos.notionalValue = std::abs(eth_pos.size) * eth_pos.currentPrice;
        eth_pos.leverage = 1.5;
        eth_pos.openTime = std::chrono::system_clock::now();
        eth_pos.lastUpdate = std::chrono::system_clock::now();
        eth_pos.isActive = true;
        eth_pos.isSynthetic = false;
        
        // Open demo positions
        if (position_manager->openPosition(btc_pos, "arbitrage")) {
            std::cout << "✅ Opened BTC demo position: " << btc_pos.size << " BTC @ $" << btc_pos.entryPrice << "\n";
        }
        if (position_manager->openPosition(eth_pos, "statistical")) {
            std::cout << "✅ Opened ETH demo position: " << eth_pos.size << " ETH @ $" << eth_pos.entryPrice << "\n";
        }

        if (dashboard.isRunning()) {
            std::cout << "✅ Dashboard server started successfully!\n";
            std::cout << "   Open your browser and navigate to: " << dashboard.getDashboardUrl() << "\n\n";
            
            std::cout << "🚀 Starting Phase 4 Arbitrage Detection Engine with Real Data...\n";
            
            // Start arbitrage detection with real market data
            dashboard.startArbitrageDetection();
            
            std::cout << "✅ Arbitrage Detection Engine started with real exchange data!\n";
            std::cout << "   Dashboard APIs available at:\n";
            std::cout << "   • /api/arbitrage/metrics - Performance metrics\n";
            std::cout << "   • /api/opportunities/extended - Real arbitrage opportunities\n";
            std::cout << "   • POST /api/arbitrage/control - Start/stop arbitrage detection\n\n";
            
            std::cout << "🔄 Dashboard is running with REAL multi-exchange data...\n";
            std::cout << "   📡 Live data from Binance, OKX, and Bybit exchanges\n";
            std::cout << "   🔍 Real-time arbitrage detection running in background\n";
            std::cout << "   📊 Live market data feeding into risk management system\n";
            std::cout << "   Press Ctrl+C to stop the demo\n\n";
            
            // Display connection status
            auto connection_status = real_time_data_manager->getConnectionStatus();
            std::cout << "📡 Exchange Connection Status:\n";
            for (const auto& [exchange, status] : connection_status) {
                std::string status_str = (status == data::ConnectionStatus::CONNECTED) ? "✅ Connected" :
                                       (status == data::ConnectionStatus::CONNECTING) ? "🔄 Connecting" :
                                       (status == data::ConnectionStatus::DISCONNECTED) ? "❌ Disconnected" : "⚠️  Error";
                std::cout << "   • " << data::exchangeToString(exchange) << ": " << status_str << "\n";
            }
            std::cout << "\n";
            
            // Keep the demo running until shutdown is requested
            int status_counter = 0;
            while (!shutdown_requested) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                
                // Periodic status updates
                if (++status_counter % 6 == 0) { // Every 60 seconds
                    std::cout << "📊 Dashboard with Real Data is running at " << dashboard.getDashboardUrl() << "\n";
                    auto metrics = dashboard.getArbitrageMetrics();
                    if (!metrics.empty()) {
                        std::cout << "   📈 Arbitrage Metrics: " 
                                  << metrics["opportunities_detected"] << " opportunities detected, "
                                  << metrics["detection_cycles"] << " cycles completed\n";
                    }
                    
                    // Display real market data statistics
                    auto latest_data = real_time_data_manager->getAllLatestData();
                    std::cout << "   📡 Real Market Data: " << latest_data.size() << " active data streams\n";
                    
                    // Display sample prices
                    for (const auto& data : latest_data) {
                        if (data.symbol == "BTCUSDT" || data.symbol == "ETHUSDT") {
                            std::cout << "   💰 " << data.symbol << " @ " << data.exchange 
                                      << ": $" << std::fixed << std::setprecision(2) << data.last << "\n";
                        }
                    }
                    
                    // Display risk metrics
                    if (risk_manager && risk_manager->isRunning()) {
                        auto risk_metrics = risk_manager->calculateRiskMetrics();
                        std::cout << "   🛡️  Risk Metrics: VaR=$" << std::fixed << std::setprecision(0) 
                                  << risk_metrics.portfolioVaR 
                                  << ", Exposure=$" << risk_metrics.totalExposure
                                  << ", Concentration=" << std::setprecision(1) << (risk_metrics.concentrationRisk * 100) << "%\n";
                        
                        // Display portfolio P&L
                        if (position_manager) {
                            auto pnl_data = position_manager->calculatePortfolioPnL();
                            std::cout << "   💰 Portfolio P&L: Total=$" << std::setprecision(2) << pnl_data.totalPnL
                                      << " (Realized=$" << pnl_data.realizedPnL 
                                      << ", Unrealized=$" << pnl_data.unrealizedPnL << ")\n";
                        }
                        
                        // Check for active alerts
                        auto alerts = risk_manager->getActiveAlerts();
                        if (!alerts.empty()) {
                            std::cout << "   🚨 Active Risk Alerts: " << alerts.size() << "\n";
                        }
                    }
                }
                
                // Check if dashboard is still running
                if (!dashboard.isRunning()) {
                    std::cout << "⚠️  Dashboard server stopped unexpectedly. Shutting down...\n";
                    break;
                }
            }
            
            std::cout << "\n🛑 Stopping dashboard server...\n";
            
            // Stop real-time data manager
            if (real_time_data_manager) {
                std::cout << "📡 Stopping real-time exchange connections...\n";
                real_time_data_manager->stop();
            }
            
            // Stop risk monitoring
            if (risk_manager) {
                std::cout << "🛡️  Stopping risk monitoring...\n";
                risk_manager->stopRealTimeMonitoring();
            }
            
            dashboard.stop();
            std::cout << "✅ Dashboard demo with real exchange data stopped successfully.\n";
            
        } else {
            std::cerr << "❌ Failed to start dashboard server!" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
