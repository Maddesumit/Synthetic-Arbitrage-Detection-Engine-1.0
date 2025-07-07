#include "PnLTracker.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

namespace arbitrage {
namespace core {

std::string TradeRecord::generateTradeId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "TRD_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << "_" << dis(gen);
    return ss.str();
}

PnLTracker::PnLTracker(const TrackingParameters& params) 
    : params_(params), start_time_(std::chrono::system_clock::now()) {
    last_snapshot_time_ = start_time_;
    utils::Logger::info("PnLTracker initialized with initial capital: $" + 
                       std::to_string(params_.initial_capital));
}

void PnLTracker::recordTrade(const ExecutionOrder& order, double executed_price, double executed_quantity) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    TradeRecord trade;
    trade.plan_id = ""; // Would be set from execution context
    trade.symbol = order.symbol;
    trade.exchange = order.exchange;
    trade.action = order.action;
    trade.quantity = executed_quantity;
    trade.entry_price = executed_price;
    trade.entry_time = std::chrono::system_clock::now();
    
    // Calculate transaction costs (simplified)
    trade.transaction_costs = executed_price * executed_quantity * 0.001; // 0.1% fee
    trade.slippage = std::abs(executed_price - order.target_price) * executed_quantity;
    trade.market_impact = 0.0; // Would be calculated based on order size vs market depth
    trade.total_costs = trade.transaction_costs + trade.slippage + trade.market_impact;
    
    trade_history_.push_back(trade);
    
    // Update position
    updatePosition(order, executed_price, executed_quantity);
    
    // Update unrealized P&L
    updateUnrealizedPnL();
    
    // Take snapshot if needed
    if (shouldTakeSnapshot()) {
        takeSnapshot();
    }
    
    utils::Logger::info("Recorded trade: " + trade.trade_id + " " + trade.action + " " + 
                       std::to_string(executed_quantity) + " " + trade.symbol + " at $" + 
                       std::to_string(executed_price));
}

void PnLTracker::updateMarketPrices(const std::vector<data::MarketDataPoint>& market_data) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    for (const auto& data : market_data) {
        std::string key = createPositionKey(data.symbol, data::stringToExchange(data.exchange));
        current_market_prices_[key] = data.last;
    }
    
    // Update unrealized P&L with new prices
    updateUnrealizedPnL();
    
    // Take snapshot if needed
    if (shouldTakeSnapshot()) {
        takeSnapshot();
    }
}

void PnLTracker::closePosition(const std::string& symbol, data::Exchange exchange, 
                              double exit_price, double exit_quantity) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::string key = createPositionKey(symbol, exchange);
    auto pos_it = current_positions_.find(key);
    
    if (pos_it == current_positions_.end()) {
        utils::Logger::warn("Attempted to close non-existent position: " + key);
        return;
    }
    
    Position& position = pos_it->second;
    double close_quantity = (exit_quantity == 0.0) ? std::abs(position.quantity) : exit_quantity;
    
    // Calculate realized P&L
    double pnl = 0.0;
    if (position.quantity > 0) { // Long position
        pnl = (exit_price - position.average_entry_price) * close_quantity;
    } else { // Short position
        pnl = (position.average_entry_price - exit_price) * close_quantity;
    }
    
    // Update trade records
    for (const auto& trade_id : position.trade_ids) {
        for (auto& trade : trade_history_) {
            if (trade.trade_id == trade_id && !trade.is_closed) {
                trade.exit_price = exit_price;
                trade.exit_time = std::chrono::system_clock::now();
                trade.holding_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    trade.exit_time - trade.entry_time);
                trade.realized_pnl = pnl * (trade.quantity / close_quantity);
                trade.return_percentage = trade.realized_pnl / (trade.entry_price * trade.quantity) * 100.0;
                trade.is_closed = true;
                break;
            }
        }
    }
    
    // Update position
    if (close_quantity >= std::abs(position.quantity)) {
        // Close entire position
        current_positions_.erase(pos_it);
    } else {
        // Partial close
        if (position.quantity > 0) {
            position.quantity -= close_quantity;
        } else {
            position.quantity += close_quantity;
        }
        position.cost_basis = position.average_entry_price * std::abs(position.quantity);
    }
    
    // Update running P&L
    running_pnl_.fetch_add(pnl);
    
    utils::Logger::info("Closed position: " + symbol + " P&L: $" + std::to_string(pnl));
}

PnLSnapshot PnLTracker::getCurrentSnapshot() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    PnLSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    
    // Calculate P&L components
    snapshot.realized_pnl = calculateRealizedPnL();
    snapshot.unrealized_pnl = calculateUnrealizedPnL();
    snapshot.total_pnl = snapshot.realized_pnl + snapshot.unrealized_pnl;
    
    // Calculate performance metrics
    auto analytics = calculatePerformanceAnalytics();
    snapshot.total_return_pct = analytics.total_return_pct;
    snapshot.annualized_return_pct = analytics.annualized_return_pct;
    snapshot.sharpe_ratio = analytics.sharpe_ratio;
    snapshot.max_drawdown_pct = analytics.max_drawdown_pct;
    snapshot.win_rate_pct = analytics.win_rate;
    snapshot.volatility = analytics.volatility;
    snapshot.var_95 = analytics.var_95;
    snapshot.expected_shortfall = analytics.expected_shortfall;
    
    // Calculate trade statistics
    size_t winning_trades = 0;
    size_t losing_trades = 0;
    double total_wins = 0.0;
    double total_losses = 0.0;
    double largest_win = 0.0;
    double largest_loss = 0.0;
    
    for (const auto& trade : trade_history_) {
        if (trade.is_closed) {
            if (trade.realized_pnl > 0) {
                winning_trades++;
                total_wins += trade.realized_pnl;
                largest_win = std::max(largest_win, trade.realized_pnl);
            } else {
                losing_trades++;
                total_losses += std::abs(trade.realized_pnl);
                largest_loss = std::max(largest_loss, std::abs(trade.realized_pnl));
            }
        }
    }
    
    snapshot.total_trades = winning_trades + losing_trades;
    snapshot.winning_trades = winning_trades;
    snapshot.losing_trades = losing_trades;
    snapshot.average_win = (winning_trades > 0) ? total_wins / winning_trades : 0.0;
    snapshot.average_loss = (losing_trades > 0) ? total_losses / losing_trades : 0.0;
    snapshot.largest_win = largest_win;
    snapshot.largest_loss = largest_loss;
    
    // Calculate capital metrics
    snapshot.total_capital_deployed = 0.0;
    for (const auto& [key, position] : current_positions_) {
        snapshot.total_capital_deployed += std::abs(position.cost_basis);
    }
    
    snapshot.available_capital = params_.initial_capital + snapshot.realized_pnl - snapshot.total_capital_deployed;
    snapshot.capital_utilization_pct = (snapshot.total_capital_deployed / params_.initial_capital) * 100.0;
    
    return snapshot;
}

std::vector<PnLSnapshot> PnLTracker::getHistoricalSnapshots(
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time) {
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<PnLSnapshot> filtered_snapshots;
    
    for (const auto& snapshot : snapshot_history_) {
        if (snapshot.timestamp >= start_time && snapshot.timestamp <= end_time) {
            filtered_snapshots.push_back(snapshot);
        }
    }
    
    return filtered_snapshots;
}

std::vector<TradeRecord> PnLTracker::getAllTrades() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return trade_history_;
}

std::vector<TradeRecord> PnLTracker::getTradeHistory(
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time) {
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<TradeRecord> filtered_trades;
    
    for (const auto& trade : trade_history_) {
        if (trade.entry_time >= start_time && trade.entry_time <= end_time) {
            filtered_trades.push_back(trade);
        }
    }
    
    return filtered_trades;
}

std::vector<Position> PnLTracker::getCurrentPositions() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<Position> positions;
    for (const auto& [key, position] : current_positions_) {
        positions.push_back(position);
    }
    
    return positions;
}

double PnLTracker::calculateRealizedPnL() {
    double realized = 0.0;
    
    for (const auto& trade : trade_history_) {
        if (trade.is_closed) {
            realized += trade.realized_pnl;
        }
    }
    
    return realized;
}

double PnLTracker::calculateUnrealizedPnL() {
    double unrealized = 0.0;
    
    for (const auto& [key, position] : current_positions_) {
        double market_price = getMarketPrice(position.symbol, position.exchange);
        if (market_price > 0.0) {
            if (position.quantity > 0) { // Long position
                unrealized += (market_price - position.average_entry_price) * position.quantity;
            } else { // Short position
                unrealized += (position.average_entry_price - market_price) * std::abs(position.quantity);
            }
        }
    }
    
    return unrealized;
}

double PnLTracker::calculateTotalPnL() {
    return calculateRealizedPnL() + calculateUnrealizedPnL();
}

PnLTracker::PerformanceAnalytics PnLTracker::calculatePerformanceAnalytics() {
    PerformanceAnalytics analytics{};
    
    if (trade_history_.empty()) {
        return analytics;
    }
    
    // Calculate basic metrics
    double total_pnl = calculateTotalPnL();
    analytics.total_return_pct = (total_pnl / params_.initial_capital) * 100.0;
    
    // Calculate time-based metrics
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::hours>(now - start_time_);
    double years = duration.count() / (365.25 * 24.0);
    
    if (years > 0) {
        analytics.annualized_return_pct = std::pow(1.0 + analytics.total_return_pct / 100.0, 1.0 / years) - 1.0;
        analytics.annualized_return_pct *= 100.0;
    }
    
    // Calculate daily returns for advanced metrics
    auto daily_returns = calculateDailyReturns();
    if (!daily_returns.empty()) {
        analytics.sharpe_ratio = calculateSharpeRatio(daily_returns);
        analytics.volatility = 0.0;
        if (!daily_returns.empty()) {
            double mean_return = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) / daily_returns.size();
            double variance = 0.0;
            for (double ret : daily_returns) {
                variance += (ret - mean_return) * (ret - mean_return);
            }
            variance /= daily_returns.size();
            analytics.volatility = std::sqrt(variance) * std::sqrt(252.0); // Annualized
        }
        
        analytics.var_95 = calculateVaR(daily_returns, 0.95);
        analytics.expected_shortfall = calculateExpectedShortfall(daily_returns, 0.95);
    }
    
    // Calculate max drawdown
    auto equity_curve = getEquityCurve();
    analytics.max_drawdown_pct = calculateMaxDrawdown(equity_curve);
    
    // Calculate trade-based metrics
    size_t winning_trades = 0;
    size_t total_closed_trades = 0;
    double total_wins = 0.0;
    double total_losses = 0.0;
    
    for (const auto& trade : trade_history_) {
        if (trade.is_closed) {
            total_closed_trades++;
            if (trade.realized_pnl > 0) {
                winning_trades++;
                total_wins += trade.realized_pnl;
            } else {
                total_losses += std::abs(trade.realized_pnl);
            }
        }
    }
    
    analytics.total_trades = total_closed_trades;
    analytics.win_rate = (total_closed_trades > 0) ? 
        (static_cast<double>(winning_trades) / total_closed_trades) * 100.0 : 0.0;
    
    analytics.average_win_loss_ratio = (total_losses > 0) ? total_wins / total_losses : 0.0;
    analytics.profit_factor = (total_losses > 0) ? total_wins / total_losses : 
        (total_wins > 0 ? 999.0 : 0.0);
    
    // Calculate efficiency metrics
    analytics.capital_efficiency = (params_.initial_capital > 0) ? 
        total_pnl / params_.initial_capital : 0.0;
    analytics.risk_adjusted_return = (analytics.volatility > 0) ? 
        analytics.total_return_pct / (analytics.volatility * 100.0) : 0.0;
    analytics.return_per_unit_risk = analytics.risk_adjusted_return;
    
    return analytics;
}

PnLTracker::PnLReport PnLTracker::generateReport() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    PnLReport report;
    report.current_snapshot = getCurrentSnapshot();
    report.analytics = calculatePerformanceAnalytics();
    report.current_positions = getCurrentPositions();
    
    // Get recent trades (last 24 hours)
    auto yesterday = std::chrono::system_clock::now() - std::chrono::hours(24);
    report.recent_trades = getTradeHistory(yesterday, std::chrono::system_clock::now());
    
    // Calculate P&L by symbol and exchange
    for (const auto& trade : trade_history_) {
        if (trade.is_closed) {
            report.pnl_by_symbol[trade.symbol] += trade.realized_pnl;
            report.pnl_by_exchange[trade.exchange] += trade.realized_pnl;
        }
    }
    
    return report;
}

// Private helper method implementations

std::string PnLTracker::createPositionKey(const std::string& symbol, data::Exchange exchange) {
    return symbol + "@" + data::exchangeToString(exchange);
}

void PnLTracker::updatePosition(const ExecutionOrder& order, double executed_price, double executed_quantity) {
    std::string key = createPositionKey(order.symbol, order.exchange);
    
    auto& position = current_positions_[key];
    position.symbol = order.symbol;
    position.exchange = order.exchange;
    
    if (order.action == "BUY") {
        if (position.quantity >= 0) { // Adding to long position
            double total_cost = position.cost_basis + (executed_price * executed_quantity);
            double total_quantity = position.quantity + executed_quantity;
            position.average_entry_price = total_cost / total_quantity;
            position.quantity = total_quantity;
            position.cost_basis = total_cost;
        } else { // Covering short position
            position.quantity += executed_quantity;
            if (position.quantity > 0) {
                position.average_entry_price = executed_price;
                position.cost_basis = executed_price * position.quantity;
            }
        }
    } else { // SELL
        if (position.quantity <= 0) { // Adding to short position
            double total_quantity = std::abs(position.quantity) + executed_quantity;
            position.quantity = -total_quantity;
            position.average_entry_price = executed_price;
            position.cost_basis = executed_price * total_quantity;
        } else { // Closing long position
            position.quantity -= executed_quantity;
            if (position.quantity < 0) {
                position.average_entry_price = executed_price;
                position.cost_basis = executed_price * std::abs(position.quantity);
            }
        }
    }
    
    if (position.opened_at.time_since_epoch().count() == 0) {
        position.opened_at = std::chrono::system_clock::now();
    }
    
    // Add trade ID to position
    for (const auto& trade : trade_history_) {
        if (trade.symbol == order.symbol && !trade.is_closed) {
            position.trade_ids.push_back(trade.trade_id);
            break;
        }
    }
}

void PnLTracker::updateUnrealizedPnL() {
    for (auto& [key, position] : current_positions_) {
        double market_price = getMarketPrice(position.symbol, position.exchange);
        if (market_price > 0.0) {
            position.current_market_price = market_price;
            if (position.quantity > 0) { // Long position
                position.unrealized_pnl = (market_price - position.average_entry_price) * position.quantity;
            } else { // Short position
                position.unrealized_pnl = (position.average_entry_price - market_price) * std::abs(position.quantity);
            }
        }
    }
}

void PnLTracker::takeSnapshot() {
    auto snapshot = getCurrentSnapshot();
    snapshot_history_.push_back(snapshot);
    last_snapshot_time_ = snapshot.timestamp;
    
    // Keep only recent snapshots (e.g., last 30 days)
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    snapshot_history_.erase(
        std::remove_if(snapshot_history_.begin(), snapshot_history_.end(),
                      [cutoff_time](const PnLSnapshot& s) { return s.timestamp < cutoff_time; }),
        snapshot_history_.end());
}

bool PnLTracker::shouldTakeSnapshot() {
    auto now = std::chrono::system_clock::now();
    return (now - last_snapshot_time_) >= params_.snapshot_interval;
}

double PnLTracker::getMarketPrice(const std::string& symbol, data::Exchange exchange) {
    std::string key = createPositionKey(symbol, exchange);
    auto it = current_market_prices_.find(key);
    return (it != current_market_prices_.end()) ? it->second : 0.0;
}

std::vector<double> PnLTracker::calculateDailyReturns() {
    std::vector<double> returns;
    
    if (snapshot_history_.size() < 2) {
        return returns;
    }
    
    for (size_t i = 1; i < snapshot_history_.size(); ++i) {
        double prev_equity = params_.initial_capital + snapshot_history_[i-1].total_pnl;
        double curr_equity = params_.initial_capital + snapshot_history_[i].total_pnl;
        
        if (prev_equity > 0) {
            double daily_return = (curr_equity - prev_equity) / prev_equity;
            returns.push_back(daily_return);
        }
    }
    
    return returns;
}

std::vector<double> PnLTracker::getEquityCurve() {
    std::vector<double> equity_curve;
    
    for (const auto& snapshot : snapshot_history_) {
        equity_curve.push_back(params_.initial_capital + snapshot.total_pnl);
    }
    
    return equity_curve;
}

double PnLTracker::calculateSharpeRatio(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return - (params_.risk_free_rate / 252.0); // Daily risk-free rate
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    double std_dev = std::sqrt(variance);
    
    return (std_dev > 0) ? (excess_return / std_dev) * std::sqrt(252.0) : 0.0; // Annualized
}

double PnLTracker::calculateMaxDrawdown(const std::vector<double>& equity_curve) {
    if (equity_curve.empty()) return 0.0;
    
    double max_drawdown = 0.0;
    double peak = equity_curve[0];
    
    for (double equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
        }
        
        double drawdown = (peak - equity) / peak;
        max_drawdown = std::max(max_drawdown, drawdown);
    }
    
    return max_drawdown * 100.0; // Return as percentage
}

double PnLTracker::calculateVaR(const std::vector<double>& returns, double confidence_level) {
    if (returns.empty()) return 0.0;
    
    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    
    size_t index = static_cast<size_t>((1.0 - confidence_level) * sorted_returns.size());
    index = std::min(index, sorted_returns.size() - 1);
    
    return -sorted_returns[index] * 100.0; // Return as positive percentage
}

double PnLTracker::calculateExpectedShortfall(const std::vector<double>& returns, double confidence_level) {
    if (returns.empty()) return 0.0;
    
    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    
    size_t cutoff_index = static_cast<size_t>((1.0 - confidence_level) * sorted_returns.size());
    cutoff_index = std::min(cutoff_index, sorted_returns.size() - 1);
    
    double sum = 0.0;
    size_t count = 0;
    
    for (size_t i = 0; i <= cutoff_index; ++i) {
        sum += sorted_returns[i];
        count++;
    }
    
    return (count > 0) ? -(sum / count) * 100.0 : 0.0; // Return as positive percentage
}

void PnLTracker::updateTrackingParameters(const TrackingParameters& params) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    params_ = params;
    utils::Logger::info("Updated P&L tracking parameters");
}

void PnLTracker::reset() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    trade_history_.clear();
    current_positions_.clear();
    snapshot_history_.clear();
    current_market_prices_.clear();
    
    running_pnl_ = 0.0;
    peak_pnl_ = 0.0;
    max_drawdown_ = 0.0;
    
    start_time_ = std::chrono::system_clock::now();
    last_snapshot_time_ = start_time_;
    
    utils::Logger::info("P&L tracker reset");
}

} // namespace core
} // namespace arbitrage
