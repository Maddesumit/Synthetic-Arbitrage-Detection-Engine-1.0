#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>

#include "core/RiskManager.hpp"
#include "core/PositionManager.hpp"
#include "core/ArbitrageEngine.hpp"
#include "utils/Logger.hpp"
#include "data/MarketData.hpp"

using namespace ArbitrageEngine;
using MarketDataPoint = arbitrage::data::MarketDataPoint;
using Logger = arbitrage::utils::Logger;

/**
 * @brief Phase 5 Demo: Advanced Risk Management & Position Tracking
 * 
 * This demo showcases:
 * - Real-time risk monitoring with VaR calculations
 * - Position tracking and P&L attribution
 * - Capital allocation optimization
 * - Risk limits enforcement
 * - Position sizing algorithms
 * - Liquidity risk assessment
 */

class Phase5Demo {
public:
    Phase5Demo() {
        // Initialize logger
        Logger::initialize();
        LOG_INFO("=== Phase 5 Demo: Advanced Risk Management & Position Tracking ===");
    }
    
    void run() {
        try {
            // Step 1: Initialize Risk Management System
            initializeRiskManagement();
            
            // Step 2: Initialize Position Management System
            initializePositionManagement();
            
            // Step 3: Demonstrate Position Sizing Algorithms
            demonstratePositionSizing();
            
            // Step 4: Simulate Trading Activity
            simulateTradingActivity();
            
            // Step 5: Demonstrate Risk Monitoring
            demonstrateRiskMonitoring();
            
            // Step 6: Test Risk Limits and Alerts
            testRiskLimitsAndAlerts();
            
            // Step 7: Demonstrate Portfolio Analytics
            demonstratePortfolioAnalytics();
            
            // Step 8: Generate Reports
            generateReports();
            
            LOG_INFO("=== Phase 5 Demo Completed Successfully ===");
            
        } catch (const std::exception& e) {
            LOG_ERROR("Demo failed: {}", e.what());
        }
    }

private:
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<PositionManager> positionManager_;
    std::mt19937 rng_{std::random_device{}()};
    
    void initializeRiskManagement() {
        LOG_INFO("\n--- Step 1: Initialize Risk Management System ---");
        
        // Configure risk limits
        RiskLimits limits;
        limits.maxPortfolioVaR = 50000.0;        // $50,000 max VaR
        limits.maxLeverage = 3.0;                // 3x max leverage
        limits.maxConcentration = 0.30;          // 30% max concentration
        limits.maxDrawdown = 0.15;               // 15% max drawdown
        limits.maxPositionSize = 200000.0;       // $200,000 max position
        limits.liquidityThreshold = 0.2;         // 20% min liquidity score
        limits.executionCostThreshold = 0.01;    // 1% max execution cost
        
        // Initialize risk manager
        riskManager_ = std::make_shared<RiskManager>(limits);
        
        if (!riskManager_->initialize()) {
            throw std::runtime_error("Failed to initialize RiskManager");
        }
        
        // Set up risk alert callback
        riskManager_->setAlertCallback([](const RiskAlert& alert) {
            std::string severityStr = (alert.severity == RiskAlert::Severity::CRITICAL) ? "CRITICAL" :
                                    (alert.severity == RiskAlert::Severity::WARNING) ? "WARNING" : "INFO";
            
            LOG_WARN("RISK ALERT [{}]: {} (Current: {:.2f}, Limit: {:.2f})",
                                     severityStr, alert.message, alert.currentValue, alert.limitValue);
        });
        
        LOG_INFO("Risk Manager initialized with limits:");
        LOG_INFO("  - Max VaR: ${:.0f}", limits.maxPortfolioVaR);
        LOG_INFO("  - Max Leverage: {:.1f}x", limits.maxLeverage);
        LOG_INFO("  - Max Concentration: {:.0f}%", limits.maxConcentration * 100);
    }
    
    void initializePositionManagement() {
        LOG_INFO("\n--- Step 2: Initialize Position Management System ---");
        
        // Configure position sizing parameters
        PositionSizingParams sizingParams;
        sizingParams.method = PositionSizingMethod::KELLY_CRITERION;
        sizingParams.kellyFraction = 0.25;           // 25% of full Kelly
        sizingParams.maxPositionSize = 150000.0;     // $150,000 max position
        sizingParams.maxLeverage = 2.5;              // 2.5x max leverage
        sizingParams.maxCorrelation = 0.7;           // 70% max correlation
        sizingParams.targetVolatility = 0.20;        // 20% target volatility
        
        // Initialize position manager
        positionManager_ = std::make_shared<PositionManager>(riskManager_, sizingParams);
        
        double initialCapital = 1000000.0; // $1M initial capital
        if (!positionManager_->initialize(initialCapital)) {
            throw std::runtime_error("Failed to initialize PositionManager");
        }
        
        // Allocate capital to different strategies
        positionManager_->allocateCapital("arbitrage", 400000.0);    // $400k for arbitrage
        positionManager_->allocateCapital("statistical", 300000.0);  // $300k for stat arb
        positionManager_->allocateCapital("volatility", 200000.0);   // $200k for vol trading
        
        LOG_INFO("Position Manager initialized with capital: ${:.0f}", initialCapital);
        LOG_INFO("Capital allocated to strategies:");
        LOG_INFO("  - Arbitrage: $400,000");
        LOG_INFO("  - Statistical: $300,000");
        LOG_INFO("  - Volatility: $200,000");
        LOG_INFO("  - Reserve: $100,000");
    }
    
    void demonstratePositionSizing() {
        LOG_INFO("\n--- Step 3: Demonstrate Position Sizing Algorithms ---");
        
        // Test different position sizing methods
        std::vector<std::pair<std::string, std::string>> instruments = {
            {"BTC", "binance"}, {"ETH", "okx"}, {"SOL", "bybit"}
        };
        
        std::vector<std::pair<double, double>> expectedReturnsVols = {
            {0.15, 0.80},  // BTC: 15% return, 80% vol
            {0.20, 1.00},  // ETH: 20% return, 100% vol
            {0.25, 1.20}   // SOL: 25% return, 120% vol
        };
        
        for (size_t i = 0; i < instruments.size(); ++i) {
            const auto& [symbol, exchange] = instruments[i];
            const auto& [expectedReturn, expectedVol] = expectedReturnsVols[i];
            
            auto optimalPosition = positionManager_->calculateOptimalPosition(
                symbol, exchange, expectedReturn, expectedVol, "arbitrage"
            );
            
            if (optimalPosition) {
                LOG_INFO("Optimal position for {}/{}: Size=${:.0f}, Notional=${:.0f}",
                                         symbol, exchange, optimalPosition->size, optimalPosition->notionalValue);
                
                // Demonstrate different sizing methods
                auto params = positionManager_->getSizingParams();
                
                // Kelly sizing
                double kellySize = positionManager_->calculateKellySize(
                    expectedReturn, expectedVol * expectedVol, 100000.0);
                LOG_INFO("  Kelly size (25%): ${:.0f}", kellySize);
                
                // Risk parity sizing
                double riskParitySize = positionManager_->calculateRiskParitySize(
                    symbol, exchange, 100000.0);
                LOG_INFO("  Risk parity size: ${:.0f}", riskParitySize);
                
                // Volatility target sizing
                double volTargetSize = positionManager_->calculateVolatilityTargetSize(
                    0.15, expectedVol, 100000.0);
                LOG_INFO("  Vol target size: ${:.0f}", volTargetSize);
            }
        }
    }
    
    void simulateTradingActivity() {
        LOG_INFO("\n--- Step 4: Simulate Trading Activity ---");
        
        // Create sample positions
        std::vector<Position> positions;
        
        // Position 1: BTC/Binance Long
        Position btcPos;
        btcPos.positionId = "BTC_BINANCE_001";
        btcPos.symbol = "BTC";
        btcPos.exchange = "binance";
        btcPos.size = 2.5;
        btcPos.entryPrice = 45000.0;
        btcPos.currentPrice = 46500.0;
        btcPos.notionalValue = btcPos.size * btcPos.currentPrice;
        btcPos.leverage = 1.0;
        btcPos.openTime = std::chrono::system_clock::now() - std::chrono::hours(2);
        btcPos.lastUpdate = std::chrono::system_clock::now();
        btcPos.isActive = true;
        btcPos.isSynthetic = false;
        positions.push_back(btcPos);
        
        // Position 2: ETH/OKX Short
        Position ethPos;
        ethPos.positionId = "ETH_OKX_002";
        ethPos.symbol = "ETH";
        ethPos.exchange = "okx";
        ethPos.size = -40.0; // Short position
        ethPos.entryPrice = 3200.0;
        ethPos.currentPrice = 3150.0;
        ethPos.notionalValue = std::abs(ethPos.size) * ethPos.currentPrice;
        ethPos.leverage = 1.5;
        ethPos.openTime = std::chrono::system_clock::now() - std::chrono::hours(1);
        ethPos.lastUpdate = std::chrono::system_clock::now();
        ethPos.isActive = true;
        ethPos.isSynthetic = false;
        positions.push_back(ethPos);
        
        // Position 3: SOL Synthetic Position
        Position solPos;
        solPos.positionId = "SOL_SYNTH_003";
        solPos.symbol = "SOL";
        solPos.exchange = "bybit";
        solPos.size = 500.0;
        solPos.entryPrice = 180.0;
        solPos.currentPrice = 185.0;
        solPos.notionalValue = solPos.size * solPos.currentPrice;
        solPos.leverage = 2.0;
        solPos.openTime = std::chrono::system_clock::now() - std::chrono::minutes(30);
        solPos.lastUpdate = std::chrono::system_clock::now();
        solPos.isActive = true;
        solPos.isSynthetic = true; // Synthetic position
        solPos.underlyingAssets = {"SOL-SPOT", "SOL-PERP"};
        positions.push_back(solPos);
        
        // Open positions
        for (const auto& position : positions) {
            std::string strategy = (position.symbol == "BTC") ? "arbitrage" : 
                                 (position.symbol == "ETH") ? "statistical" : "volatility";
            
            if (positionManager_->openPosition(position, strategy)) {
                LOG_INFO("Opened position: {} {} {:.1f} @ ${:.2f}",
                                         position.symbol, position.exchange, 
                                         position.size, position.entryPrice);
            }
        }
        
        // Simulate market data updates
        std::vector<MarketDataPoint> marketData;
        
        MarketDataPoint btcData;
        btcData.symbol = "BTC";
        btcData.exchange = "binance";
        btcData.last = 46500.0;
        btcData.volume = 15000.0;
        btcData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        marketData.push_back(btcData);
        
        MarketDataPoint ethData;
        ethData.symbol = "ETH";
        ethData.exchange = "okx";
        ethData.last = 3150.0;
        ethData.volume = 25000.0;
        ethData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        marketData.push_back(ethData);
        
        MarketDataPoint solData;
        solData.symbol = "SOL";
        solData.exchange = "bybit";
        solData.last = 185.0;
        solData.volume = 8000.0;
        solData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        marketData.push_back(solData);
        
        // Update positions with market data
        positionManager_->updatePositionPrices(marketData);
        riskManager_->updateMarketData(marketData);
        
        LOG_INFO("Market data updated for all positions");
    }
    
    void demonstrateRiskMonitoring() {
        LOG_INFO("\n--- Step 5: Demonstrate Risk Monitoring ---");
        
        // Start real-time risk monitoring
        riskManager_->startRealTimeMonitoring();
        
        // Calculate current risk metrics
        RiskMetrics metrics = riskManager_->calculateRiskMetrics();
        
        LOG_INFO("Current Risk Metrics:");
        LOG_INFO("  Portfolio VaR (95%): ${:.2f}", metrics.portfolioVaR);
        LOG_INFO("  Expected Shortfall: ${:.2f}", metrics.expectedShortfall);
        LOG_INFO("  Total Exposure: ${:.2f}", metrics.totalExposure);
        LOG_INFO("  Leveraged Exposure: ${:.2f}", metrics.leveragedExposure);
        LOG_INFO("  Concentration Risk: {:.1f}%", metrics.concentrationRisk * 100);
        LOG_INFO("  Correlation Risk: {:.1f}%", metrics.correlationRisk * 100);
        LOG_INFO("  Liquidity Risk: {:.1f}%", metrics.liquidityRisk * 100);
        LOG_INFO("  Funding Rate Risk: {:.1f}%", metrics.fundingRateRisk * 100);
        
        // Calculate portfolio P&L
        PnLData portfolioPnL = positionManager_->calculatePortfolioPnL();
        LOG_INFO("\nPortfolio P&L:");
        LOG_INFO("  Realized P&L: ${:.2f}", portfolioPnL.realizedPnL);
        LOG_INFO("  Unrealized P&L: ${:.2f}", portfolioPnL.unrealizedPnL);
        LOG_INFO("  Total P&L: ${:.2f}", portfolioPnL.totalPnL);
        LOG_INFO("  Funding P&L: ${:.2f}", portfolioPnL.fundingPnL);
        
        // Wait a bit for monitoring to run
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    void testRiskLimitsAndAlerts() {
        LOG_INFO("\n--- Step 6: Test Risk Limits and Alerts ---");
        
        // Simulate a large position that would breach risk limits
        Position largePosition;
        largePosition.positionId = "LARGE_POSITION_004";
        largePosition.symbol = "BTC";
        largePosition.exchange = "binance";
        largePosition.size = 50.0; // Very large position
        largePosition.entryPrice = 46000.0;
        largePosition.currentPrice = 46000.0;
        largePosition.notionalValue = largePosition.size * largePosition.currentPrice;
        largePosition.leverage = 5.0; // High leverage
        largePosition.openTime = std::chrono::system_clock::now();
        largePosition.lastUpdate = std::chrono::system_clock::now();
        largePosition.isActive = true;
        
        LOG_INFO("Attempting to open large position: BTC {:.1f} @ ${:.0f} (${:.0f} notional, {:.1f}x leverage)",
                                 largePosition.size, largePosition.entryPrice, 
                                 largePosition.notionalValue, largePosition.leverage);
        
        // This should trigger risk alerts
        if (!positionManager_->openPosition(largePosition, "arbitrage")) {
            LOG_INFO("Large position rejected due to risk limits - GOOD!");
        }
        
        // Check for active alerts
        auto alerts = riskManager_->getActiveAlerts();
        if (!alerts.empty()) {
            LOG_INFO("Active risk alerts: {}", alerts.size());
            for (const auto& alert : alerts) {
                std::string severityStr = (alert.severity == RiskAlert::Severity::CRITICAL) ? "CRITICAL" :
                                        (alert.severity == RiskAlert::Severity::WARNING) ? "WARNING" : "INFO";
                LOG_INFO("  - [{}] {}", severityStr, alert.message);
            }
        }
        
        // Test risk limit violations by manually adding to risk manager
        riskManager_->addPosition(largePosition);
        
        // Check risk limits
        auto limitAlerts = riskManager_->checkRiskLimits();
        LOG_INFO("Risk limit check generated {} alerts", limitAlerts.size());
        
        // Wait for monitoring alerts
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    
    void demonstratePortfolioAnalytics() {
        LOG_INFO("\n--- Step 7: Demonstrate Portfolio Analytics ---");
        
        // Portfolio-level analytics
        double portfolioVol = positionManager_->getPortfolioVolatility();
        LOG_INFO("Portfolio volatility: {:.1f}%", portfolioVol * 100);
        
        // Capital allocation
        CapitalAllocation capitalAlloc = positionManager_->getCapitalAllocation();
        LOG_INFO("\nCapital Allocation:");
        LOG_INFO("  Total Capital: ${:.0f}", capitalAlloc.totalCapital);
        LOG_INFO("  Allocated Capital: ${:.0f}", capitalAlloc.allocatedCapital);
        LOG_INFO("  Available Capital: ${:.0f}", capitalAlloc.availableCapital);
        LOG_INFO("  Used Margin: ${:.0f}", capitalAlloc.usedMargin);
        
        LOG_INFO("  Strategy Allocations:");
        for (const auto& [strategy, allocation] : capitalAlloc.strategyAllocations) {
            LOG_INFO("    {}: ${:.0f}", strategy, allocation);
        }
        
        // Simulate some price movements and P&L changes
        simulatePriceMovements();
        
        // Updated metrics after price movements
        RiskMetrics updatedMetrics = riskManager_->calculateRiskMetrics();
        PnLData updatedPnL = positionManager_->calculatePortfolioPnL();
        
        LOG_INFO("\nAfter price movements:");
        LOG_INFO("  Portfolio VaR: ${:.2f}", updatedMetrics.portfolioVaR);
        LOG_INFO("  Total P&L: ${:.2f}", updatedPnL.totalPnL);
    }
    
    void simulatePriceMovements() {
        LOG_INFO("\nSimulating market price movements...");
        
        std::normal_distribution<> priceDist(0.0, 0.02); // 2% price volatility
        
        // Generate random price movements
        std::vector<MarketDataPoint> newMarketData;
        
        // BTC price movement
        MarketDataPoint btcData;
        btcData.symbol = "BTC";
        btcData.exchange = "binance";
        btcData.last = 46500.0 * (1.0 + priceDist(rng_));
        btcData.volume = 16000.0;
        btcData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        newMarketData.push_back(btcData);
        
        // ETH price movement
        MarketDataPoint ethData;
        ethData.symbol = "ETH";
        ethData.exchange = "okx";
        ethData.last = 3150.0 * (1.0 + priceDist(rng_));
        ethData.volume = 27000.0;
        ethData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        newMarketData.push_back(ethData);
        
        // SOL price movement
        MarketDataPoint solData;
        solData.symbol = "SOL";
        solData.exchange = "bybit";
        solData.last = 185.0 * (1.0 + priceDist(rng_));
        solData.volume = 9000.0;
        solData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        newMarketData.push_back(solData);
        
        // Update positions with new prices
        positionManager_->updatePositionPrices(newMarketData);
        riskManager_->updateMarketData(newMarketData);
        
        LOG_INFO("New prices: BTC=${:.2f}, ETH=${:.2f}, SOL=${:.2f}",
                                 btcData.last, ethData.last, solData.last);
    }
    
    void generateReports() {
        LOG_INFO("\n--- Step 8: Generate Reports ---");
        
        // Position manager report
        std::string positionReport = positionManager_->getPositionReport();
        LOG_INFO("\n{}", positionReport);
        
        // Risk manager status report
        std::string riskReport = riskManager_->getStatusReport();
        LOG_INFO("\n{}", riskReport);
        
        // Stop monitoring
        riskManager_->stopRealTimeMonitoring();
        
        LOG_INFO("Risk monitoring stopped");
    }
};

int main() {
    try {
        Phase5Demo demo;
        demo.run();
        
        std::cout << "\nPhase 5 Demo completed successfully!" << std::endl;
        std::cout << "Check the logs for detailed risk management and position tracking output." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Phase 5 Demo failed: " << e.what() << std::endl;
        return 1;
    }
}
