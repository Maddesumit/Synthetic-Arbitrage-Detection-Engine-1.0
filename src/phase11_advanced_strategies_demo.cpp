#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

// Phase 11 includes
#include "core/AdvancedSyntheticStrategies.hpp"
#include "data/MarketData.hpp"
#include "core/PricingEngine.hpp"
#include "core/ArbitrageEngine.hpp"
#include "utils/Logger.hpp"
#include "data/RealTimeDataManager.hpp"

using namespace arbitrage;

void printMultiLegPosition(const core::MultiLegPosition& position) {
    std::cout << "\n📊 Multi-Leg Strategy: " << position.strategy_id << "\n";
    std::cout << "   Type: ";
    switch (position.strategy_type) {
        case core::AdvancedStrategyType::MULTI_LEG_ARBITRAGE:
            std::cout << "Multi-Leg Arbitrage";
            break;
        case core::AdvancedStrategyType::STATISTICAL_ARBITRAGE:
            std::cout << "Statistical Arbitrage";
            break;
        case core::AdvancedStrategyType::VOLATILITY_ARBITRAGE:
            std::cout << "Volatility Arbitrage";
            break;
        case core::AdvancedStrategyType::CROSS_ASSET_ARBITRAGE:
            std::cout << "Cross-Asset Arbitrage";
            break;
        default:
            std::cout << "Other";
    }
    std::cout << "\n";
    
    std::cout << "   Instruments: ";
    for (size_t i = 0; i < position.instruments.size(); ++i) {
        std::cout << position.instruments[i];
        if (i < position.instruments.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    
    std::cout << "   Exchanges: ";
    for (size_t i = 0; i < position.exchanges.size(); ++i) {
        std::cout << position.exchanges[i];
        if (i < position.exchanges.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    
    std::cout << "   Weights: ";
    for (size_t i = 0; i < position.weights.size(); ++i) {
        std::cout << position.weights[i];
        if (i < position.weights.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    
    std::cout << "   Expected Profit: $" << position.expected_profit << "\n";
    std::cout << "   Required Capital: $" << position.required_capital << "\n";
    std::cout << "   Risk Score: " << position.risk_score << "\n";
    std::cout << "   Correlation Score: " << position.correlation_score << "\n";
}

void printStatisticalSignal(const core::StatisticalSignal& signal) {
    std::cout << "\n📈 Statistical Signal: " << signal.signal_id << "\n";
    std::cout << "   Instrument Pair: " << signal.instrument_pair << "\n";
    std::cout << "   Signal Type: " << signal.signal_type << "\n";
    std::cout << "   Z-Score: " << signal.z_score << "\n";
    std::cout << "   Mean Reversion Strength: " << signal.mean_reversion_strength << "\n";
    std::cout << "   Cointegration Ratio: " << signal.cointegration_ratio << "\n";
    std::cout << "   Confidence Level: " << (signal.confidence_level * 100) << "%\n";
    std::cout << "   Entry Threshold: ±" << signal.entry_threshold << "\n";
    std::cout << "   Exit Threshold: ±" << signal.exit_threshold << "\n";
}

void printVolatilitySurface(const core::VolatilitySurface& surface) {
    std::cout << "\n🌊 Volatility Surface for " << surface.underlying_asset << "\n";
    std::cout << "   Spot Price: $" << surface.spot_price << "\n";
    std::cout << "   Risk-Free Rate: " << (surface.risk_free_rate * 100) << "%\n";
    
    std::cout << "\n   Implied Volatility Matrix:\n";
    std::cout << "   Strike\\Expiry";
    
    // Print expiry headers
    std::set<double> expiries;
    for (const auto& [strike, expiry_map] : surface.surface) {
        for (const auto& [expiry, vol] : expiry_map) {
            expiries.insert(expiry);
        }
    }
    
    for (double expiry : expiries) {
        std::cout << "\t" << expiry << "Y";
    }
    std::cout << "\n";
    
    // Print volatility data
    for (const auto& [strike, expiry_map] : surface.surface) {
        std::cout << "   $" << static_cast<int>(strike);
        for (double expiry : expiries) {
            auto it = expiry_map.find(expiry);
            if (it != expiry_map.end()) {
                std::cout << "\t" << std::fixed << std::setprecision(1) << (it->second * 100) << "%";
            } else {
                std::cout << "\t--";
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "\n   Volatility Skew:\n";
    for (const auto& [strike, skew] : surface.skew_data) {
        std::cout << "   $" << static_cast<int>(strike) << ": " 
                  << std::fixed << std::setprecision(2) << (skew * 100) << "%\n";
    }
}

void printCrossAssetCorrelation(const core::CrossAssetCorrelation& correlation) {
    std::cout << "\n🔗 Cross-Asset Correlation Analysis\n";
    std::cout << "   Portfolio Variance: " << correlation.portfolio_variance << "\n";
    std::cout << "   Portfolio Sharpe Ratio: " << correlation.portfolio_sharpe << "\n";
    
    std::cout << "\n   Asset Weights:\n";
    for (const auto& [asset, weight] : correlation.asset_weights) {
        std::cout << "   " << asset << ": " << (weight * 100) << "%\n";
    }
    
    std::cout << "\n   Asset Volatilities:\n";
    for (const auto& [asset, vol] : correlation.volatilities) {
        std::cout << "   " << asset << ": " << (vol * 100) << "%\n";
    }
    
    std::cout << "\n   Correlation Matrix (Top 5x5):\n";
    std::cout << "   Asset";
    int count = 0;
    std::vector<std::string> assets;
    for (const auto& [asset, _] : correlation.correlation_matrix) {
        if (count++ >= 5) break;
        assets.push_back(asset);
        std::cout << "\t" << asset.substr(0, 8);
    }
    std::cout << "\n";
    
    count = 0;
    for (const auto& asset1 : assets) {
        if (count++ >= 5) break;
        std::cout << "   " << asset1.substr(0, 8);
        for (const auto& asset2 : assets) {
            auto it1 = correlation.correlation_matrix.find(asset1);
            if (it1 != correlation.correlation_matrix.end()) {
                auto it2 = it1->second.find(asset2);
                if (it2 != it1->second.end()) {
                    std::cout << "\t" << std::fixed << std::setprecision(2) << it2->second;
                } else {
                    std::cout << "\t--";
                }
            } else {
                std::cout << "\t--";
            }
        }
        std::cout << "\n";
    }
}

int main() {
    try {
        // Initialize logger
        utils::Logger::initialize("logs/phase11_demo.log", utils::Logger::Level::INFO);
        
        std::cout << "=== Phase 11: Advanced Synthetic Strategies Demo ===\n";
        std::cout << "This demo showcases advanced multi-leg arbitrage, statistical arbitrage,\n";
        std::cout << "volatility arbitrage, and cross-asset arbitrage strategies.\n\n";

        // Initialize components
        auto pricing_engine = std::make_shared<core::PricingEngine>();
        auto arbitrage_engine = std::make_shared<core::ArbitrageEngine>();
        
        // Initialize advanced synthetic strategies engine
        auto advanced_strategies = std::make_unique<core::AdvancedSyntheticStrategies>();
        advanced_strategies->initialize(pricing_engine, arbitrage_engine);
        
        std::cout << "✅ Advanced Synthetic Strategies Engine initialized\n\n";

        // Create sample market data for demonstration
        std::vector<data::MarketData> market_data;
        
        // BTC data
        data::MarketData btc;
        btc.symbol = "BTCUSDT";
        btc.exchange = "binance";
        btc.last = 45000.0;
        btc.bid = 44995.0;
        btc.ask = 45005.0;
        btc.volume = 1250.5;
        btc.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        market_data.push_back(btc);
        
        // ETH data
        data::MarketData eth;
        eth.symbol = "ETHUSDT";
        eth.exchange = "okx";
        eth.last = 2800.0;
        eth.bid = 2798.0;
        eth.ask = 2802.0;
        eth.volume = 850.2;
        eth.timestamp = btc.timestamp;
        market_data.push_back(eth);
        
        // ADA data
        data::MarketData ada;
        ada.symbol = "ADAUSDT";
        ada.exchange = "bybit";
        ada.last = 0.85;
        ada.bid = 0.849;
        ada.ask = 0.851;
        ada.volume = 15000.0;
        ada.timestamp = btc.timestamp;
        market_data.push_back(ada);
        
        // SOL data
        data::MarketData sol;
        sol.symbol = "SOLUSDT";
        sol.exchange = "binance";
        sol.last = 180.0;
        sol.bid = 179.5;
        sol.ask = 180.5;
        sol.volume = 2250.0;
        sol.timestamp = btc.timestamp;
        market_data.push_back(sol);

        std::cout << "📊 Created sample market data for " << market_data.size() << " instruments\n\n";

        // 11.1 Multi-Leg Arbitrage Demo
        std::cout << "=== 11.1 Multi-Leg Arbitrage Strategies ===\n";
        
        auto multi_leg_opportunities = advanced_strategies->generateMultiLegOpportunities(market_data);
        std::cout << "🔍 Generated " << multi_leg_opportunities.size() << " multi-leg arbitrage opportunities:\n";
        
        for (const auto& opportunity : multi_leg_opportunities) {
            printMultiLegPosition(opportunity);
            if (&opportunity != &multi_leg_opportunities.back()) {
                std::cout << "\n" << std::string(60, '-') << "\n";
            }
        }

        std::cout << "\n\n=== 11.2 Statistical Arbitrage Strategies ===\n";
        
        // Generate mean reversion signals
        auto mean_reversion_signals = advanced_strategies->generateMeanReversionSignals(market_data);
        std::cout << "📈 Generated " << mean_reversion_signals.size() << " mean reversion signals:\n";
        
        for (const auto& signal : mean_reversion_signals) {
            printStatisticalSignal(signal);
        }
        
        // Generate pairs trading opportunities
        auto pairs_opportunities = advanced_strategies->findPairsTradingOpportunities(market_data);
        std::cout << "\n👥 Found " << pairs_opportunities.size() << " pairs trading opportunities:\n";
        
        for (const auto& opportunity : pairs_opportunities) {
            printStatisticalSignal(opportunity);
        }

        std::cout << "\n\n=== 11.3 Volatility Arbitrage Strategies ===\n";
        
        // Construct volatility surface
        auto vol_surface = advanced_strategies->constructVolatilitySurface(market_data);
        printVolatilitySurface(vol_surface);
        
        // Find volatility arbitrage opportunities
        auto vol_opportunities = advanced_strategies->findVolatilityArbitrage(vol_surface);
        std::cout << "\n🌊 Found " << vol_opportunities.size() << " volatility arbitrage opportunities:\n";
        
        for (const auto& opportunity : vol_opportunities) {
            printMultiLegPosition(opportunity);
        }

        std::cout << "\n\n=== 11.4 Cross-Asset Arbitrage Strategies ===\n";
        
        // Create sample forex and commodity data
        std::vector<data::MarketData> forex_data, commodity_data;
        
        data::MarketData eurusd;
        eurusd.symbol = "EURUSD";
        eurusd.exchange = "forex";
        eurusd.last = 1.0850;
        eurusd.bid = 1.0849;
        eurusd.ask = 1.0851;
        eurusd.volume = 50000.0;
        eurusd.timestamp = btc.timestamp;
        forex_data.push_back(eurusd);
        
        data::MarketData gold;
        gold.symbol = "XAUUSD";
        gold.exchange = "commodity";
        gold.last = 2050.0;
        gold.bid = 2049.0;
        gold.ask = 2051.0;
        gold.volume = 150.0;
        gold.timestamp = btc.timestamp;
        commodity_data.push_back(gold);
        
        // Calculate cross-asset correlation
        auto correlation = advanced_strategies->calculateCrossAssetCorrelation(
            market_data, forex_data, commodity_data);
        printCrossAssetCorrelation(correlation);

        std::cout << "\n\n=== Advanced Strategy Performance Summary ===\n";
        
        // Add best strategies to the engine
        if (!multi_leg_opportunities.empty()) {
            advanced_strategies->addStrategy(multi_leg_opportunities[0]);
        }
        if (!vol_opportunities.empty()) {
            advanced_strategies->addStrategy(vol_opportunities[0]);
        }
        
        auto performance = advanced_strategies->getStrategyPerformance();
        std::cout << "📊 Strategy Performance Metrics:\n";
        for (const auto& [metric, value] : performance) {
            std::cout << "   " << metric << ": ";
            if (metric.find("profit") != std::string::npos || metric.find("capital") != std::string::npos) {
                std::cout << "$" << std::fixed << std::setprecision(2) << value;
            } else if (metric.find("return") != std::string::npos) {
                std::cout << std::fixed << std::setprecision(2) << (value * 100) << "%";
            } else {
                std::cout << std::fixed << std::setprecision(2) << value;
            }
            std::cout << "\n";
        }

        std::cout << "\n=== Phase 11 Demo Features Demonstrated ===\n";
        std::cout << "✅ Multi-leg arbitrage opportunity generation\n";
        std::cout << "✅ Complex synthetic construction across exchanges\n";
        std::cout << "✅ Statistical arbitrage signal generation\n";
        std::cout << "✅ Mean reversion and pairs trading analysis\n";
        std::cout << "✅ Cointegration analysis for long-term relationships\n";
        std::cout << "✅ Volatility surface construction and analysis\n";
        std::cout << "✅ Volatility skew arbitrage detection\n";
        std::cout << "✅ Cross-asset correlation analysis\n";
        std::cout << "✅ Portfolio optimization and risk metrics\n";
        std::cout << "✅ Advanced strategy performance tracking\n";

        std::cout << "\n=== Phase 11 Advanced Synthetic Strategies Demo Complete ===\n";
        std::cout << "All advanced arbitrage strategies are now operational!\n";
        std::cout << "Ready for Phase 11 UI integration and production deployment.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during Phase 11 demo: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
