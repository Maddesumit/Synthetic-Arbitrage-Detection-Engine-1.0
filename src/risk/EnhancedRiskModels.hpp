#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <Eigen/Dense>

// Include the actual definitions instead of forward declarations
#include "../core/RiskManager.hpp"
#include "../core/PnLTracker.hpp"

namespace arbitrage {
namespace risk {

// Use the actual types from their respective namespaces
using RiskMetrics = ArbitrageEngine::RiskMetrics;
using Position = arbitrage::core::Position;

// Advanced correlation analysis
class CorrelationAnalyzer {
public:
    struct CorrelationResult {
        double correlation_coefficient;
        double p_value;
        double confidence_interval_lower;
        double confidence_interval_upper;
        std::chrono::system_clock::time_point calculated_at;
    };
    
    struct RollingCorrelation {
        std::vector<double> values;
        std::chrono::seconds window_size;
        std::chrono::system_clock::time_point last_update;
    };
    
    CorrelationAnalyzer(size_t max_history_size = 10000);
    
    // Correlation calculations
    CorrelationResult calculateCorrelation(const std::string& asset1, 
                                         const std::string& asset2,
                                         std::chrono::seconds lookback = std::chrono::hours(24)) const;
    
    RollingCorrelation calculateRollingCorrelation(const std::string& asset1,
                                                 const std::string& asset2,
                                                 std::chrono::seconds window = std::chrono::hours(1),
                                                 std::chrono::seconds step = std::chrono::minutes(5)) const;
    
    // Matrix operations for portfolio correlation
    Eigen::MatrixXd getCorrelationMatrix(const std::vector<std::string>& assets,
                                       std::chrono::seconds lookback = std::chrono::hours(24)) const;
    
    // Dynamic correlation monitoring
    void updatePriceData(const std::string& asset, double price, 
                        std::chrono::system_clock::time_point timestamp);
    
    // Regime detection based on correlation changes
    enum class MarketRegime {
        NORMAL,
        HIGH_CORRELATION,
        CRISIS,
        DECORRELATION,
        UNKNOWN
    };
    
    MarketRegime detectMarketRegime(const std::vector<std::string>& assets) const;
    double getRegimeConfidence(MarketRegime regime, const std::vector<std::string>& assets) const;
    
    // Correlation-based risk metrics
    double calculateConcentrationRisk(const std::vector<std::string>& assets,
                                    const std::vector<double>& weights) const;
    
    double calculateDiversificationRatio(const std::vector<std::string>& assets,
                                        const std::vector<double>& weights) const;
    
private:
    struct PriceHistory {
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> prices;
        size_t max_size;
    };
    
    std::unordered_map<std::string, PriceHistory> price_history_;
    size_t max_history_size_;
    
    // Helper methods
    std::vector<double> getReturns(const std::string& asset, 
                                 std::chrono::seconds lookback) const;
    double calculatePearsonCorrelation(const std::vector<double>& x, 
                                     const std::vector<double>& y) const;
    double calculateSpearmanCorrelation(const std::vector<double>& x,
                                      const std::vector<double>& y) const;
};

// Volatility modeling and prediction
class VolatilityPredictor {
public:
    enum class Model {
        EWMA,           // Exponentially Weighted Moving Average
        GARCH,          // GARCH(1,1)
        EGARCH,         // Exponential GARCH
        REALIZED_VOL,   // Realized Volatility
        JUMP_DIFFUSION  // Jump Diffusion Model
    };
    
    struct VolatilityForecast {
        double predicted_volatility;
        double confidence_interval_lower;
        double confidence_interval_upper;
        Model model_used;
        double model_confidence;
        std::chrono::system_clock::time_point forecast_time;
        std::chrono::seconds forecast_horizon;
    };
    
    struct VolatilitySurface {
        std::vector<std::chrono::seconds> time_points;
        std::vector<double> volatilities;
        std::vector<double> confidence_bands;
        std::chrono::system_clock::time_point created_at;
    };
    
    VolatilityPredictor();
    
    // Model calibration and prediction
    bool calibrateModel(const std::string& asset, Model model,
                       const std::vector<double>& returns);
    
    VolatilityForecast predictVolatility(const std::string& asset,
                                       std::chrono::seconds horizon,
                                       Model model = Model::GARCH) const;
    
    VolatilitySurface generateVolatilitySurface(const std::string& asset,
                                              std::chrono::seconds max_horizon = std::chrono::hours(24),
                                              size_t num_points = 100) const;
    
    // Real-time volatility estimation
    void updateReturns(const std::string& asset, double return_value,
                      std::chrono::system_clock::time_point timestamp);
    
    double getRealizedVolatility(const std::string& asset,
                               std::chrono::seconds window = std::chrono::hours(24)) const;
    
    // Volatility regime detection
    enum class VolatilityRegime {
        LOW,
        NORMAL,
        HIGH,
        EXTREME,
        UNKNOWN
    };
    
    VolatilityRegime getVolatilityRegime(const std::string& asset) const;
    double getRegimeTransitionProbability(const std::string& asset,
                                        VolatilityRegime from,
                                        VolatilityRegime to) const;
    
    // Model comparison and selection
    struct ModelPerformance {
        Model model;
        double mse;
        double mae;
        double directional_accuracy;
        double log_likelihood;
    };
    
    std::vector<ModelPerformance> compareModels(const std::string& asset,
                                              const std::vector<Model>& models) const;
    
    Model selectBestModel(const std::string& asset) const;
    
private:
    struct ModelParameters {
        Model type;
        std::vector<double> parameters;
        double log_likelihood;
        std::chrono::system_clock::time_point calibrated_at;
    };
    
    struct ReturnHistory {
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> returns;
        size_t max_size;
    };
    
    std::unordered_map<std::string, std::unordered_map<Model, ModelParameters>> model_parameters_;
    std::unordered_map<std::string, ReturnHistory> return_history_;
    
    // Model implementations
    double ewmaVolatility(const std::vector<double>& returns, double lambda = 0.94) const;
    VolatilityForecast garchForecast(const std::string& asset, 
                                   std::chrono::seconds horizon) const;
    VolatilityForecast egarchForecast(const std::string& asset,
                                    std::chrono::seconds horizon) const;
    VolatilityForecast realizedVolForecast(const std::string& asset,
                                         std::chrono::seconds horizon) const;
    
    // Calibration helpers
    bool calibrateGarch(const std::string& asset, const std::vector<double>& returns);
    bool calibrateEgarch(const std::string& asset, const std::vector<double>& returns);
    
    // Statistical utilities
    double calculateMSE(const std::vector<double>& actual, 
                       const std::vector<double>& predicted) const;
    double calculateMAE(const std::vector<double>& actual,
                       const std::vector<double>& predicted) const;
    double calculateDirectionalAccuracy(const std::vector<double>& actual,
                                      const std::vector<double>& predicted) const;
};

// Market regime detection
class MarketRegimeDetector {
public:
    enum class Regime {
        BULL_MARKET,
        BEAR_MARKET,
        SIDEWAYS,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        CRISIS,
        RECOVERY,
        UNKNOWN
    };
    
    struct RegimeDetection {
        Regime current_regime;
        double confidence;
        std::chrono::system_clock::time_point detected_at;
        std::chrono::seconds regime_duration;
        double transition_probability;
    };
    
    struct RegimeIndicators {
        double volatility_level;
        double trend_strength;
        double correlation_level;
        double liquidity_score;
        double momentum_score;
    };
    
    MarketRegimeDetector();
    
    // Regime detection methods
    RegimeDetection detectCurrentRegime(const std::vector<std::string>& assets) const;
    
    RegimeIndicators calculateIndicators(const std::vector<std::string>& assets) const;
    
    // Regime transition analysis
    std::unordered_map<Regime, double> getRegimeTransitionProbabilities(
        Regime current_regime) const;
    
    std::chrono::seconds estimateRegimeDuration(Regime regime) const;
    
    // Historical regime analysis
    std::vector<std::pair<Regime, std::chrono::seconds>> getRegimeHistory(
        std::chrono::seconds lookback = std::chrono::hours(24 * 30)) const;
    
    // Real-time updates
    void updateMarketData(const std::string& asset, double price, double volume,
                         std::chrono::system_clock::time_point timestamp);
    
    // Risk adjustment based on regime
    double getRegimeRiskMultiplier(Regime regime) const;
    
    struct RiskAdjustment {
        double position_size_multiplier;
        double leverage_adjustment;
        double stop_loss_adjustment;
        double profit_target_adjustment;
    };
    
    RiskAdjustment getRegimeRiskAdjustment(Regime regime) const;
    
private:
    struct MarketData {
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> prices;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> volumes;
        size_t max_size;
    };
    
    std::unordered_map<std::string, MarketData> market_data_;
    std::vector<std::pair<std::chrono::system_clock::time_point, Regime>> regime_history_;
    
    // Regime detection algorithms
    Regime detectTrendRegime(const std::vector<std::string>& assets) const;
    Regime detectVolatilityRegime(const std::vector<std::string>& assets) const;
    Regime detectLiquidityRegime(const std::vector<std::string>& assets) const;
    
    // Indicator calculations
    double calculateTrendStrength(const std::string& asset) const;
    double calculateVolatilityLevel(const std::string& asset) const;
    double calculateLiquidityScore(const std::string& asset) const;
    double calculateMomentumScore(const std::string& asset) const;
    
    // Regime combination logic
    Regime combineRegimeSignals(const std::vector<Regime>& signals,
                              const std::vector<double>& weights) const;
};

// Main Enhanced Risk Models class
class EnhancedRiskModels {
public:
    EnhancedRiskModels();
    
    // Market data updates
    void updateMarketData(const std::string& asset, double price, double volume,
                         std::chrono::system_clock::time_point timestamp);
    
    // Risk calculations
    double calculateVaR(const std::string& symbol, double position_value);
    double calculateExpectedShortfall(const std::string& symbol, double position_value);
    RiskMetrics calculatePortfolioRisk(const std::vector<Position>& positions);
    
    // Advanced risk metrics
    double calculateMaxDrawdown(const std::vector<Position>& positions);
    double calculateSharpeRatio(const std::vector<Position>& positions);
    double calculateBeta(const std::vector<Position>& positions);
    
    // Real-time volatility modeling
    void updateRealTimeVolatility(const std::string& symbol);
    
    // Monte Carlo simulation
    std::vector<double> runMonteCarloSimulation(
        const std::vector<Position>& positions,
        int simulations,
        int days_ahead);
    
    // Configuration
    void setConfidenceLevel(double level);
    void setLookbackPeriod(int days);
    void setMonteCarloSimulations(int simulations);

private:
    struct MarketData {
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> prices;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> volumes;
        size_t max_size;
    };
    
    std::unordered_map<std::string, MarketData> market_data_;
    std::unordered_map<std::string, double> volatility_cache_;
    std::unordered_map<std::string, std::vector<double>> returns_cache_;
    
    // Legacy data structures used in implementation
    std::unordered_map<std::string, std::vector<double>> historical_prices_;
    std::unordered_map<std::string, std::vector<double>> historical_volumes_;
    
    double confidence_level_;
    int lookback_period_;
    int monte_carlo_simulations_;
    
    // Thread safety
    mutable std::mutex data_mutex_;
    
    // Helper methods
    std::vector<double> calculateReturns(const std::string& symbol);
    double calculateVolatility(const std::vector<double>& returns);
    double calculateMean(const std::vector<double>& data);
    double calculateStandardDeviation(const std::vector<double>& data);
};

} // namespace risk
} // namespace arbitrage
