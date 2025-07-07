#include "EnhancedRiskModels.hpp"
#include "../core/PnLTracker.hpp"
#include "../core/RiskManager.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace arbitrage {
namespace risk {

EnhancedRiskModels::EnhancedRiskModels() 
    : confidence_level_(0.95), 
      lookback_period_(252),
      monte_carlo_simulations_(10000) {
}

void EnhancedRiskModels::updateMarketData(const std::string& asset, double price, double volume,
                                         std::chrono::system_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Store historical data for risk calculations
    historical_prices_[asset].push_back(price);
    historical_volumes_[asset].push_back(volume);
    
    // Maintain rolling window
    if (historical_prices_[asset].size() > static_cast<size_t>(lookback_period_)) {
        historical_prices_[asset].erase(historical_prices_[asset].begin());
        historical_volumes_[asset].erase(historical_volumes_[asset].begin());
    }
    
    // Update real-time volatility
    updateRealTimeVolatility(asset);
}

double EnhancedRiskModels::calculateVaR(const std::string& symbol, double position_value) {
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 30) {
        return 0.0; // Not enough data
    }
    
    // Calculate returns
    std::vector<double> returns = calculateReturns(symbol);
    if (returns.empty()) {
        return 0.0;
    }
    
    // Sort returns to find percentile
    std::sort(returns.begin(), returns.end());
    
    // Calculate VaR at confidence level
    size_t index = static_cast<size_t>((1.0 - confidence_level_) * returns.size());
    double var_return = returns[index];
    
    return std::abs(var_return * position_value);
}

double EnhancedRiskModels::calculateExpectedShortfall(const std::string& symbol, double position_value) {
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 30) {
        return 0.0;
    }
    
    std::vector<double> returns = calculateReturns(symbol);
    if (returns.empty()) {
        return 0.0;
    }
    
    std::sort(returns.begin(), returns.end());
    
    // Calculate Expected Shortfall (average of worst returns beyond VaR)
    size_t var_index = static_cast<size_t>((1.0 - confidence_level_) * returns.size());
    double sum = 0.0;
    for (size_t i = 0; i < var_index; ++i) {
        sum += returns[i];
    }
    
    double expected_shortfall = (var_index > 0) ? sum / var_index : 0.0;
    return std::abs(expected_shortfall * position_value);
}

RiskMetrics EnhancedRiskModels::calculatePortfolioRisk(const std::vector<Position>& positions) {
    RiskMetrics metrics;
    
    if (positions.empty()) {
        return metrics;
    }
    
    double total_var = 0.0;
    double total_exposure = 0.0;
    double total_value = 0.0;
    
    for (const auto& position : positions) {
        double position_value = std::abs(position.quantity * position.current_market_price);
        total_exposure += position_value;
        total_value += position.quantity * position.current_market_price;
        
        // Calculate VaR for each position
        double position_var = calculateVaR(position.symbol, position_value);
        total_var += position_var * position_var; // Simple aggregation
    }
    
    metrics.portfolioVaR = std::sqrt(total_var);
    metrics.totalExposure = total_exposure;
    metrics.maxDrawdown = calculateMaxDrawdown(positions);
    metrics.portfolioBeta = calculateBeta(positions);
    metrics.timestamp = std::chrono::system_clock::now();
    
    return metrics;
}

double EnhancedRiskModels::calculateMaxDrawdown(const std::vector<Position>& positions) {
    if (positions.empty()) {
        return 0.0;
    }
    
    double max_drawdown = 0.0;
    
    for (const auto& position : positions) {
        auto it = historical_prices_.find(position.symbol);
        if (it != historical_prices_.end() && !it->second.empty()) {
            const auto& prices = it->second;
            
            double peak = prices[0];
            double current_drawdown = 0.0;
            
            for (size_t i = 1; i < prices.size(); ++i) {
                if (prices[i] > peak) {
                    peak = prices[i];
                } else {
                    current_drawdown = (peak - prices[i]) / peak;
                    max_drawdown = std::max(max_drawdown, current_drawdown);
                }
            }
        }
    }
    
    return max_drawdown;
}

double EnhancedRiskModels::calculateSharpeRatio(const std::vector<Position>& positions) {
    if (positions.empty()) {
        return 0.0;
    }
    
    double total_return = 0.0;
    double total_volatility = 0.0;
    size_t valid_positions = 0;
    
    for (const auto& position : positions) {
        auto it = historical_prices_.find(position.symbol);
        if (it != historical_prices_.end() && it->second.size() > 1) {
            std::vector<double> returns = calculateReturns(position.symbol);
            if (!returns.empty()) {
                double mean_return = calculateMean(returns);
                double volatility = calculateStandardDeviation(returns);
                
                total_return += mean_return;
                total_volatility += volatility;
                valid_positions++;
            }
        }
    }
    
    if (valid_positions == 0 || total_volatility == 0.0) {
        return 0.0;
    }
    
    double avg_return = total_return / valid_positions;
    double avg_volatility = total_volatility / valid_positions;
    
    // Assume risk-free rate of 2% annually (adjust as needed)
    double risk_free_rate = 0.02 / 252.0; // Daily rate
    
    return (avg_return - risk_free_rate) / avg_volatility;
}

double EnhancedRiskModels::calculateBeta(const std::vector<Position>& positions) {
    // Simplified beta calculation - would need market index data for proper implementation
    return 1.0;
}

void EnhancedRiskModels::updateRealTimeVolatility(const std::string& symbol) {
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 20) {
        return;
    }
    
    std::vector<double> returns = calculateReturns(symbol);
    if (returns.empty()) {
        return;
    }
    
    double volatility = calculateVolatility(returns);
    volatility_cache_[symbol] = volatility;
}

std::vector<double> EnhancedRiskModels::runMonteCarloSimulation(
    const std::vector<Position>& positions,
    int simulations,
    int days_ahead) {
    
    std::vector<double> simulated_prices;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0.0, 1.0);
    
    for (const auto& position : positions) {
        auto it = historical_prices_.find(position.symbol);
        if (it != historical_prices_.end() && !it->second.empty()) {
            double current_price = it->second.back();
            
            // Get or calculate volatility
            double volatility = 0.02; // Default 2% daily volatility
            auto vol_it = volatility_cache_.find(position.symbol);
            if (vol_it != volatility_cache_.end()) {
                volatility = vol_it->second;
            }
            
            // Run simulations
            for (int sim = 0; sim < simulations; ++sim) {
                double price = current_price;
                for (int day = 0; day < days_ahead; ++day) {
                    double random_shock = dis(gen) * volatility;
                    price *= (1.0 + random_shock);
                }
                simulated_prices.push_back(price);
            }
        }
    }
    
    std::sort(simulated_prices.begin(), simulated_prices.end());
    return simulated_prices;
}

void EnhancedRiskModels::setConfidenceLevel(double level) {
    if (level > 0.0 && level < 1.0) {
        confidence_level_ = level;
    }
}

void EnhancedRiskModels::setLookbackPeriod(int days) {
    if (days > 0) {
        lookback_period_ = days;
    }
}

void EnhancedRiskModels::setMonteCarloSimulations(int simulations) {
    if (simulations > 0) {
        monte_carlo_simulations_ = simulations;
    }
}

// Helper methods
std::vector<double> EnhancedRiskModels::calculateReturns(const std::string& symbol) {
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 2) {
        return {};
    }
    
    const auto& prices = it->second;
    std::vector<double> returns;
    
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i-1] != 0.0) {
            double return_val = (prices[i] - prices[i-1]) / prices[i-1];
            returns.push_back(return_val);
        }
    }
    
    return returns;
}

double EnhancedRiskModels::calculateVolatility(const std::vector<double>& returns) {
    if (returns.empty()) {
        return 0.0;
    }
    
    return calculateStandardDeviation(returns);
}

double EnhancedRiskModels::calculateMean(const std::vector<double>& data) {
    if (data.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double EnhancedRiskModels::calculateStandardDeviation(const std::vector<double>& data) {
    if (data.size() < 2) {
        return 0.0;
    }
    
    double mean = calculateMean(data);
    double variance = 0.0;
    
    for (double value : data) {
        variance += (value - mean) * (value - mean);
    }
    
    variance /= (data.size() - 1);
    return std::sqrt(variance);
}

} // namespace risk
} // namespace arbitrage
