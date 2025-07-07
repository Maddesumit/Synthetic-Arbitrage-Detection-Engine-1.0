#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include "MarketData.hpp"
#include "../core/PricingEngine.hpp"

namespace arbitrage {
namespace data {

// Type aliases for convenience
using ArbitrageOpportunity = core::ArbitrageOpportunity;
using MarketDataPoint = arbitrage::data::MarketDataPoint;

namespace persistence {

// Time-series data point for analytics
struct TimeSeriesPoint {
    std::chrono::system_clock::time_point timestamp;
    std::string metric_name;
    double value;
    std::unordered_map<std::string, std::string> tags;
    nlohmann::json metadata;
};

// Historical data query parameters
struct QueryParameters {
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::vector<std::string> metrics;
    std::vector<std::string> exchanges;
    std::vector<std::string> symbols;
    std::string aggregation_function = "raw"; // raw, avg, min, max, sum
    std::chrono::seconds aggregation_interval = std::chrono::seconds(0);
    size_t limit = 0; // 0 means no limit
};

// Analytics result structure
struct AnalyticsResult {
    std::string query_id;
    std::vector<TimeSeriesPoint> data_points;
    nlohmann::json metadata;
    std::chrono::system_clock::time_point generated_at;
    std::chrono::milliseconds execution_time;
};

class HistoricalDataStorage {
public:
    HistoricalDataStorage(const std::string& database_path);
    ~HistoricalDataStorage();
    
    // Database initialization
    bool initialize();
    bool createTables();
    
    // Data insertion
    bool insertMarketData(const MarketDataPoint& data_point);
    bool insertMarketDataBatch(const std::vector<MarketDataPoint>& data_points);
    
    bool insertArbitrageOpportunity(const ArbitrageOpportunity& opportunity);
    bool insertTimeSeriesPoint(const TimeSeriesPoint& point);
    bool insertTimeSeriesPointBatch(const std::vector<TimeSeriesPoint>& points);
    
    // Data retrieval
    std::vector<MarketDataPoint> getMarketData(const QueryParameters& params);
    std::vector<ArbitrageOpportunity> getArbitrageOpportunities(const QueryParameters& params);
    std::vector<TimeSeriesPoint> getTimeSeriesData(const QueryParameters& params);
    
    // Analytics queries
    AnalyticsResult performAnalyticsQuery(const std::string& query_sql,
                                        const std::unordered_map<std::string, std::string>& parameters = {});
    
    // Aggregation functions
    std::vector<TimeSeriesPoint> aggregateData(const std::string& metric_name,
                                             std::chrono::system_clock::time_point start_time,
                                             std::chrono::system_clock::time_point end_time,
                                             std::chrono::seconds interval,
                                             const std::string& aggregation_function = "avg");
    
    // Data management
    bool cleanupOldData(std::chrono::hours retention_period);
    bool compactDatabase();
    size_t getDatabaseSize() const;
    
    // Performance monitoring
    struct DatabaseStats {
        size_t total_records;
        size_t market_data_records;
        size_t arbitrage_opportunity_records;
        size_t time_series_records;
        std::chrono::system_clock::time_point oldest_record;
        std::chrono::system_clock::time_point newest_record;
        size_t database_size_bytes;
    };
    
    DatabaseStats getStatistics() const;
    
    // Async operations
    void enableAsyncWrites(bool enable = true);
    void flushPendingWrites();
    
private:
    sqlite3* database_;
    std::string database_path_;
    
    // Prepared statements for performance
    sqlite3_stmt* insert_market_data_stmt_;
    sqlite3_stmt* insert_arbitrage_opportunity_stmt_;
    sqlite3_stmt* insert_time_series_stmt_;
    
    // Async write support
    std::atomic<bool> async_writes_enabled_;
    std::atomic<bool> writer_running_;
    std::thread writer_thread_;
    std::queue<std::function<void()>> write_queue_;
    std::mutex write_queue_mutex_;
    std::condition_variable write_queue_cv_;
    
    // Helper methods
    bool executeSQL(const std::string& sql);
    bool prepareStatements();
    void asyncWriteWorker();
    std::string buildWhereClause(const QueryParameters& params) const;
    
    // Schema management
    static constexpr const char* CREATE_MARKET_DATA_TABLE = R"(
        CREATE TABLE IF NOT EXISTS market_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            exchange TEXT NOT NULL,
            symbol TEXT NOT NULL,
            bid REAL NOT NULL,
            ask REAL NOT NULL,
            last REAL NOT NULL,
            volume REAL NOT NULL,
            funding_rate REAL,
            metadata TEXT,
            INDEX(timestamp),
            INDEX(exchange, symbol, timestamp)
        )
    )";
    
    static constexpr const char* CREATE_ARBITRAGE_OPPORTUNITIES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS arbitrage_opportunities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            underlying_symbol TEXT NOT NULL,
            expected_profit_pct REAL NOT NULL,
            required_capital REAL NOT NULL,
            risk_score REAL NOT NULL,
            confidence REAL NOT NULL,
            legs_json TEXT NOT NULL,
            metadata TEXT,
            INDEX(timestamp),
            INDEX(underlying_symbol, timestamp)
        )
    )";
    
    static constexpr const char* CREATE_TIME_SERIES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS time_series (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            metric_name TEXT NOT NULL,
            value REAL NOT NULL,
            tags TEXT,
            metadata TEXT,
            INDEX(timestamp),
            INDEX(metric_name, timestamp)
        )
    )";
};

class PerformanceAnalytics {
public:
    PerformanceAnalytics(std::shared_ptr<HistoricalDataStorage> storage);
    
    // Performance metrics calculation
    struct PerformanceMetrics {
        double total_return;
        double annualized_return;
        double volatility;
        double sharpe_ratio;
        double max_drawdown;
        double win_rate;
        double profit_factor;
        double calmar_ratio;
        size_t total_trades;
        std::chrono::system_clock::time_point period_start;
        std::chrono::system_clock::time_point period_end;
    };
    
    PerformanceMetrics calculatePerformanceMetrics(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    // Risk analytics
    struct RiskAnalytics {
        double value_at_risk_95;
        double expected_shortfall;
        double beta;
        double alpha;
        double information_ratio;
        double tracking_error;
        std::vector<double> correlation_matrix;
        std::vector<std::string> asset_names;
    };
    
    RiskAnalytics calculateRiskAnalytics(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    // Attribution analysis
    struct AttributionAnalysis {
        std::unordered_map<std::string, double> strategy_attribution;
        std::unordered_map<std::string, double> exchange_attribution;
        std::unordered_map<std::string, double> asset_attribution;
        double total_attributed_return;
        double unexplained_return;
    };
    
    AttributionAnalysis performAttributionAnalysis(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    // Trend analysis
    struct TrendAnalysis {
        double linear_trend_slope;
        double r_squared;
        double trend_strength;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> trend_line;
        bool is_trending_up;
        double trend_confidence;
    };
    
    TrendAnalysis analyzeTrend(const std::string& metric_name,
                             std::chrono::system_clock::time_point start_time,
                             std::chrono::system_clock::time_point end_time) const;
    
    // Correlation analysis
    std::vector<std::vector<double>> calculateCorrelationMatrix(
        const std::vector<std::string>& metrics,
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    // Statistical testing
    struct StatisticalTest {
        std::string test_name;
        double test_statistic;
        double p_value;
        bool is_significant;
        double confidence_level;
        std::string interpretation;
    };
    
    StatisticalTest performStationarityTest(const std::string& metric_name,
                                          std::chrono::system_clock::time_point start_time,
                                          std::chrono::system_clock::time_point end_time) const;
    
    StatisticalTest performNormalityTest(const std::string& metric_name,
                                       std::chrono::system_clock::time_point start_time,
                                       std::chrono::system_clock::time_point end_time) const;
    
    // Report generation
    nlohmann::json generatePerformanceReport(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    nlohmann::json generateRiskReport(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;
    
    // Real-time analytics
    void updateRealTimeMetrics(const std::string& metric_name, double value);
    double getRealTimeMetric(const std::string& metric_name) const;
    
private:
    std::shared_ptr<HistoricalDataStorage> storage_;
    mutable std::unordered_map<std::string, double> real_time_metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Helper methods for calculations
    std::vector<double> getMetricValues(const std::string& metric_name,
                                      std::chrono::system_clock::time_point start_time,
                                      std::chrono::system_clock::time_point end_time) const;
    
    double calculateSharpeRatio(const std::vector<double>& returns, double risk_free_rate = 0.02) const;
    double calculateMaxDrawdown(const std::vector<double>& cumulative_returns) const;
    double calculateVolatility(const std::vector<double>& returns) const;
    double calculateBeta(const std::vector<double>& asset_returns,
                        const std::vector<double>& market_returns) const;
    
    // Statistical calculations
    double calculateMean(const std::vector<double>& values) const;
    double calculateStandardDeviation(const std::vector<double>& values) const;
    double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y) const;
    
    // Time series analysis
    std::pair<double, double> fitLinearTrend(const std::vector<double>& values) const;
    double calculateRSquared(const std::vector<double>& actual,
                           const std::vector<double>& predicted) const;
};

class TradeAuditTrail {
public:
    struct TradeRecord {
        std::string trade_id;
        std::chrono::system_clock::time_point timestamp;
        std::string strategy_name;
        std::string action; // "open", "close", "modify"
        std::string symbol;
        std::string exchange;
        double quantity;
        double price;
        double commission;
        double realized_pnl;
        std::string order_id;
        std::string parent_trade_id; // For multi-leg trades
        nlohmann::json metadata;
    };
    
    struct AuditQuery {
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        std::vector<std::string> trade_ids;
        std::vector<std::string> strategies;
        std::vector<std::string> symbols;
        std::vector<std::string> exchanges;
        std::string action_filter;
    };
    
    TradeAuditTrail(std::shared_ptr<HistoricalDataStorage> storage);
    
    // Trade recording
    bool recordTrade(const TradeRecord& trade);
    bool recordTradeBatch(const std::vector<TradeRecord>& trades);
    
    // Audit queries
    std::vector<TradeRecord> queryTrades(const AuditQuery& query) const;
    std::vector<TradeRecord> getTradesByStrategy(const std::string& strategy_name,
                                               std::chrono::system_clock::time_point start_time,
                                               std::chrono::system_clock::time_point end_time) const;
    
    // Compliance reporting
    nlohmann::json generateComplianceReport(std::chrono::system_clock::time_point start_time,
                                           std::chrono::system_clock::time_point end_time) const;
    
    // Trade reconstruction
    std::vector<TradeRecord> reconstructTradeSequence(const std::string& parent_trade_id) const;
    
    // Analytics
    struct TradeAnalytics {
        size_t total_trades;
        double total_volume;
        double total_commission;
        double total_realized_pnl;
        double win_rate;
        double average_trade_size;
        std::unordered_map<std::string, size_t> trades_by_strategy;
        std::unordered_map<std::string, size_t> trades_by_exchange;
    };
    
    TradeAnalytics calculateTradeAnalytics(const AuditQuery& query) const;
    
private:
    std::shared_ptr<HistoricalDataStorage> storage_;
    
    // Helper methods
    std::string generateTradeId() const;
    bool validateTradeRecord(const TradeRecord& trade) const;
};

} // namespace persistence
} // namespace data
} // namespace arbitrage
