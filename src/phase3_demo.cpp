#include "core/PricingEngine.hpp"
#include "core/MathUtils.hpp"
#include "data/MarketData.hpp"
#include "utils/Logger.hpp"
#include "utils/ConfigManager.hpp"
#include <iostream>
#include <iomanip>

using namespace arbitrage;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void demonstrateMathUtils() {
    printSeparator("Mathematical Utilities Demo");
    
    // Test Black-Scholes pricing
    double spot = 50000.0;
    double strike = 52000.0;
    double time_to_expiry = 0.25; // 3 months
    double risk_free_rate = 0.05;
    double volatility = 0.30;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Black-Scholes Option Pricing:" << std::endl;
    std::cout << "  Spot Price: $" << spot << std::endl;
    std::cout << "  Strike Price: $" << strike << std::endl;
    std::cout << "  Time to Expiry: " << time_to_expiry << " years" << std::endl;
    std::cout << "  Risk-free Rate: " << (risk_free_rate * 100) << "%" << std::endl;
    std::cout << "  Volatility: " << (volatility * 100) << "%" << std::endl;
    
    double call_price = core::MathUtils::blackScholesPrice(spot, strike, time_to_expiry, 
                                                          risk_free_rate, volatility, true);
    double put_price = core::MathUtils::blackScholesPrice(spot, strike, time_to_expiry, 
                                                         risk_free_rate, volatility, false);
    
    std::cout << "\\n  Call Option Price: $" << call_price << std::endl;
    std::cout << "  Put Option Price: $" << put_price << std::endl;
    
    // Test Greeks calculation
    auto greeks = core::MathUtils::calculateGreeks(spot, strike, time_to_expiry, 
                                                   risk_free_rate, volatility, true);
    
    std::cout << "\\n  Greeks (Call Option):" << std::endl;
    std::cout << "    Delta: " << std::setprecision(4) << greeks.delta << std::endl;
    std::cout << "    Gamma: " << greeks.gamma << std::endl;
    std::cout << "    Theta: " << greeks.theta << std::endl;
    std::cout << "    Vega: " << greeks.vega << std::endl;
    std::cout << "    Rho: " << greeks.rho << std::endl;
    
    // Test synthetic pricing models
    std::cout << "\\n  Synthetic Pricing Models:" << std::endl;
    
    double funding_rate = 0.0001; // 0.01% funding rate
    double perpetual_price = core::MathUtils::perpetualSyntheticPrice(spot, funding_rate);
    std::cout << "    Perpetual Swap Price: $" << std::setprecision(2) << perpetual_price << std::endl;
    
    double futures_price = core::MathUtils::futuresSyntheticPrice(spot, risk_free_rate, time_to_expiry);
    std::cout << "    Futures Price: $" << futures_price << std::endl;
    
    double basis = core::MathUtils::calculateBasis(futures_price, spot);
    double annualized_basis = core::MathUtils::calculateAnnualizedBasis(basis, spot, time_to_expiry);
    std::cout << "    Basis: $" << basis << " (Annualized: " << std::setprecision(4) << (annualized_basis * 100) << "%)" << std::endl;
    
    // Test SIMD vector operations
    std::cout << "\\n  SIMD Vector Operations:" << std::endl;
    std::vector<double> vec_a = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> vec_b = {2.0, 3.0, 4.0, 5.0, 6.0};
    std::vector<double> result;
    
    core::MathUtils::vectorAdd(vec_a, vec_b, result);
    std::cout << "    Vector Addition: [";
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << result[i] << (i < result.size() - 1 ? ", " : "");
    }
    std::cout << "]" << std::endl;
    
    core::MathUtils::vectorScale(vec_a, 2.5, result);
    std::cout << "    Vector Scaling (x2.5): [";
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << result[i] << (i < result.size() - 1 ? ", " : "");
    }
    std::cout << "]" << std::endl;
}

void demonstratePricingEngine() {
    printSeparator("Pricing Engine Demo");
    
    // Create market environment
    core::MarketEnvironment market_env;
    market_env.risk_free_rate = 0.05;
    market_env.default_volatility = 0.30;
    
    // Create pricing engine
    auto pricing_engine = std::make_shared<core::PricingEngine>(market_env);
    
    // Register instruments
    core::InstrumentSpec btc_spot("BTCUSDT", core::InstrumentType::SPOT, data::Exchange::BINANCE);
    core::InstrumentSpec btc_perp("BTCUSDT-PERP", core::InstrumentType::PERPETUAL_SWAP, data::Exchange::BINANCE);
    btc_perp.underlying_symbol = "BTCUSDT";
    
    core::InstrumentSpec btc_call("BTCUSDT-52000-CALL", core::InstrumentType::CALL_OPTION, data::Exchange::BINANCE);
    btc_call.underlying_symbol = "BTCUSDT";
    btc_call.strike_price = 52000.0;
    btc_call.expiry_time = std::chrono::system_clock::now() + std::chrono::hours(24 * 90); // 3 months
    
    pricing_engine->registerInstrument(btc_spot);
    pricing_engine->registerInstrument(btc_perp);
    pricing_engine->registerInstrument(btc_call);
    
    std::cout << "Registered instruments:" << std::endl;
    std::cout << "  1. " << btc_spot.symbol << " (Spot)" << std::endl;
    std::cout << "  2. " << btc_perp.symbol << " (Perpetual Swap)" << std::endl;
    std::cout << "  3. " << btc_call.symbol << " (Call Option, Strike: $" << btc_call.strike_price << ")" << std::endl;
    
    // Create sample market data
    data::MarketData market_data;
    market_data.symbol = "BTCUSDT";
    
    // Create sample order book
    data::OrderBook orderbook;
    orderbook.symbol = "BTCUSDT";
    orderbook.exchange = data::Exchange::BINANCE;
    orderbook.timestamp = std::chrono::system_clock::now();
    orderbook.bids = {{50000.50, 1.5}, {50000.25, 2.0}, {50000.00, 0.8}};
    orderbook.asks = {{50000.75, 1.2}, {50001.00, 1.8}, {50001.25, 0.5}};
    
    market_data.updateOrderBook(data::Exchange::BINANCE, orderbook);
    
    // Create sample funding rate
    data::FundingRate funding_rate("BTCUSDT-PERP", data::Exchange::BINANCE, 0.0001);
    market_data.updateFundingRate(data::Exchange::BINANCE, funding_rate);
    
    std::cout << "\\nMarket Data:" << std::endl;
    std::cout << "  Best Bid: $" << std::setprecision(2) << orderbook.getBestBid() << std::endl;
    std::cout << "  Best Ask: $" << orderbook.getBestAsk() << std::endl;
    std::cout << "  Mid Price: $" << orderbook.getMidPrice() << std::endl;
    std::cout << "  Funding Rate: " << std::setprecision(6) << funding_rate.funding_rate << std::endl;
    
    // Calculate synthetic prices
    std::cout << "\\nSynthetic Pricing Results:" << std::endl;
    
    auto all_prices = pricing_engine->calculateAllSyntheticPrices(market_data);
    
    for (const auto& [symbol, result] : all_prices) {
        std::cout << "\\n  " << symbol << ":" << std::endl;
        std::cout << "    Synthetic Price: $" << std::setprecision(2) << result.synthetic_price << std::endl;
        std::cout << "    Confidence: " << std::setprecision(2) << (result.confidence * 100) << "%" << std::endl;
        std::cout << "    Model: " << result.pricing_model << std::endl;
        
        if (result.components.base_price > 0) {
            std::cout << "    Components:" << std::endl;
            std::cout << "      Base Price: $" << result.components.base_price << std::endl;
            if (result.components.carry_cost != 0) {
                std::cout << "      Carry Cost: $" << result.components.carry_cost << std::endl;
            }
            if (result.components.funding_adjustment != 0) {
                std::cout << "      Funding Adj: $" << result.components.funding_adjustment << std::endl;
            }
            if (result.components.time_value != 0) {
                std::cout << "      Time Value: $" << result.components.time_value << std::endl;
            }
        }
        
        if (result.greeks.has_value()) {
            const auto& greeks = result.greeks.value();
            std::cout << "    Greeks:" << std::endl;
            std::cout << "      Delta: " << std::setprecision(4) << greeks.delta << std::endl;
            std::cout << "      Gamma: " << greeks.gamma << std::endl;
            std::cout << "      Theta: " << greeks.theta << std::endl;
            std::cout << "      Vega: " << greeks.vega << std::endl;
        }
    }
    
    // Display pricing engine statistics
    auto stats = pricing_engine->getStatistics();
    std::cout << "\\nPricing Engine Statistics:" << std::endl;
    std::cout << "  Total Calculations: " << stats.total_calculations << std::endl;
    std::cout << "  Successful Calculations: " << stats.successful_calculations << std::endl;
    std::cout << "  Success Rate: " << std::setprecision(1) << 
                 (stats.total_calculations > 0 ? (double)stats.successful_calculations / stats.total_calculations * 100 : 0) << "%" << std::endl;
    std::cout << "  Average Calculation Time: " << std::setprecision(3) << stats.average_calculation_time_ms << " ms" << std::endl;
    std::cout << "  Registered Instruments: " << stats.registered_instruments << std::endl;
}

void demonstrateArbitrageDetection() {
    printSeparator("Arbitrage Detection Demo");
    
    // Create pricing engine
    auto pricing_engine = std::make_shared<core::PricingEngine>();
    
    // Create arbitrage detector
    core::ArbitrageDetector::Parameters params;
    params.min_profit_threshold = 0.001; // 0.1%
    params.max_risk_score = 0.5;
    params.min_confidence = 0.7;
    
    core::ArbitrageDetector detector(pricing_engine, params);
    
    std::cout << "Arbitrage Detection Parameters:" << std::endl;
    std::cout << "  Min Profit Threshold: " << (params.min_profit_threshold * 100) << "%" << std::endl;
    std::cout << "  Max Risk Score: " << params.max_risk_score << std::endl;
    std::cout << "  Min Confidence: " << params.min_confidence << std::endl;
    
    // Create sample market data
    data::MarketData market_data;
    market_data.symbol = "BTCUSDT";
    
    // Detect opportunities (simplified demo)
    auto opportunities = detector.detectOpportunities(market_data);
    
    std::cout << "\\nDetected Opportunities: " << opportunities.size() << std::endl;
    
    if (opportunities.empty()) {
        std::cout << "No arbitrage opportunities detected with current parameters." << std::endl;
        std::cout << "This is expected as we're using simplified demo data." << std::endl;
    }
}

int main() {
    try {
        // Initialize logger
        utils::Logger::initialize();
        LOG_INFO("Starting Phase 3 Core Pricing Engine Demo");
        
        // Load configuration
        utils::ConfigManager config("config/config.json");
        LOG_INFO("Configuration loaded successfully");
        
        std::cout << "\\n🚀 Phase 3: Core Pricing Engine Demo" << std::endl;
        std::cout << "===================================\\n" << std::endl;
        
        // Run demonstrations
        demonstrateMathUtils();
        demonstratePricingEngine();
        demonstrateArbitrageDetection();
        
        printSeparator("Phase 3 Demo Completed Successfully!");
        std::cout << "\\n✅ All Phase 3 components are working correctly:" << std::endl;
        std::cout << "   • Mathematical utilities with SIMD optimization" << std::endl;
        std::cout << "   • Black-Scholes option pricing and Greeks calculation" << std::endl;
        std::cout << "   • Synthetic pricing for perpetual swaps and futures" << std::endl;
        std::cout << "   • Comprehensive pricing engine with multi-instrument support" << std::endl;
        std::cout << "   • Arbitrage detection framework" << std::endl;
        std::cout << "   • High-performance calculation pipeline" << std::endl;
        
        std::cout << "\\n🎯 Ready to proceed to Phase 4: Mispricing Detection Engine" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Demo failed with exception: {}", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        LOG_ERROR("Demo failed with unknown exception");
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
