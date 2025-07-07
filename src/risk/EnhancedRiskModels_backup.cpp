#include "EnhancedRiskModels.hpp"
#include "../core/PnLTracker.hpp"
#include "../core/RiskManager.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

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
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 30) {
        return position_value * 0.02; // Default 2% VaR
    }
    
    // Calculate returns
    std::vector<double> returns;
    const auto& prices = it->second;
    for (size_t i = 1; i < prices.size(); ++i) {
        double ret = (prices[i] - prices[i-1]) / prices[i-1];
        returns.push_back(ret);
    }
    
    // Sort returns for percentile calculation
    std::sort(returns.begin(), returns.end());
    
    // Calculate VaR at confidence level
    size_t var_index = static_cast<size_t>((1.0 - confidence_level_) * returns.size());
    double var_return = returns[var_index];
    
    return std::abs(var_return * position_value);
}

double EnhancedRiskModels::calculateExpectedShortfall(const std::string& symbol, double position_value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 30) {
        return position_value * 0.025; // Default 2.5% ES
    }
    
    // Calculate returns
    std::vector<double> returns;
    const auto& prices = it->second;
    for (size_t i = 1; i < prices.size(); ++i) {
        double ret = (prices[i] - prices[i-1]) / prices[i-1];
        returns.push_back(ret);
    }
    
    // Sort returns for percentile calculation
    std::sort(returns.begin(), returns.end());
    
    // Calculate Expected Shortfall (average of worst returns beyond VaR)
    size_t var_index = static_cast<size_t>((1.0 - confidence_level_) * returns.size());
    
    double sum_worst_returns = 0.0;
    for (size_t i = 0; i < var_index; ++i) {
        sum_worst_returns += returns[i];
    }
    
    double es_return = sum_worst_returns / var_index;
    return std::abs(es_return * position_value);
}

RiskMetrics EnhancedRiskModels::calculatePortfolioRisk(const std::vector<Position>& positions) {
    RiskMetrics metrics;
    
    double total_var = 0.0;
    double total_es = 0.0;
    double total_exposure = 0.0;
    
    for (const auto& position : positions) {
        double position_value = position.quantity * position.current_price;
        total_exposure += std::abs(position_value);
        
        double var = calculateVaR(position.symbol, position_value);
        double es = calculateExpectedShortfall(position.symbol, position_value);
        
        total_var += var * var; // Sum of squares for diversification
        total_es += es;
    }
    
    metrics.value_at_risk = std::sqrt(total_var); // Apply diversification
    metrics.expected_shortfall = total_es;
    metrics.maximum_drawdown = calculateMaxDrawdown(positions);
    metrics.sharpe_ratio = calculateSharpeRatio(positions);
    metrics.beta = calculateBeta(positions);
    metrics.total_exposure = total_exposure;
    
    return metrics;
}

double EnhancedRiskModels::calculateMaxDrawdown(const std::vector<Position>& positions) {
    // Simplified calculation - in practice would use historical portfolio values
    double max_drawdown = 0.0;
    
    for (const auto& position : positions) {
        auto it = historical_prices_.find(position.symbol);
        if (it != historical_prices_.end() && !it->second.empty()) {
            const auto& prices = it->second;
            double peak = prices[0];
            double max_dd = 0.0;
            
            for (double price : prices) {
                if (price > peak) peak = price;
                double drawdown = (peak - price) / peak;
                max_dd = std::max(max_dd, drawdown);
            }
            
            max_drawdown = std::max(max_drawdown, max_dd);
        }
    }
    
    return max_drawdown;
}

double EnhancedRiskModels::calculateSharpeRatio(const std::vector<Position>& positions) {
    // Simplified Sharpe ratio calculation
    // In practice, would calculate portfolio returns and use risk-free rate
    
    std::vector<double> portfolio_returns;
    for (const auto& position : positions) {
        auto it = historical_prices_.find(position.symbol);
        if (it != historical_prices_.end() && it->second.size() > 1) {
            const auto& prices = it->second;
            for (size_t i = 1; i < prices.size(); ++i) {
                double ret = (prices[i] - prices[i-1]) / prices[i-1];
                portfolio_returns.push_back(ret * position.quantity / positions.size());
            }
        }
    }
    
    if (portfolio_returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(portfolio_returns.begin(), portfolio_returns.end(), 0.0) / portfolio_returns.size();
    
    double variance = 0.0;
    for (double ret : portfolio_returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= portfolio_returns.size();
    double std_dev = std::sqrt(variance);
    
    const double risk_free_rate = 0.02 / 252; // 2% annual risk-free rate, daily
    return std_dev > 0 ? (mean_return - risk_free_rate) / std_dev : 0.0;
}

double EnhancedRiskModels::calculateBeta(const std::vector<Position>& positions) {
    // Simplified beta calculation against a market proxy
    // In practice, would use actual market index data
    return 1.0; // Placeholder
}

void EnhancedRiskModels::updateRealTimeVolatility(const std::string& symbol) {
    auto it = historical_prices_.find(symbol);
    if (it == historical_prices_.end() || it->second.size() < 20) {
        return;
    }
    
    const auto& prices = it->second;
    std::vector<double> returns;
    
    // Calculate recent returns
    size_t start_idx = prices.size() > 20 ? prices.size() - 20 : 0;
    for (size_t i = start_idx + 1; i < prices.size(); ++i) {
        double ret = (prices[i] - prices[i-1]) / prices[i-1];
        returns.push_back(ret);
    }
    
    // Calculate volatility
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    real_time_volatility_[symbol] = std::sqrt(variance * 252); // Annualized volatility
}

std::vector<double> EnhancedRiskModels::runMonteCarloSimulation(
    const std::string& symbol, 
    double initial_price, 
    int days_ahead
) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<double> simulated_prices;
    
    // Get volatility for the symbol
    double volatility = 0.2; // Default 20% volatility
    auto vol_it = real_time_volatility_.find(symbol);
    if (vol_it != real_time_volatility_.end()) {
        volatility = vol_it->second;
    }
    
    // Random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> normal_dist(0.0, 1.0);
    
    double dt = 1.0 / 252.0; // Daily time step
    double drift = 0.05; // 5% annual drift
    
    for (int sim = 0; sim < monte_carlo_simulations_; ++sim) {
        double price = initial_price;
        
        for (int day = 0; day < days_ahead; ++day) {
            double random_shock = normal_dist(gen);
            double price_change = price * (drift * dt + volatility * std::sqrt(dt) * random_shock);
            price += price_change;
        }
        
        simulated_prices.push_back(price);
    }
    
    std::sort(simulated_prices.begin(), simulated_prices.end());
    return simulated_prices;
}

void EnhancedRiskModels::setConfidenceLevel(double level) {
    const std::vector<Position>& positions,
    const StressScenario& scenario
) {
    StressTestResults results;
    results.scenario_name = scenario.name;
    
    double total_pnl = 0.0;
    
    for (const auto& position : positions) {
        double current_value = position.quantity * position.current_price;
        
        // Apply stress scenario
        double stressed_price = position.current_price;
        auto shock_it = scenario.price_shocks.find(position.symbol);
        if (shock_it != scenario.price_shocks.end()) {
            stressed_price *= (1.0 + shock_it->second);
        } else {
            // Apply market-wide shock if no specific shock for this symbol
            stressed_price *= (1.0 + scenario.market_shock);
        }
        
        double stressed_value = position.quantity * stressed_price;
        double pnl = stressed_value - current_value;
        
        total_pnl += pnl;
        
        PositionStressResult pos_result;
        pos_result.symbol = position.symbol;
        pos_result.original_value = current_value;
        pos_result.stressed_value = stressed_value;
        pos_result.pnl = pnl;
        pos_result.percentage_change = (stressed_price / position.current_price - 1.0) * 100.0;
        
        results.position_results.push_back(pos_result);
    }
    
    results.total_portfolio_pnl = total_pnl;
    results.total_portfolio_change_percent = 
        (total_pnl / calculateTotalPortfolioValue(positions)) * 100.0;
    
    return results;
}

double EnhancedRiskModels::calculateTotalPortfolioValue(const std::vector<Position>& positions) {
    double total = 0.0;
    for (const auto& position : positions) {
        total += std::abs(position.quantity * position.current_price);
    }
    return total;
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

} // namespace risk
} // namespace arbitrage
