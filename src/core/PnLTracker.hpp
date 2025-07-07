#pragma once

#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include "ExecutionPlanner.hpp"
#include "../utils/Logger.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief P&L calculation types
 */
enum class PnLType {
    REALIZED,
    UNREALIZED,
    TOTAL
};

/**
 * @brief Individual trade record
 */
struct TradeRecord {
    std::string trade_id;
    std::string plan_id;
    std::string symbol;
    data::Exchange exchange;
    std::string action; // "BUY" or "SELL"
    
    // Trade details
    double quantity;
    double entry_price;
    double exit_price;
    double realized_pnl;
    
    // Timing
    std::chrono::system_clock::time_point entry_time;
    std::chrono::system_clock::time_point exit_time;
    std::chrono::milliseconds holding_duration{0};
    
    // Costs
    double transaction_costs;
    double slippage;
    double market_impact;
    double total_costs;
    
    // Performance metrics
    double return_percentage;
    double risk_adjusted_return;
    
    // Status
    bool is_closed = false;
    
    TradeRecord() {
        entry_time = std::chrono::system_clock::now();
        trade_id = generateTradeId();
    }
    
private:
    static std::string generateTradeId();
};

/**
 * @brief P&L snapshot at a point in time
 */
struct PnLSnapshot {
    std::chrono::system_clock::time_point timestamp;
    
    // P&L breakdown
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double total_pnl = 0.0;
    
    // Performance metrics
    double total_return_pct = 0.0;
    double annualized_return_pct = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown_pct = 0.0;
    double win_rate_pct = 0.0;
    
    // Trade statistics
    size_t total_trades = 0;
    size_t winning_trades = 0;
    size_t losing_trades = 0;
    double average_win = 0.0;
    double average_loss = 0.0;
    double largest_win = 0.0;
    double largest_loss = 0.0;
    
    // Risk metrics
    double volatility = 0.0;
    double var_95 = 0.0;
    double expected_shortfall = 0.0;
    
    // Capital metrics
    double total_capital_deployed = 0.0;
    double available_capital = 0.0;
    double capital_utilization_pct = 0.0;
};

/**
 * @brief Position tracking for open trades
 */
struct Position {
    std::string symbol;
    data::Exchange exchange;
    double quantity; // Positive for long, negative for short
    double average_entry_price;
    double current_market_price;
    double unrealized_pnl;
    double cost_basis;
    
    std::chrono::system_clock::time_point opened_at;
    std::vector<std::string> trade_ids; // Contributing trades
    
    // Risk metrics
    double position_var;
    double position_risk_contribution;
};

/**
 * @brief Comprehensive P&L tracking and analytics system
 */
class PnLTracker {
public:
    struct TrackingParameters {
        // Capital settings
        double initial_capital; // $100k initial capital
        double risk_free_rate; // 2% annual risk-free rate
        
        // Calculation settings
        bool include_unrealized;
        bool mark_to_market_realtime;
        std::chrono::minutes snapshot_interval; // 5-minute snapshots
        
        // Risk calculation parameters
        double var_confidence_level; // 95% VaR
        size_t var_lookback_days;
        size_t max_drawdown_lookback_days;
        
        // Performance calculation settings
        size_t sharpe_lookback_days;
        bool annualize_returns;

        // Constructor with default values
        TrackingParameters() 
            : initial_capital(100000.0),
              risk_free_rate(0.02),
              include_unrealized(true),
              mark_to_market_realtime(true),
              snapshot_interval(std::chrono::minutes(5)),
              var_confidence_level(0.95),
              var_lookback_days(30),
              max_drawdown_lookback_days(365),
              sharpe_lookback_days(30),
              annualize_returns(true) {
        }
    };
    
    explicit PnLTracker(const TrackingParameters& params = TrackingParameters{});
    
    /**
     * @brief Record a new trade execution
     */
    void recordTrade(const ExecutionOrder& order, double executed_price, double executed_quantity);
    
    /**
     * @brief Update market prices for unrealized P&L calculation
     */
    void updateMarketPrices(const std::vector<data::MarketDataPoint>& market_data);
    
    /**
     * @brief Close a position and realize P&L
     */
    void closePosition(const std::string& symbol, data::Exchange exchange, 
                      double exit_price, double exit_quantity = 0.0);
    
    /**
     * @brief Get current P&L snapshot
     */
    PnLSnapshot getCurrentSnapshot();
    
    /**
     * @brief Get historical P&L snapshots
     */
    std::vector<PnLSnapshot> getHistoricalSnapshots(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time);
    
    /**
     * @brief Get all trade records
     */
    std::vector<TradeRecord> getAllTrades();
    
    /**
     * @brief Get trade records for a specific time period
     */
    std::vector<TradeRecord> getTradeHistory(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time);
    
    /**
     * @brief Get current open positions
     */
    std::vector<Position> getCurrentPositions();
    
    /**
     * @brief Calculate realized P&L
     */
    double calculateRealizedPnL();
    
    /**
     * @brief Calculate unrealized P&L
     */
    double calculateUnrealizedPnL();
    
    /**
     * @brief Calculate total P&L
     */
    double calculateTotalPnL();
    
    /**
     * @brief Performance analytics
     */
    struct PerformanceAnalytics {
        // Return metrics
        double total_return_pct;
        double annualized_return_pct;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;
        
        // Risk metrics
        double volatility;
        double max_drawdown_pct;
        double var_95;
        double expected_shortfall;
        
        // Trade metrics
        double win_rate;
        double average_win_loss_ratio;
        double profit_factor;
        size_t total_trades;
        
        // Efficiency metrics
        double capital_efficiency;
        double risk_adjusted_return;
        double return_per_unit_risk;
    };
    
    PerformanceAnalytics calculatePerformanceAnalytics();
    
    /**
     * @brief Generate P&L report
     */
    struct PnLReport {
        PnLSnapshot current_snapshot;
        PerformanceAnalytics analytics;
        std::vector<TradeRecord> recent_trades;
        std::vector<Position> current_positions;
        std::map<std::string, double> pnl_by_symbol;
        std::map<data::Exchange, double> pnl_by_exchange;
    };
    
    PnLReport generateReport();
    
    /**
     * @brief Update tracking parameters
     */
    void updateTrackingParameters(const TrackingParameters& params);
    
    /**
     * @brief Get current tracking parameters
     */
    const TrackingParameters& getTrackingParameters() const { return params_; }
    
    /**
     * @brief Reset tracking (clear all data)
     */
    void reset();

private:
    TrackingParameters params_;
    mutable std::mutex data_mutex_;
    
    // Core data storage
    std::vector<TradeRecord> trade_history_;
    std::map<std::string, Position> current_positions_; // Key: symbol@exchange
    std::vector<PnLSnapshot> snapshot_history_;
    std::map<std::string, double> current_market_prices_; // Key: symbol@exchange
    
    // Performance tracking
    std::atomic<double> running_pnl_{0.0};
    std::atomic<double> peak_pnl_{0.0};
    std::atomic<double> max_drawdown_{0.0};
    
    // Timing
    std::chrono::system_clock::time_point last_snapshot_time_;
    std::chrono::system_clock::time_point start_time_;
    
    // Helper methods
    std::string createPositionKey(const std::string& symbol, data::Exchange exchange);
    void updatePosition(const ExecutionOrder& order, double executed_price, double executed_quantity);
    void updateUnrealizedPnL();
    void takeSnapshot();
    bool shouldTakeSnapshot();
    
    // Calculation helpers
    double calculateSharpeRatio(const std::vector<double>& returns);
    double calculateMaxDrawdown(const std::vector<double>& equity_curve);
    double calculateVaR(const std::vector<double>& returns, double confidence_level);
    double calculateExpectedShortfall(const std::vector<double>& returns, double confidence_level);
    std::vector<double> calculateDailyReturns();
    std::vector<double> getEquityCurve();
    
    // Utility functions
    std::string generateTradeId();

public:
    // Exposed for simulation purposes
    double getMarketPrice(const std::string& symbol, data::Exchange exchange);
};

} // namespace core
} // namespace arbitrage
