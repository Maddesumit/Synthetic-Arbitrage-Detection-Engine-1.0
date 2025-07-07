#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include "core/ArbitrageEngine.hpp"
#include "data/MarketData.hpp"
#include "utils/Logger.hpp"

using namespace arbitrage;

int main() {
    std::cout << "=== Phase 4: Arbitrage Detection Engine Demo ===" << std::endl;
    
    try {
        // Initialize Logger
        arbitrage::utils::Logger::initialize("logs/demo.log", arbitrage::utils::Logger::Level::INFO, arbitrage::utils::Logger::Level::DEBUG);
        
        // 1. Create arbitrage engine with custom configuration
        core::ArbitrageConfig config;
        config.min_profit_threshold_usd = 5.0;        // $5 minimum profit
        config.min_profit_threshold_percent = 0.02;   // 0.02% minimum spread
        config.min_confidence_score = 0.6;            // 60% confidence minimum
        config.max_position_size_usd = 5000.0;        // $5k max position
        
        core::ArbitrageEngine engine(config);
        
        std::cout << "✓ ArbitrageEngine initialized successfully" << std::endl;
        
        // 2. Create sample market data
        std::vector<data::MarketDataPoint> market_data;
        
        // BTC data from different exchanges
        data::MarketDataPoint btc_binance("BTC-USDT", "binance");
        btc_binance.last = 43500.00;
        btc_binance.bid = 43495.00;
        btc_binance.ask = 43505.00;
        btc_binance.volume = 1500000.0;
        btc_binance.funding_rate = 0.0001;
        market_data.push_back(btc_binance);
        
        data::MarketDataPoint btc_okx("BTC-USDT", "okx");
        btc_okx.last = 43520.00;  // $20 higher - potential arbitrage!
        btc_okx.bid = 43515.00;
        btc_okx.ask = 43525.00;
        btc_okx.volume = 1200000.0;
        btc_okx.funding_rate = 0.0002;
        market_data.push_back(btc_okx);
        
        // BTC Perpetual data
        data::MarketDataPoint btc_perp_binance("BTC-PERP", "binance");
        btc_perp_binance.last = 43480.00;  // $20 lower than spot - potential spot/perp arbitrage
        btc_perp_binance.bid = 43475.00;
        btc_perp_binance.ask = 43485.00;
        btc_perp_binance.volume = 2000000.0;
        btc_perp_binance.funding_rate = 0.0001;
        market_data.push_back(btc_perp_binance);
        
        // ETH data
        data::MarketDataPoint eth_binance("ETH-USDT", "binance");
        eth_binance.last = 2650.00;
        eth_binance.bid = 2649.00;
        eth_binance.ask = 2651.00;
        eth_binance.volume = 800000.0;
        eth_binance.funding_rate = 0.00015;
        market_data.push_back(eth_binance);
        
        data::MarketDataPoint eth_okx("ETH-USDT", "okx");
        eth_okx.last = 2645.00;  // $5 lower
        eth_okx.bid = 2644.00;
        eth_okx.ask = 2646.00;
        eth_okx.volume = 750000.0;
        eth_okx.funding_rate = 0.0003;  // Higher funding rate
        market_data.push_back(eth_okx);
        
        std::cout << "✓ Sample market data created (" << market_data.size() << " data points)" << std::endl;
        
        // 3. Create sample pricing results (synthetic prices)
        std::vector<core::PricingResult> pricing_results;
        
        core::PricingResult btc_synthetic;
        btc_synthetic.instrument_id = "BTC-PERP_synthetic";
        btc_synthetic.synthetic_price = 43510.00;  // Synthetic perpetual price
        btc_synthetic.confidence = 0.85;
        btc_synthetic.pricing_model = "synthetic_perpetual";
        btc_synthetic.success = true;
        pricing_results.push_back(btc_synthetic);
        
        std::cout << "✓ Sample pricing results created" << std::endl;
        
        // 4. Start the arbitrage engine
        engine.start();
        
        // 5. Run detection cycle
        std::cout << "\n=== Running Arbitrage Detection ===" << std::endl;
        
        auto opportunities = engine.detectOpportunities(market_data, pricing_results);
        
        std::cout << "\n=== Detection Results ===" << std::endl;
        std::cout << "Found " << opportunities.size() << " arbitrage opportunities" << std::endl;
        
        // 6. Display results
        for (size_t i = 0; i < opportunities.size(); ++i) {
            const auto& opp = opportunities[i];
            std::cout << "\n--- Opportunity " << (i + 1) << " ---" << std::endl;
            std::cout << "ID: " << opp.id << std::endl;
            std::cout << "Symbol: " << opp.instrument_symbol << std::endl;
            std::cout << "Strategy: ";
            
            switch (opp.strategy_type) {
                case core::ArbitrageOpportunityExtended::StrategyType::SPOT_PERP_ARBITRAGE:
                    std::cout << "Spot-Perpetual Arbitrage";
                    break;
                case core::ArbitrageOpportunityExtended::StrategyType::FUNDING_RATE_ARBITRAGE:
                    std::cout << "Funding Rate Arbitrage";
                    break;
                case core::ArbitrageOpportunityExtended::StrategyType::CROSS_EXCHANGE_ARBITRAGE:
                    std::cout << "Cross-Exchange Arbitrage";
                    break;
                default:
                    std::cout << "Other";
            }
            std::cout << std::endl;
            
            std::cout << "Price A: $" << opp.price_a << " (" << opp.exchange_a << ")" << std::endl;
            std::cout << "Price B: $" << opp.price_b << " (" << opp.exchange_b << ")" << std::endl;
            std::cout << "Spread: " << std::fixed << std::setprecision(3) << opp.percentage_spread << "%" << std::endl;
            std::cout << "Expected Profit: $" << std::setprecision(2) << opp.expected_profit_usd << std::endl;
            std::cout << "Required Capital: $" << std::setprecision(0) << opp.required_capital << std::endl;
            std::cout << "Confidence: " << std::setprecision(1) << (opp.confidence_score * 100) << "%" << std::endl;
            std::cout << "Risk-Adjusted Return: $" << std::setprecision(2) << opp.risk_adjusted_return << std::endl;
            std::cout << "Valid: " << (opp.is_valid ? "Yes" : "No") << std::endl;
            std::cout << "Executable: " << (opp.is_executable ? "Yes" : "No") << std::endl;
            
            if (!opp.validation_notes.empty()) {
                std::cout << "Notes: " << opp.validation_notes << std::endl;
            }
            
            std::cout << "Execution Legs: " << opp.legs.size() << std::endl;
            for (size_t j = 0; j < opp.legs.size(); ++j) {
                const auto& leg = opp.legs[j];
                std::cout << "  " << (j + 1) << ". " << leg.action << " " << leg.quantity 
                         << " " << leg.instrument << " on " << leg.exchange << std::endl;
            }
        }
        
        // 7. Show performance metrics
        std::cout << "\n=== Performance Metrics ===" << std::endl;
        const auto& metrics = engine.getPerformanceMetrics();
        std::cout << "Detection Cycles: " << metrics.detection_cycles.load() << std::endl;
        std::cout << "Opportunities Detected: " << metrics.opportunities_detected.load() << std::endl;
        std::cout << "Opportunities Validated: " << metrics.opportunities_validated.load() << std::endl;
        std::cout << "Total Expected Profit: $" << std::fixed << std::setprecision(2) 
                 << metrics.total_expected_profit.load() << std::endl;
        std::cout << "Avg Detection Latency: " << std::setprecision(1) 
                 << metrics.avg_detection_latency_ms.load() << "ms" << std::endl;
        
        // 8. Test synthetic construction
        std::cout << "\n=== Testing Synthetic Construction ===" << std::endl;
        
        double synthetic_perp = engine.constructSyntheticPerpetual("BTC-USDT", "binance");
        std::cout << "Synthetic BTC Perpetual Price: $" << std::fixed << std::setprecision(2) 
                 << synthetic_perp << std::endl;
        
        auto future_expiry = std::chrono::system_clock::now() + std::chrono::hours(24 * 30); // 30 days
        double synthetic_future = engine.constructSyntheticFuture("BTC-USDT", "binance", future_expiry);
        std::cout << "Synthetic BTC Future Price (30 days): $" << std::fixed << std::setprecision(2) 
                 << synthetic_future << std::endl;
        
        // 9. Stop the engine
        engine.stop();
        
        std::cout << "\n=== Demo Complete ===" << std::endl;
        std::cout << "✓ ArbitrageEngine successfully demonstrated all core functionality" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
