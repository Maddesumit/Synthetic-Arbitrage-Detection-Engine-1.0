#include "AdvancedSyntheticStrategies.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>

namespace arbitrage {
namespace core {

AdvancedSyntheticStrategies::AdvancedSyntheticStrategies() {
    // Initialize default risk parameters
    risk_parameters_["max_position_size"] = 100000.0;
    risk_parameters_["max_leverage"] = 3.0;
    risk_parameters_["correlation_threshold"] = 0.7;
    risk_parameters_["volatility_threshold"] = 0.5;
    risk_parameters_["z_score_entry"] = 2.0;
    risk_parameters_["z_score_exit"] = 0.5;
    
    // Initialize strategy limits
    strategy_limits_["multi_leg_max_legs"] = 5.0;
    strategy_limits_["max_strategies"] = 20.0;
    strategy_limits_["min_expected_profit"] = 0.01; // 1%
}

AdvancedSyntheticStrategies::~AdvancedSyntheticStrategies() = default;

void AdvancedSyntheticStrategies::initialize(
    std::shared_ptr<PricingEngine> pricing_engine,
    std::shared_ptr<core::ArbitrageEngine> arbitrage_engine) {
    
    pricing_engine_ = pricing_engine;
    arbitrage_engine_ = arbitrage_engine;
    
    utils::Logger::info("AdvancedSyntheticStrategies initialized with pricing and arbitrage engines");
}

// Multi-Leg Arbitrage Implementation (11.1)
std::vector<MultiLegPosition> AdvancedSyntheticStrategies::generateMultiLegOpportunities(
    const std::vector<data::MarketData>& market_data) {
    
    std::vector<MultiLegPosition> opportunities;
    
    if (market_data.size() < 2) {
        return opportunities;
    }
    
    utils::Logger::debug("Generating multi-leg arbitrage opportunities from {} market data points", 
                        market_data.size());
    
    // Generate 2-leg, 3-leg, and 4-leg strategies
    for (size_t i = 0; i < market_data.size(); ++i) {
        for (size_t j = i + 1; j < market_data.size(); ++j) {
            // 2-leg strategy
            std::vector<std::string> instruments = {market_data[i].symbol, market_data[j].symbol};
            std::vector<std::string> exchanges = {market_data[i].exchange, market_data[j].exchange};
            
            // Calculate optimal weights using correlation and volatility
            double correlation = calculateCorrelation(
                {market_data[i].last, market_data[i].bid, market_data[i].ask},
                {market_data[j].last, market_data[j].bid, market_data[j].ask}
            );
            
            if (std::abs(correlation) > risk_parameters_["correlation_threshold"]) {
                std::vector<double> weights = {1.0, -correlation}; // Long/short based on correlation
                
                MultiLegPosition strategy = createComplexSynthetic(instruments, exchanges, weights);
                if (strategy.expected_profit > strategy_limits_["min_expected_profit"]) {
                    opportunities.push_back(strategy);
                }
            }
            
            // 3-leg strategy if we have more data
            if (j + 1 < market_data.size()) {
                instruments.push_back(market_data[j + 1].symbol);
                exchanges.push_back(market_data[j + 1].exchange);
                std::vector<double> weights_3leg = {1.0, -0.5, -0.5};
                
                MultiLegPosition strategy_3leg = createComplexSynthetic(instruments, exchanges, weights_3leg);
                if (strategy_3leg.expected_profit > strategy_limits_["min_expected_profit"]) {
                    opportunities.push_back(strategy_3leg);
                }
            }
        }
    }
    
    // Sort by expected profit
    std::sort(opportunities.begin(), opportunities.end(),
              [](const MultiLegPosition& a, const MultiLegPosition& b) {
                  return a.expected_profit > b.expected_profit;
              });
    
    // Limit to max strategies
    if (opportunities.size() > static_cast<size_t>(strategy_limits_["max_strategies"])) {
        opportunities.resize(static_cast<size_t>(strategy_limits_["max_strategies"]));
    }
    
    utils::Logger::info("Generated {} multi-leg arbitrage opportunities", opportunities.size());
    return opportunities;
}

MultiLegPosition AdvancedSyntheticStrategies::createComplexSynthetic(
    const std::vector<std::string>& instruments,
    const std::vector<std::string>& exchanges,
    const std::vector<double>& weights) {
    
    MultiLegPosition position;
    position.strategy_id = "MULTI_LEG_" + std::to_string(active_strategies_.size() + 1);
    position.strategy_type = AdvancedStrategyType::MULTI_LEG_ARBITRAGE;
    position.instruments = instruments;
    position.exchanges = exchanges;
    position.weights = weights;
    position.timestamp = std::chrono::system_clock::now();
    
    // Calculate synthetic pricing and expected profit
    double total_value = 0.0;
    double total_risk = 0.0;
    position.required_capital = 0.0;
    
    for (size_t i = 0; i < instruments.size(); ++i) {
        // Simulate current market prices (in real implementation, get from market data)
        double current_price = 50000.0 * (1.0 + (static_cast<double>(rand()) / RAND_MAX - 0.5) * 0.02);
        double quantity = weights[i] * 1000.0; // Base quantity
        
        position.quantities.push_back(quantity);
        position.leg_prices[instruments[i]] = current_price;
        position.leg_volumes[instruments[i]] = std::abs(quantity);
        position.execution_costs[instruments[i]] = current_price * 0.001; // 0.1% execution cost
        
        total_value += weights[i] * current_price * std::abs(quantity);
        total_risk += std::pow(weights[i] * current_price * std::abs(quantity), 2);
        position.required_capital += std::abs(weights[i]) * current_price * std::abs(quantity);
    }
    
    position.risk_score = std::sqrt(total_risk) / position.required_capital;
    position.correlation_score = calculateCorrelation(weights, 
                                                     std::vector<double>(weights.size(), 1.0));
    
    // Calculate expected profit based on price convergence
    position.expected_profit = std::abs(total_value) * 0.005; // 0.5% expected convergence
    
    return position;
}

// Statistical Arbitrage Implementation (11.2)
std::vector<StatisticalSignal> AdvancedSyntheticStrategies::generateMeanReversionSignals(
    const std::vector<data::MarketData>& market_data) {
    
    std::vector<StatisticalSignal> signals;
    
    if (market_data.size() < 2) {
        return signals;
    }
    
    utils::Logger::debug("Generating mean reversion signals from {} market data points", 
                        market_data.size());
    
    for (size_t i = 0; i < market_data.size(); ++i) {
        for (size_t j = i + 1; j < market_data.size(); ++j) {
            StatisticalSignal signal;
            signal.signal_id = "MEAN_REV_" + market_data[i].symbol + "_" + market_data[j].symbol;
            signal.instrument_pair = market_data[i].symbol + "/" + market_data[j].symbol;
            
            // Create synthetic price series for analysis
            signal.price_series_1 = {market_data[i].last, market_data[i].bid, market_data[i].ask};
            signal.price_series_2 = {market_data[j].last, market_data[j].bid, market_data[j].ask};
            
            // Calculate spread
            signal.spread_series.clear();
            for (size_t k = 0; k < signal.price_series_1.size(); ++k) {
                signal.spread_series.push_back(signal.price_series_1[k] - signal.price_series_2[k]);
            }
            
            // Calculate z-score
            double spread_mean = std::accumulate(signal.spread_series.begin(), 
                                               signal.spread_series.end(), 0.0) / signal.spread_series.size();
            double current_spread = signal.spread_series.back();
            double spread_std = calculateStandardDeviation(signal.spread_series);
            
            signal.z_score = (current_spread - spread_mean) / spread_std;
            signal.mean_reversion_strength = 1.0 / (1.0 + std::abs(signal.z_score));
            signal.confidence_level = std::min(0.95, 0.5 + 0.1 * std::abs(signal.z_score));
            signal.entry_threshold = risk_parameters_["z_score_entry"];
            signal.exit_threshold = risk_parameters_["z_score_exit"];
            signal.timestamp = std::chrono::system_clock::now();
            
            // Generate signal type
            if (signal.z_score > signal.entry_threshold) {
                signal.signal_type = "SHORT"; // Spread too high, expect reversion
            } else if (signal.z_score < -signal.entry_threshold) {
                signal.signal_type = "LONG"; // Spread too low, expect reversion
            } else {
                signal.signal_type = "NEUTRAL";
            }
            
            if (signal.signal_type != "NEUTRAL") {
                signals.push_back(signal);
            }
        }
    }
    
    utils::Logger::info("Generated {} mean reversion signals", signals.size());
    return signals;
}

std::vector<StatisticalSignal> AdvancedSyntheticStrategies::findPairsTradingOpportunities(
    const std::vector<data::MarketData>& market_data) {
    
    std::vector<StatisticalSignal> opportunities;
    
    // Generate signals using mean reversion
    auto signals = generateMeanReversionSignals(market_data);
    
    for (const auto& signal : signals) {
        // Additional pairs trading criteria
        if (std::abs(signal.z_score) > 1.5 && signal.confidence_level > 0.7) {
            StatisticalSignal pairs_signal = signal;
            pairs_signal.signal_id = "PAIRS_" + signal.instrument_pair;
            
            // Calculate cointegration
            pairs_signal.cointegration_ratio = calculateCointegration(
                signal.price_series_1, signal.price_series_2);
            
            if (std::abs(pairs_signal.cointegration_ratio) > 0.6) {
                opportunities.push_back(pairs_signal);
            }
        }
    }
    
    utils::Logger::info("Found {} pairs trading opportunities", opportunities.size());
    return opportunities;
}

double AdvancedSyntheticStrategies::calculateCointegration(
    const std::vector<double>& series1, const std::vector<double>& series2) {
    
    if (series1.size() != series2.size() || series1.empty()) {
        return 0.0;
    }
    
    // Simple cointegration test using correlation of differences
    std::vector<double> diff1, diff2;
    for (size_t i = 1; i < series1.size(); ++i) {
        diff1.push_back(series1[i] - series1[i-1]);
        diff2.push_back(series2[i] - series2[i-1]);
    }
    
    return calculateCorrelation(diff1, diff2);
}

// Volatility Arbitrage Implementation (11.3)
VolatilitySurface AdvancedSyntheticStrategies::constructVolatilitySurface(
    const std::vector<data::MarketData>& options_data) {
    
    VolatilitySurface surface;
    surface.underlying_asset = "BTC"; // Default
    surface.spot_price = 50000.0; // Default spot price
    surface.risk_free_rate = 0.05; // 5% risk-free rate
    surface.timestamp = std::chrono::system_clock::now();
    
    // Generate synthetic volatility surface for demonstration
    std::vector<double> strikes = {40000, 45000, 50000, 55000, 60000};
    std::vector<double> expiries = {0.25, 0.5, 1.0}; // 3M, 6M, 1Y
    
    for (double strike : strikes) {
        for (double expiry : expiries) {
            // Simple volatility smile model
            double moneyness = strike / surface.spot_price;
            double base_vol = 0.5; // 50% base volatility
            double skew = 0.1 * (moneyness - 1.0); // Linear skew
            double vol = base_vol + skew + 0.05 * std::sin(moneyness * 3.14159);
            
            surface.surface[strike][expiry] = std::max(0.1, vol);
        }
        
        // Calculate skew
        double atm_vol = surface.surface[strike][0.5];
        surface.skew_data[strike] = (surface.surface[strike][0.25] - surface.surface[strike][1.0]) / atm_vol;
    }
    
    // Calculate term structure (ATM vols)
    for (double expiry : expiries) {
        surface.term_structure[expiry] = surface.surface[50000][expiry];
    }
    
    utils::Logger::info("Constructed volatility surface with {} strikes and {} expiries", 
                       strikes.size(), expiries.size());
    
    return surface;
}

std::vector<MultiLegPosition> AdvancedSyntheticStrategies::findVolatilityArbitrage(
    const VolatilitySurface& surface) {
    
    std::vector<MultiLegPosition> opportunities;
    
    // Look for volatility skew arbitrage
    for (const auto& [strike, skew] : surface.skew_data) {
        if (std::abs(skew) > 0.05) { // 5% skew threshold
            MultiLegPosition position;
            position.strategy_id = "VOL_SKEW_" + std::to_string(static_cast<int>(strike));
            position.strategy_type = AdvancedStrategyType::VOLATILITY_ARBITRAGE;
            position.instruments = {"BTC_CALL_" + std::to_string(static_cast<int>(strike)),
                                   "BTC_PUT_" + std::to_string(static_cast<int>(strike))};
            position.exchanges = {"binance", "okx"};
            
            // Skew strategy: long low vol, short high vol
            if (skew > 0) {
                position.weights = {1.0, -1.0}; // Long call, short put
            } else {
                position.weights = {-1.0, 1.0}; // Short call, long put
            }
            
            position.quantities = {10.0, 10.0}; // 10 contracts each
            position.expected_profit = std::abs(skew) * 1000.0; // Profit from skew convergence
            position.required_capital = strike * 0.1; // 10% margin
            position.risk_score = std::abs(skew);
            position.timestamp = std::chrono::system_clock::now();
            
            opportunities.push_back(position);
        }
    }
    
    utils::Logger::info("Found {} volatility arbitrage opportunities", opportunities.size());
    return opportunities;
}

double AdvancedSyntheticStrategies::calculateImpliedVolatility(
    double option_price, double spot_price, double strike, double expiry, double rate, bool is_call) {
    
    // Newton-Raphson method for implied volatility
    double vol = 0.5; // Initial guess
    double tolerance = 1e-6;
    int max_iterations = 100;
    
    for (int i = 0; i < max_iterations; ++i) {
        double price = is_call ? blackScholesCall(spot_price, strike, expiry, rate, vol)
                               : blackScholesPut(spot_price, strike, expiry, rate, vol);
        
        double vega = spot_price * std::sqrt(expiry) * normalPDF((std::log(spot_price/strike) + 
                      (rate + 0.5*vol*vol)*expiry) / (vol*std::sqrt(expiry)));
        
        if (std::abs(vega) < tolerance) break;
        
        double diff = price - option_price;
        if (std::abs(diff) < tolerance) break;
        
        vol -= diff / vega;
        vol = std::max(0.01, std::min(5.0, vol)); // Bound volatility
    }
    
    return vol;
}

// Cross-Asset Arbitrage Implementation (11.4)
CrossAssetCorrelation AdvancedSyntheticStrategies::calculateCrossAssetCorrelation(
    const std::vector<data::MarketData>& crypto_data,
    const std::vector<data::MarketData>& forex_data,
    const std::vector<data::MarketData>& commodity_data) {
    
    CrossAssetCorrelation correlation;
    correlation.timestamp = std::chrono::system_clock::now();
    
    // Combine all asset data
    std::vector<std::string> all_assets;
    std::map<std::string, std::vector<double>> all_prices;
    
    // Add crypto assets
    for (const auto& data : crypto_data) {
        all_assets.push_back("CRYPTO_" + data.symbol);
        all_prices["CRYPTO_" + data.symbol] = {data.last, data.bid, data.ask};
    }
    
    // Add forex assets
    for (const auto& data : forex_data) {
        all_assets.push_back("FOREX_" + data.symbol);
        all_prices["FOREX_" + data.symbol] = {data.last, data.bid, data.ask};
    }
    
    // Add commodity assets
    for (const auto& data : commodity_data) {
        all_assets.push_back("COMMODITY_" + data.symbol);
        all_prices["COMMODITY_" + data.symbol] = {data.last, data.bid, data.ask};
    }
    
    // Calculate correlation matrix
    for (const auto& asset1 : all_assets) {
        for (const auto& asset2 : all_assets) {
            if (all_prices.find(asset1) != all_prices.end() && 
                all_prices.find(asset2) != all_prices.end()) {
                correlation.correlation_matrix[asset1][asset2] = 
                    calculateCorrelation(all_prices[asset1], all_prices[asset2]);
            }
        }
    }
    
    // Calculate portfolio metrics
    correlation.portfolio_variance = 0.0;
    double total_weight = 0.0;
    
    for (const auto& asset : all_assets) {
        double weight = 1.0 / all_assets.size(); // Equal weight
        correlation.asset_weights[asset] = weight;
        correlation.volatilities[asset] = calculateStandardDeviation(all_prices[asset]);
        correlation.price_history[asset] = all_prices[asset];
        total_weight += weight;
    }
    
    // Calculate portfolio variance
    for (const auto& asset1 : all_assets) {
        for (const auto& asset2 : all_assets) {
            correlation.portfolio_variance += 
                correlation.asset_weights[asset1] * correlation.asset_weights[asset2] *
                correlation.volatilities[asset1] * correlation.volatilities[asset2] *
                correlation.correlation_matrix[asset1][asset2];
        }
    }
    
    correlation.portfolio_sharpe = 0.1 / std::sqrt(correlation.portfolio_variance); // Assume 10% return
    
    utils::Logger::info("Calculated cross-asset correlation matrix for {} assets", all_assets.size());
    return correlation;
}

// Helper Methods
double AdvancedSyntheticStrategies::calculateZScore(const std::vector<double>& series, double value) {
    if (series.empty()) return 0.0;
    
    double mean = std::accumulate(series.begin(), series.end(), 0.0) / series.size();
    double std_dev = calculateStandardDeviation(series);
    
    return std_dev > 0 ? (value - mean) / std_dev : 0.0;
}

std::vector<double> AdvancedSyntheticStrategies::calculateMovingAverage(
    const std::vector<double>& series, int window) {
    
    std::vector<double> ma;
    if (series.size() < static_cast<size_t>(window)) return ma;
    
    for (size_t i = window - 1; i < series.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < window; ++j) {
            sum += series[i - j];
        }
        ma.push_back(sum / window);
    }
    
    return ma;
}

double AdvancedSyntheticStrategies::calculateStandardDeviation(const std::vector<double>& series) {
    if (series.empty()) return 0.0;
    
    double mean = std::accumulate(series.begin(), series.end(), 0.0) / series.size();
    double variance = 0.0;
    
    for (double value : series) {
        variance += std::pow(value - mean, 2);
    }
    
    return std::sqrt(variance / series.size());
}

double AdvancedSyntheticStrategies::calculateCorrelation(
    const std::vector<double>& series1, const std::vector<double>& series2) {
    
    if (series1.size() != series2.size() || series1.empty()) return 0.0;
    
    double mean1 = std::accumulate(series1.begin(), series1.end(), 0.0) / series1.size();
    double mean2 = std::accumulate(series2.begin(), series2.end(), 0.0) / series2.size();
    
    double numerator = 0.0, sum1_sq = 0.0, sum2_sq = 0.0;
    
    for (size_t i = 0; i < series1.size(); ++i) {
        double diff1 = series1[i] - mean1;
        double diff2 = series2[i] - mean2;
        numerator += diff1 * diff2;
        sum1_sq += diff1 * diff1;
        sum2_sq += diff2 * diff2;
    }
    
    double denominator = std::sqrt(sum1_sq * sum2_sq);
    return denominator > 0 ? numerator / denominator : 0.0;
}

// Black-Scholes Implementation
double AdvancedSyntheticStrategies::normalCDF(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

double AdvancedSyntheticStrategies::normalPDF(double x) {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * M_PI);
}

double AdvancedSyntheticStrategies::blackScholesCall(
    double S, double K, double T, double r, double sigma) {
    
    if (T <= 0 || sigma <= 0) return std::max(0.0, S - K);
    
    double d1 = (std::log(S/K) + (r + 0.5*sigma*sigma)*T) / (sigma*std::sqrt(T));
    double d2 = d1 - sigma*std::sqrt(T);
    
    return S * normalCDF(d1) - K * std::exp(-r*T) * normalCDF(d2);
}

double AdvancedSyntheticStrategies::blackScholesPut(
    double S, double K, double T, double r, double sigma) {
    
    if (T <= 0 || sigma <= 0) return std::max(0.0, K - S);
    
    double d1 = (std::log(S/K) + (r + 0.5*sigma*sigma)*T) / (sigma*std::sqrt(T));
    double d2 = d1 - sigma*std::sqrt(T);
    
    return K * std::exp(-r*T) * normalCDF(-d2) - S * normalCDF(-d1);
}

// Strategy Management
void AdvancedSyntheticStrategies::addStrategy(const MultiLegPosition& strategy) {
    active_strategies_.push_back(strategy);
    utils::Logger::info("Added strategy: {}", strategy.strategy_id);
}

std::vector<MultiLegPosition> AdvancedSyntheticStrategies::getActiveStrategies() const {
    return active_strategies_;
}

std::map<std::string, double> AdvancedSyntheticStrategies::getStrategyPerformance() const {
    std::map<std::string, double> performance;
    
    double total_profit = 0.0;
    double total_capital = 0.0;
    
    for (const auto& strategy : active_strategies_) {
        total_profit += strategy.expected_profit;
        total_capital += strategy.required_capital;
        performance["strategy_" + strategy.strategy_id + "_profit"] = strategy.expected_profit;
        performance["strategy_" + strategy.strategy_id + "_risk"] = strategy.risk_score;
    }
    
    performance["total_expected_profit"] = total_profit;
    performance["total_required_capital"] = total_capital;
    performance["expected_return"] = total_capital > 0 ? total_profit / total_capital : 0.0;
    performance["active_strategies_count"] = static_cast<double>(active_strategies_.size());
    
    return performance;
}

std::vector<StatisticalSignal> AdvancedSyntheticStrategies::getActiveSignals() const {
    return active_signals_;
}

VolatilitySurface AdvancedSyntheticStrategies::getCurrentVolatilitySurface() const {
    return current_vol_surface_;
}

CrossAssetCorrelation AdvancedSyntheticStrategies::getCurrentCorrelationMatrix() const {
    return current_correlation_;
}

} // namespace core
} // namespace arbitrage
