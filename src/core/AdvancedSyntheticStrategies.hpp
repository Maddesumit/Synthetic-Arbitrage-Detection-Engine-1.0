#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <chrono>
#include "MarketData.hpp"
#include "PricingEngine.hpp"
#include "ArbitrageEngine.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief Advanced Strategy Types for Phase 11
 */
enum class AdvancedStrategyType {
    MULTI_LEG_ARBITRAGE,
    STATISTICAL_ARBITRAGE,
    VOLATILITY_ARBITRAGE,
    CROSS_ASSET_ARBITRAGE,
    CORRELATION_ARBITRAGE,
    MEAN_REVERSION,
    PAIRS_TRADING,
    COINTEGRATION,
    VOLATILITY_SURFACE
};

/**
 * @brief Multi-Leg Position representing complex arbitrage strategy
 */
struct MultiLegPosition {
    std::string strategy_id;
    AdvancedStrategyType strategy_type;
    std::vector<std::string> instruments;
    std::vector<std::string> exchanges;
    std::vector<double> weights;
    std::vector<double> quantities;
    double expected_profit;
    double required_capital;
    double risk_score;
    double correlation_score;
    std::chrono::system_clock::time_point timestamp;
    
    // Complex strategy metadata
    std::map<std::string, double> leg_prices;
    std::map<std::string, double> leg_volumes;
    std::map<std::string, double> execution_costs;
};

/**
 * @brief Statistical Arbitrage Signal
 */
struct StatisticalSignal {
    std::string signal_id;
    std::string instrument_pair;
    double z_score;
    double mean_reversion_strength;
    double cointegration_ratio;
    double confidence_level;
    double entry_threshold;
    double exit_threshold;
    std::string signal_type; // "LONG", "SHORT", "NEUTRAL"
    std::chrono::system_clock::time_point timestamp;
    
    // Time series data
    std::vector<double> price_series_1;
    std::vector<double> price_series_2;
    std::vector<double> spread_series;
};

/**
 * @brief Volatility Surface Data
 */
struct VolatilitySurface {
    std::string underlying_asset;
    std::map<double, std::map<double, double>> surface; // strike -> expiry -> implied_vol
    std::map<double, double> skew_data; // strike -> skew
    std::map<double, double> term_structure; // expiry -> atm_vol
    double spot_price;
    double risk_free_rate;
    std::chrono::system_clock::time_point timestamp;
    
    // Greeks
    std::map<std::string, double> delta_map;
    std::map<std::string, double> gamma_map;
    std::map<std::string, double> theta_map;
    std::map<std::string, double> vega_map;
};

/**
 * @brief Cross-Asset Correlation Data
 */
struct CrossAssetCorrelation {
    std::map<std::string, std::map<std::string, double>> correlation_matrix;
    std::map<std::string, double> asset_weights;
    std::map<std::string, double> volatilities;
    std::map<std::string, std::vector<double>> price_history;
    double portfolio_variance;
    double portfolio_sharpe;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Advanced Synthetic Strategies Engine
 * 
 * Implements sophisticated arbitrage strategies:
 * - Multi-leg arbitrage across instruments and exchanges
 * - Statistical arbitrage with mean reversion and pairs trading
 * - Volatility arbitrage with options and surface analysis
 * - Cross-asset arbitrage strategies
 */
class AdvancedSyntheticStrategies {
public:
    AdvancedSyntheticStrategies();
    ~AdvancedSyntheticStrategies();
    
    // Initialization
    void initialize(std::shared_ptr<PricingEngine> pricing_engine,
                   std::shared_ptr<core::ArbitrageEngine> arbitrage_engine);
    
    // Multi-Leg Arbitrage (11.1)
    std::vector<MultiLegPosition> generateMultiLegOpportunities(
        const std::vector<data::MarketData>& market_data);
    
    MultiLegPosition createComplexSynthetic(
        const std::vector<std::string>& instruments,
        const std::vector<std::string>& exchanges,
        const std::vector<double>& weights);
    
    std::vector<MultiLegPosition> findCrossAssetArbitrage(
        const std::vector<data::MarketData>& crypto_data,
        const std::vector<data::MarketData>& forex_data = {});
    
    double calculatePortfolioOptimization(const std::vector<MultiLegPosition>& positions);
    
    // Statistical Arbitrage (11.2)
    std::vector<StatisticalSignal> generateMeanReversionSignals(
        const std::vector<data::MarketData>& market_data);
    
    std::vector<StatisticalSignal> findPairsTradingOpportunities(
        const std::vector<data::MarketData>& market_data);
    
    double calculateCointegration(const std::vector<double>& series1,
                                 const std::vector<double>& series2);
    
    StatisticalSignal generateMLSignal(const std::string& instrument_pair,
                                      const std::vector<double>& features);
    
    std::vector<double> performTimeSeriesAnalysis(
        const std::vector<double>& price_series);
    
    // Volatility Arbitrage (11.3)
    VolatilitySurface constructVolatilitySurface(
        const std::vector<data::MarketData>& options_data);
    
    std::vector<MultiLegPosition> findVolatilityArbitrage(
        const VolatilitySurface& surface);
    
    double calculateImpliedVolatility(double option_price, double spot_price,
                                     double strike, double expiry, double rate,
                                     bool is_call = true);
    
    std::map<std::string, double> calculateGreeks(
        double spot_price, double strike, double expiry,
        double rate, double volatility, bool is_call = true);
    
    std::vector<MultiLegPosition> generateGammaHedgingStrategies(
        const VolatilitySurface& surface);
    
    // Cross-Asset Arbitrage (11.4)
    CrossAssetCorrelation calculateCrossAssetCorrelation(
        const std::vector<data::MarketData>& crypto_data,
        const std::vector<data::MarketData>& forex_data,
        const std::vector<data::MarketData>& commodity_data);
    
    std::vector<MultiLegPosition> findCurrencyArbitrage(
        const std::vector<data::MarketData>& market_data);
    
    std::vector<MultiLegPosition> findInterestRateArbitrage(
        const std::vector<data::MarketData>& funding_rate_data);
    
    std::vector<MultiLegPosition> findCommodityCryptoArbitrage(
        const std::vector<data::MarketData>& crypto_data,
        const std::vector<data::MarketData>& commodity_data);
    
    // Strategy Management
    void addStrategy(const MultiLegPosition& strategy);
    void removeStrategy(const std::string& strategy_id);
    std::vector<MultiLegPosition> getActiveStrategies() const;
    
    // Performance Analytics
    std::map<std::string, double> getStrategyPerformance() const;
    std::map<std::string, double> getRiskMetrics() const;
    std::vector<StatisticalSignal> getActiveSignals() const;
    VolatilitySurface getCurrentVolatilitySurface() const;
    CrossAssetCorrelation getCurrentCorrelationMatrix() const;
    
    // Strategy Optimization
    void optimizePortfolio();
    void rebalanceStrategies();
    void updateRiskLimits(const std::map<std::string, double>& limits);
    
private:
    // Core engines
    std::shared_ptr<PricingEngine> pricing_engine_;
    std::shared_ptr<core::ArbitrageEngine> arbitrage_engine_;
    
    // Strategy storage
    std::vector<MultiLegPosition> active_strategies_;
    std::vector<StatisticalSignal> active_signals_;
    VolatilitySurface current_vol_surface_;
    CrossAssetCorrelation current_correlation_;
    
    // Configuration
    std::map<std::string, double> strategy_limits_;
    std::map<std::string, double> risk_parameters_;
    
    // Helper methods
    double calculateZScore(const std::vector<double>& series, double value);
    std::vector<double> calculateMovingAverage(const std::vector<double>& series, int window);
    double calculateStandardDeviation(const std::vector<double>& series);
    double calculateCorrelation(const std::vector<double>& series1, 
                               const std::vector<double>& series2);
    
    // Black-Scholes helpers
    double normalCDF(double x);
    double normalPDF(double x);
    double blackScholesCall(double S, double K, double T, double r, double sigma);
    double blackScholesPut(double S, double K, double T, double r, double sigma);
    
    // Optimization helpers
    std::vector<double> solveQuadraticProgramming(
        const std::vector<std::vector<double>>& Q,
        const std::vector<double>& c,
        const std::vector<std::vector<double>>& A,
        const std::vector<double>& b);
};

} // namespace core
} // namespace arbitrage
