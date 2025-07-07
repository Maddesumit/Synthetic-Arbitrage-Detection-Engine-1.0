#pragma once

#include "MathUtils.hpp"
#include "data/MarketData.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>
#include <shared_mutex>

namespace arbitrage {
namespace core {

/**
 * @brief Instrument types supported by the pricing engine
 */
enum class InstrumentType {
    SPOT,
    PERPETUAL_SWAP,
    FUTURES,
    CALL_OPTION,
    PUT_OPTION
};

/**
 * @brief Instrument specification
 */
struct InstrumentSpec {
    std::string symbol;
    InstrumentType type;
    data::Exchange exchange;
    
    // For derivatives
    std::string underlying_symbol;
    double strike_price = 0.0;          // For options
    std::chrono::system_clock::time_point expiry_time; // For futures/options
    
    // Additional parameters
    double contract_size = 1.0;         // Contract multiplier
    std::string quote_currency = "USDT"; // Quote currency
    
    InstrumentSpec() = default;
    InstrumentSpec(const std::string& sym, InstrumentType t, data::Exchange ex)
        : symbol(sym), type(t), exchange(ex) {}
};

/**
 * @brief Pricing result containing synthetic price and metadata
 */
struct PricingResult {
    double synthetic_price = 0.0;
    double confidence = 0.0;           // Confidence in the pricing (0-1)
    std::string pricing_model;         // Model used for pricing
    std::string model_name;            // Model name for UI compatibility
    std::string instrument_id;         // Instrument identifier
    bool success = false;              // Whether pricing was successful
    double calculation_time_ms = 0.0;  // Calculation time in milliseconds
    std::chrono::system_clock::time_point timestamp;
    
    // Pricing components (for analysis)
    struct Components {
        double base_price = 0.0;       // Underlying/spot price
        double carry_cost = 0.0;       // Cost of carry
        double funding_adjustment = 0.0; // Funding rate adjustment
        double volatility_component = 0.0; // Volatility premium
        double time_value = 0.0;       // Time value (for options)
    } components;
    
    // Greeks (for options)
    std::optional<MathUtils::Greeks> greeks;
    
    PricingResult() {
        timestamp = std::chrono::system_clock::now();
        success = true;  // Default to successful
        model_name = pricing_model;  // Keep in sync
    }
};

/**
 * @brief Market environment parameters
 */
struct MarketEnvironment {
    double risk_free_rate = 0.05;     // Risk-free interest rate (5%)
    double default_volatility = 0.3;   // Default volatility (30%)
    
    // Currency-specific parameters
    std::unordered_map<std::string, double> interest_rates;
    std::unordered_map<std::string, double> convenience_yields;
    
    MarketEnvironment() {
        // Initialize default interest rates
        interest_rates["USD"] = 0.05;
        interest_rates["USDT"] = 0.05;
        interest_rates["BTC"] = 0.0;
        interest_rates["ETH"] = 0.0;
    }
};

/**
 * @brief High-performance synthetic pricing engine
 */
class PricingEngine {
public:
    /**
     * @brief Constructor
     * @param market_env Market environment parameters
     */
    explicit PricingEngine(const MarketEnvironment& market_env = MarketEnvironment{});
    
    /**
     * @brief Destructor
     */
    ~PricingEngine();
    
    /**
     * @brief Register instrument for pricing
     * @param spec Instrument specification
     * @return true if registration successful
     */
    bool registerInstrument(const InstrumentSpec& spec);
    
    /**
     * @brief Unregister instrument
     * @param symbol Instrument symbol
     * @return true if unregistration successful
     */
    bool unregisterInstrument(const std::string& symbol);
    
    /**
     * @brief Calculate synthetic price for an instrument
     * @param symbol Instrument symbol
     * @param market_data Current market data
     * @return Pricing result
     */
    PricingResult calculateSyntheticPrice(const std::string& symbol, 
                                         const data::MarketData& market_data);
    
    /**
     * @brief Calculate synthetic prices for all registered instruments
     * @param market_data Current market data
     * @return Map of symbol to pricing result
     */
    std::unordered_map<std::string, PricingResult> 
    calculateAllSyntheticPrices(const data::MarketData& market_data);
    
    /**
     * @brief Update market environment
     * @param market_env New market environment
     */
    void updateMarketEnvironment(const MarketEnvironment& market_env);
    
    /**
     * @brief Get current market environment
     */
    const MarketEnvironment& getMarketEnvironment() const;
    
    /**
     * @brief Set volatility for a specific symbol
     * @param symbol Symbol
     * @param volatility Volatility (annualized)
     */
    void setVolatility(const std::string& symbol, double volatility);
    
    /**
     * @brief Get volatility for a symbol
     * @param symbol Symbol
     * @return Volatility (uses default if not set)
     */
    double getVolatility(const std::string& symbol) const;
    
    /**
     * @brief Enable/disable SIMD optimization
     */
    void setSIMDEnabled(bool enabled) { simd_enabled_ = enabled; }
    
    /**
     * @brief Get pricing statistics
     */
    struct Statistics {
        size_t total_calculations = 0;
        size_t successful_calculations = 0;
        double average_calculation_time_ms = 0.0;
        size_t registered_instruments = 0;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    /**
     * @brief Price spot instruments
     */
    PricingResult priceSpot(const InstrumentSpec& spec, const data::MarketData& market_data);
    
    /**
     * @brief Price perpetual swaps
     */
    PricingResult pricePerpetualSwap(const InstrumentSpec& spec, const data::MarketData& market_data);
    
    /**
     * @brief Price futures contracts
     */
    PricingResult priceFutures(const InstrumentSpec& spec, const data::MarketData& market_data);
    
    /**
     * @brief Price options
     */
    PricingResult priceOption(const InstrumentSpec& spec, const data::MarketData& market_data, bool is_call);
    
    /**
     * @brief Get underlying price from market data
     */
    double getUnderlyingPrice(const std::string& symbol, const data::MarketData& market_data);
    
    /**
     * @brief Get funding rate from market data
     */
    double getFundingRate(const std::string& symbol, data::Exchange exchange, 
                         const data::MarketData& market_data);
    
    /**
     * @brief Calculate time to expiry in years
     */
    double getTimeToExpiry(const std::chrono::system_clock::time_point& expiry_time);
    
    /**
     * @brief Get interest rate for currency
     */
    double getInterestRate(const std::string& currency) const;
    
    /**
     * @brief Update pricing statistics
     */
    void updateStatistics(bool success, double calculation_time_ms);
    
    // Configuration
    MarketEnvironment market_env_;
    bool simd_enabled_ = true;
    
    // Registered instruments
    std::unordered_map<std::string, InstrumentSpec> instruments_;
    mutable std::shared_mutex instruments_mutex_;
    
    // Volatility data
    std::unordered_map<std::string, double> volatilities_;
    mutable std::shared_mutex volatilities_mutex_;
    
    // Statistics
    mutable Statistics statistics_;
    mutable std::mutex statistics_mutex_;
    
    // Calculation pipeline
    std::unique_ptr<CalculationPipeline> calculation_pipeline_;
    
    // Memory pools for optimization
    std::unique_ptr<MemoryPool<PricingResult>> pricing_result_pool_;
};

/**
 * @brief Arbitrage opportunity detection result
 */
struct ArbitrageOpportunity {
    std::string underlying_symbol;
    
    // Instruments involved
    struct Leg {
        std::string symbol;
        InstrumentType type;
        data::Exchange exchange;
        double price;
        double synthetic_price;
        double deviation;              // (price - synthetic_price) / synthetic_price
        std::string action;            // "BUY" or "SELL"
    };
    
    std::vector<Leg> legs;
    
    // Opportunity metrics
    double expected_profit_pct = 0.0;  // Expected profit percentage
    double required_capital = 0.0;     // Required capital
    double risk_score = 0.0;          // Risk score (0-1, lower is better)
    double confidence = 0.0;          // Confidence in opportunity (0-1)
    
    std::chrono::system_clock::time_point detected_at;
    
    ArbitrageOpportunity() {
        detected_at = std::chrono::system_clock::now();
    }
};

/**
 * @brief Arbitrage detector for identifying mispricings
 */
class ArbitrageDetector {
public:
    /**
     * @brief Detection parameters
     */
    struct Parameters {
        double min_profit_threshold;           // Minimum profit threshold (0.1%)
        double max_risk_score;                 // Maximum acceptable risk score
        double min_confidence;                 // Minimum confidence threshold
        bool enable_cross_exchange;            // Enable cross-exchange arbitrage
        bool enable_synthetic_arbitrage;       // Enable synthetic arbitrage
        
        // Default constructor with default values
        Parameters() 
            : min_profit_threshold(0.001)
            , max_risk_score(0.5)
            , min_confidence(0.7)
            , enable_cross_exchange(true)
            , enable_synthetic_arbitrage(true) {}
    };
    
    /**
     * @brief Constructor
     * @param pricing_engine Shared pricing engine
     * @param params Detection parameters
     */
    ArbitrageDetector(std::shared_ptr<PricingEngine> pricing_engine,
                     const Parameters& params = Parameters{});
    
    /**
     * @brief Detect arbitrage opportunities
     * @param market_data Current market data
     * @return Vector of detected opportunities
     */
    std::vector<ArbitrageOpportunity> 
    detectOpportunities(const data::MarketData& market_data);
    
    /**
     * @brief Update detection parameters
     */
    void updateParameters(const Parameters& params);
    
    /**
     * @brief Get current parameters
     */
    const Parameters& getParameters() const { return params_; }

private:
    /**
     * @brief Detect spot vs synthetic spot arbitrage
     */
    std::vector<ArbitrageOpportunity> 
    detectSpotArbitrage(const data::MarketData& market_data);
    
    /**
     * @brief Detect derivative vs synthetic derivative arbitrage
     */
    std::vector<ArbitrageOpportunity> 
    detectDerivativeArbitrage(const data::MarketData& market_data);
    
    /**
     * @brief Calculate risk score for an opportunity
     */
    double calculateRiskScore(const ArbitrageOpportunity& opportunity);
    
    std::shared_ptr<PricingEngine> pricing_engine_;
    Parameters params_;
    mutable std::shared_mutex params_mutex_;
};

} // namespace core
} // namespace arbitrage
