#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "../data/MarketData.hpp"
#include "../utils/Logger.hpp"
#include "PricingEngine.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief Extended arbitrage opportunity with additional fields
 * 
 * This structure extends the basic ArbitrageOpportunity from PricingEngine
 * with additional fields needed for comprehensive arbitrage analysis.
 */
struct ArbitrageOpportunityExtended : public ArbitrageOpportunity {
    // Enhanced Leg structure with additional fields
    struct Leg {
        std::string exchange;          // Exchange name (e.g., "binance")
        std::string instrument;        // Instrument name (e.g., "BTC-USDT")
        std::string action;           // "buy" or "sell"
        double quantity;              // Quantity to trade
        double price;                 // Execution price
        double weight;                // Weight in portfolio (0-1)
        
        // Additional leg-specific metrics
        double expected_slippage = 0.0;
        double transaction_cost = 0.0;
        bool is_synthetic = false;
    };
    
    // Override the legs with our enhanced version
    std::vector<Leg> legs;
    
    // Basic identification
    std::string id;                    // Unique identifier for this opportunity
    std::string instrument_symbol;     // e.g., "BTC-USDT"
    std::string exchange_a;            // First exchange (e.g., "binance")
    std::string exchange_b;            // Second exchange or "synthetic"
    
    // Price information
    double price_a;                    // Price on exchange A
    double price_b;                    // Price on exchange B (or synthetic price)
    double price_difference;           // Absolute price difference
    double percentage_spread;          // Percentage spread (price_diff / avg_price)
    
    // Profitability metrics
    double expected_profit_usd;        // Expected profit in USD
    double expected_profit_percent;    // Expected profit as percentage
    double risk_adjusted_return;       // Profit adjusted for risk
    
    // Execution requirements
    double max_position_size;          // Maximum size we can trade
    double execution_cost;             // Estimated transaction costs
    double slippage_cost;             // Estimated slippage
    
    // Risk metrics
    double confidence_score;           // How confident we are (0-1)
    double liquidity_score;           // How liquid this opportunity is (0-1)
    double volatility_risk;           // Risk from price volatility
    
    // Timing information
    std::chrono::milliseconds estimated_duration;
    std::chrono::milliseconds time_to_expiry;
    
    // Strategy information
    enum class StrategyType {
        SPOT_PERP_ARBITRAGE,          // Spot vs Perpetual
        FUNDING_RATE_ARBITRAGE,       // Funding rate differences
        CROSS_EXCHANGE_ARBITRAGE,     // Same instrument, different exchanges
        BASIS_ARBITRAGE,              // Futures vs spot
        VOLATILITY_ARBITRAGE,         // Options vs underlying
        STATISTICAL_ARBITRAGE         // Mean reversion strategies
    };
    StrategyType strategy_type;
    
    // Execution details
    struct ExecutionLeg {
        std::string exchange;
        std::string instrument;
        std::string side;             // "buy" or "sell"
        double quantity;
        double price;
        double weight;                // How much of total position
    };
    std::vector<ExecutionLeg> execution_legs;   // Multi-leg execution plan
    
    // Validation flags
    bool is_valid;                    // Passed all validation checks
    bool is_executable;               // Can be executed right now
    std::string validation_notes;     // Why invalid if not valid
    
    ArbitrageOpportunityExtended() : ArbitrageOpportunity() {
        // Initialize additional fields
        price_a = 0.0;
        price_b = 0.0;
        price_difference = 0.0;
        percentage_spread = 0.0;
        expected_profit_usd = 0.0;
        expected_profit_percent = 0.0;
        risk_adjusted_return = 0.0;
        max_position_size = 0.0;
        execution_cost = 0.0;
        slippage_cost = 0.0;
        confidence_score = 0.0;
        liquidity_score = 0.0;
        volatility_risk = 0.0;
        is_valid = false;
        is_executable = false;
        strategy_type = StrategyType::CROSS_EXCHANGE_ARBITRAGE;
    }
};

/**
 * @brief Configuration for arbitrage detection
 */
struct ArbitrageConfig {
    // Minimum thresholds
    double min_profit_threshold_usd = 10.0;        // Minimum $10 profit
    double min_profit_threshold_percent = 0.05;    // Minimum 0.05% profit
    double min_confidence_score = 0.7;             // Minimum 70% confidence
    double min_liquidity_score = 0.5;              // Minimum 50% liquidity
    
    // Risk limits
    double max_position_size_usd = 10000.0;        // Max $10k per trade
    double max_leverage = 3.0;                     // Max 3x leverage
    double max_correlation_risk = 0.8;             // Max correlation between legs
    
    // Timing constraints
    std::chrono::milliseconds max_detection_latency{100};    // Max 100ms to detect
    std::chrono::milliseconds min_opportunity_duration{5000}; // Min 5s duration
    
    // Strategy weights (for ranking)
    double spot_perp_weight = 1.0;
    double funding_rate_weight = 0.8;
    double cross_exchange_weight = 1.2;
    double basis_weight = 0.9;
    double volatility_weight = 0.7;
    double statistical_weight = 0.6;
};

/**
 * @brief Main arbitrage detection engine
 * 
 * This class is responsible for:
 * 1. Constructing synthetic instruments
 * 2. Comparing real vs synthetic prices
 * 3. Detecting and validating arbitrage opportunities
 * 4. Ranking opportunities by profitability and risk
 */
class ArbitrageEngine {
public:
    explicit ArbitrageEngine(const ArbitrageConfig& config = ArbitrageConfig{});
    ~ArbitrageEngine() = default;

    // Core detection methods
    std::vector<ArbitrageOpportunityExtended> detectOpportunities(
        const std::vector<data::MarketDataPoint>& market_data,
        const std::vector<PricingResult>& pricing_results
    );
    
    void updateMarketData(const std::vector<data::MarketDataPoint>& data);
    void updatePricingResults(const std::vector<PricingResult>& results);
    
    // Synthetic construction methods
    double constructSyntheticPerpetual(const std::string& symbol, const std::string& exchange);
    double constructSyntheticFuture(const std::string& symbol, const std::string& exchange, 
                                   const std::chrono::system_clock::time_point& expiry);
    double constructSyntheticOption(const std::string& symbol, const std::string& exchange,
                                   double strike, const std::chrono::system_clock::time_point& expiry);
    
    // Opportunity analysis methods
    double calculateExpectedProfit(const ArbitrageOpportunityExtended& opportunity);
    double calculateRiskAdjustedReturn(const ArbitrageOpportunityExtended& opportunity);
    double calculateConfidenceScore(const ArbitrageOpportunityExtended& opportunity);
    double calculateLiquidityScore(const ArbitrageOpportunityExtended& opportunity);
    
    // Validation methods
    bool validateOpportunity(ArbitrageOpportunityExtended& opportunity);
    bool checkLiquidityRequirements(const ArbitrageOpportunityExtended& opportunity);
    bool checkRiskLimits(const ArbitrageOpportunityExtended& opportunity);
    bool checkExecutionFeasibility(const ArbitrageOpportunityExtended& opportunity);
    
    // Ranking and filtering
    std::vector<ArbitrageOpportunityExtended> rankOpportunities(
        std::vector<ArbitrageOpportunityExtended> opportunities
    );
    std::vector<ArbitrageOpportunityExtended> filterOpportunities(
        const std::vector<ArbitrageOpportunityExtended>& opportunities
    );
    
    // Performance monitoring
    struct PerformanceMetrics {
        std::atomic<uint64_t> opportunities_detected{0};
        std::atomic<uint64_t> opportunities_validated{0};
        std::atomic<uint64_t> detection_cycles{0};
        std::atomic<double> avg_detection_latency_ms{0.0};
        std::atomic<double> total_expected_profit{0.0};
        
        std::chrono::system_clock::time_point last_update;
    };
    
    const PerformanceMetrics& getPerformanceMetrics() const { return performance_metrics_; }
    void resetPerformanceMetrics();
    
    // Configuration management
    void updateConfig(const ArbitrageConfig& config);
    const ArbitrageConfig& getConfig() const { return config_; }
    
    // State management
    bool isRunning() const { return is_running_.load(); }
    void start();
    void stop();

private:
    // Configuration
    ArbitrageConfig config_;
    
    // Data storage
    std::unordered_map<std::string, data::MarketDataPoint> latest_market_data_;
    std::unordered_map<std::string, PricingResult> latest_pricing_results_;
    std::mutex data_mutex_;
    
    // Performance tracking
    mutable PerformanceMetrics performance_metrics_;
    std::chrono::system_clock::time_point last_detection_cycle_;
    
    // State
    std::atomic<bool> is_running_{false};
    
    // Internal detection methods
    std::vector<ArbitrageOpportunityExtended> detectSpotPerpArbitrage();
    std::vector<ArbitrageOpportunityExtended> detectFundingRateArbitrage();
    std::vector<ArbitrageOpportunityExtended> detectCrossExchangeArbitrage();
    std::vector<ArbitrageOpportunityExtended> detectBasisArbitrage();
    std::vector<ArbitrageOpportunityExtended> detectVolatilityArbitrage();
    std::vector<ArbitrageOpportunityExtended> detectStatisticalArbitrage();
    
    // Helper methods
    std::string generateOpportunityId(const std::string& strategy, const std::string& symbol);
    double calculateTransactionCosts(const ArbitrageOpportunityExtended& opportunity);
    double calculateSlippage(const ArbitrageOpportunityExtended& opportunity);
    double getSpotPrice(const std::string& symbol, const std::string& exchange);
    double getPerpetualPrice(const std::string& symbol, const std::string& exchange);
    double getFundingRate(const std::string& symbol, const std::string& exchange);
    
    // Risk calculation helpers
    double calculateVolatilityRisk(const std::string& symbol);
    double calculateCorrelationRisk(const ArbitrageOpportunityExtended& opportunity);
    double calculateLiquidityRisk(const ArbitrageOpportunityExtended& opportunity);
    
    // Utility methods
    void updatePerformanceMetrics(const std::vector<ArbitrageOpportunityExtended>& opportunities);
    void logOpportunity(const ArbitrageOpportunityExtended& opportunity);
};

} // namespace core
} // namespace arbitrage
