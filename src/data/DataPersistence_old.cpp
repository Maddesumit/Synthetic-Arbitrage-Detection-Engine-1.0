#include "DataPersistence.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <random>

namespace arbitrage {
namespace data {
namespace persistence {

// HistoricalDataStorage implementation
HistoricalDataStorage::HistoricalDataStorage(const std::string& database_path)
    : database_(nullptr), database_path_(database_path),
      insert_market_data_stmt_(nullptr), insert_arbitrage_opportunity_stmt_(nullptr),
      insert_time_series_stmt_(nullptr), async_writes_enabled_(false), writer_running_(false) {
    
    // Create directory if it doesn't exist
    std::filesystem::create_directories(std::filesystem::path(database_path_).parent_path());
}

HistoricalDataStorage::~HistoricalDataStorage() {
    if (writer_running_) {
        writer_running_ = false;
        write_queue_cv_.notify_all();
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }
    }
    
    if (insert_market_data_stmt_) sqlite3_finalize(insert_market_data_stmt_);
    if (insert_arbitrage_opportunity_stmt_) sqlite3_finalize(insert_arbitrage_opportunity_stmt_);
    if (insert_time_series_stmt_) sqlite3_finalize(insert_time_series_stmt_);
    
    if (database_) {
        sqlite3_close(database_);
    }
}

bool DataPersistence::saveTradeData(const TradeRecord& trade) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        // Save to file
        std::string filename = base_path_ + "/trades/trades_" + 
                              trade.timestamp.substr(0, 10) + ".csv"; // YYYY-MM-DD
        
        bool file_exists = std::filesystem::exists(filename);
        std::ofstream file(filename, std::ios::app);
        
        if (!file.is_open()) {
            return false;
        }
        
        // Write header if new file
        if (!file_exists) {
            file << "timestamp,trade_id,symbol,side,quantity,price,pnl,strategy,execution_time_ms\n";
        }
        
        // Write trade data
        file << trade.timestamp << ","
             << trade.trade_id << ","
             << trade.symbol << ","
             << trade.side << ","
             << std::fixed << std::setprecision(8) << trade.quantity << ","
             << std::fixed << std::setprecision(4) << trade.price << ","
             << std::fixed << std::setprecision(2) << trade.pnl << ","
             << trade.strategy << ","
             << trade.execution_time_ms << "\n";
        
        file.close();
        
        // Also save to in-memory cache
        trade_history_.push_back(trade);
        
        // Maintain cache size
        if (trade_history_.size() > max_cache_size_) {
            trade_history_.erase(trade_history_.begin());
        }
        
        return true;
        
    } catch (const std::exception& e) {
        // Log error
        return false;
    }
}

bool DataPersistence::saveMarketData(const MarketDataRecord& data) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        std::string filename = base_path_ + "/market_data/" + data.symbol + "_" +
                              data.timestamp.substr(0, 10) + ".csv";
        
        bool file_exists = std::filesystem::exists(filename);
        std::ofstream file(filename, std::ios::app);
        
        if (!file.is_open()) {
            return false;
        }
        
        // Write header if new file
        if (!file_exists) {
            file << "timestamp,symbol,price,volume,bid,ask,spread\n";
        }
        
        // Write market data
        file << data.timestamp << ","
             << data.symbol << ","
             << std::fixed << std::setprecision(4) << data.price << ","
             << std::fixed << std::setprecision(2) << data.volume << ","
             << std::fixed << std::setprecision(4) << data.bid << ","
             << std::fixed << std::setprecision(4) << data.ask << ","
             << std::fixed << std::setprecision(6) << data.spread << "\n";
        
        file.close();
        
        // Update in-memory cache
        market_data_cache_[data.symbol].push_back(data);
        
        // Maintain cache size per symbol
        if (market_data_cache_[data.symbol].size() > max_cache_size_) {
            market_data_cache_[data.symbol].erase(market_data_cache_[data.symbol].begin());
        }
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool DataPersistence::saveAnalyticsData(const AnalyticsRecord& analytics) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        std::string filename = base_path_ + "/analytics/analytics_" +
                              analytics.timestamp.substr(0, 10) + ".csv";
        
        bool file_exists = std::filesystem::exists(filename);
        std::ofstream file(filename, std::ios::app);
        
        if (!file.is_open()) {
            return false;
        }
        
        // Write header if new file
        if (!file_exists) {
            file << "timestamp,metric_name,value,category,symbol\n";
        }
        
        // Write analytics data
        file << analytics.timestamp << ","
             << analytics.metric_name << ","
             << std::fixed << std::setprecision(6) << analytics.value << ","
             << analytics.category << ","
             << analytics.symbol << "\n";
        
        file.close();
        
        // Update analytics cache
        analytics_cache_[analytics.metric_name].push_back(analytics);
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<TradeRecord> DataPersistence::loadTradeHistory(
    const std::string& start_date,
    const std::string& end_date
) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<TradeRecord> trades;
    
    try {
        // Generate list of dates to check
        std::vector<std::string> dates_to_check = generateDateRange(start_date, end_date);
        
        for (const auto& date : dates_to_check) {
            std::string filename = base_path_ + "/trades/trades_" + date + ".csv";
            
            if (!std::filesystem::exists(filename)) {
                continue;
            }
            
            std::ifstream file(filename);
            if (!file.is_open()) {
                continue;
            }
            
            std::string line;
            std::getline(file, line); // Skip header
            
            while (std::getline(file, line)) {
                TradeRecord trade = parseTradeRecord(line);
                if (!trade.trade_id.empty()) {
                    trades.push_back(trade);
                }
            }
            
            file.close();
        }
        
        // Sort by timestamp
        std::sort(trades.begin(), trades.end(), 
                 [](const TradeRecord& a, const TradeRecord& b) {
                     return a.timestamp < b.timestamp;
                 });
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return trades;
}

std::vector<MarketDataRecord> DataPersistence::loadMarketData(
    const std::string& symbol,
    const std::string& start_date,
    const std::string& end_date
) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<MarketDataRecord> data;
    
    try {
        std::vector<std::string> dates_to_check = generateDateRange(start_date, end_date);
        
        for (const auto& date : dates_to_check) {
            std::string filename = base_path_ + "/market_data/" + symbol + "_" + date + ".csv";
            
            if (!std::filesystem::exists(filename)) {
                continue;
            }
            
            std::ifstream file(filename);
            if (!file.is_open()) {
                continue;
            }
            
            std::string line;
            std::getline(file, line); // Skip header
            
            while (std::getline(file, line)) {
                MarketDataRecord record = parseMarketDataRecord(line);
                if (!record.symbol.empty()) {
                    data.push_back(record);
                }
            }
            
            file.close();
        }
        
        // Sort by timestamp
        std::sort(data.begin(), data.end(),
                 [](const MarketDataRecord& a, const MarketDataRecord& b) {
                     return a.timestamp < b.timestamp;
                 });
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return data;
}

PerformanceMetrics DataPersistence::calculatePerformanceMetrics(
    const std::string& start_date,
    const std::string& end_date
) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    PerformanceMetrics metrics;
    
    try {
        auto trades = loadTradeHistory(start_date, end_date);
        
        if (trades.empty()) {
            return metrics;
        }
        
        // Calculate basic metrics
        double total_pnl = 0.0;
        double total_volume = 0.0;
        int winning_trades = 0;
        int losing_trades = 0;
        double max_profit = 0.0;
        double max_loss = 0.0;
        double total_execution_time = 0.0;
        
        std::map<std::string, int> strategy_counts;
        std::map<std::string, double> strategy_pnl;
        
        for (const auto& trade : trades) {
            total_pnl += trade.pnl;
            total_volume += std::abs(trade.quantity * trade.price);
            total_execution_time += trade.execution_time_ms;
            
            if (trade.pnl > 0) {
                winning_trades++;
                max_profit = std::max(max_profit, trade.pnl);
            } else if (trade.pnl < 0) {
                losing_trades++;
                max_loss = std::min(max_loss, trade.pnl);
            }
            
            strategy_counts[trade.strategy]++;
            strategy_pnl[trade.strategy] += trade.pnl;
        }
        
        metrics.total_trades = trades.size();
        metrics.total_pnl = total_pnl;
        metrics.total_volume = total_volume;
        metrics.winning_trades = winning_trades;
        metrics.losing_trades = losing_trades;
        metrics.win_rate = static_cast<double>(winning_trades) / trades.size();
        metrics.average_pnl_per_trade = total_pnl / trades.size();
        metrics.max_profit = max_profit;
        metrics.max_loss = max_loss;
        metrics.average_execution_time = total_execution_time / trades.size();
        
        // Calculate Sharpe ratio (simplified)
        if (!trades.empty()) {
            std::vector<double> daily_returns;
            std::map<std::string, double> daily_pnl;
            
            for (const auto& trade : trades) {
                std::string date = trade.timestamp.substr(0, 10);
                daily_pnl[date] += trade.pnl;
            }
            
            for (const auto& day_pnl : daily_pnl) {
                daily_returns.push_back(day_pnl.second);
            }
            
            if (daily_returns.size() > 1) {
                double mean_return = total_pnl / daily_returns.size();
                double variance = 0.0;
                
                for (double ret : daily_returns) {
                    variance += (ret - mean_return) * (ret - mean_return);
                }
                variance /= daily_returns.size();
                
                double std_dev = std::sqrt(variance);
                metrics.sharpe_ratio = std_dev > 0 ? mean_return / std_dev : 0.0;
            }
        }
        
        // Calculate maximum drawdown
        metrics.max_drawdown = calculateMaxDrawdown(trades);
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return metrics;
}

bool DataPersistence::generateReport(
    const std::string& report_type,
    const std::string& start_date,
    const std::string& end_date,
    const std::string& output_file
) {
    try {
        std::string filename = base_path_ + "/reports/" + output_file;
        std::ofstream file(filename);
        
        if (!file.is_open()) {
            return false;
        }
        
        // Generate report header
        file << "# " << report_type << " Report\n";
        file << "Period: " << start_date << " to " << end_date << "\n";
        file << "Generated: " << getCurrentTimestamp() << "\n\n";
        
        if (report_type == "performance") {
            generatePerformanceReport(file, start_date, end_date);
        } else if (report_type == "trades") {
            generateTradeReport(file, start_date, end_date);
        } else if (report_type == "risk") {
            generateRiskReport(file, start_date, end_date);
        }
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

void DataPersistence::initializeDatabase() {
    // Placeholder for database initialization
    // In production, would initialize SQLite/PostgreSQL connection
    db_connection_ = nullptr; // Placeholder
}

std::vector<std::string> DataPersistence::generateDateRange(
    const std::string& start_date,
    const std::string& end_date
) {
    std::vector<std::string> dates;
    
    // Simple implementation - in production would use proper date library
    dates.push_back(start_date);
    if (start_date != end_date) {
        dates.push_back(end_date);
    }
    
    return dates;
}

TradeRecord DataPersistence::parseTradeRecord(const std::string& line) {
    TradeRecord trade;
    
    std::stringstream ss(line);
    std::string item;
    
    try {
        std::getline(ss, trade.timestamp, ',');
        std::getline(ss, trade.trade_id, ',');
        std::getline(ss, trade.symbol, ',');
        std::getline(ss, trade.side, ',');
        
        std::getline(ss, item, ',');
        trade.quantity = std::stod(item);
        
        std::getline(ss, item, ',');
        trade.price = std::stod(item);
        
        std::getline(ss, item, ',');
        trade.pnl = std::stod(item);
        
        std::getline(ss, trade.strategy, ',');
        
        std::getline(ss, item, ',');
        trade.execution_time_ms = std::stod(item);
        
    } catch (const std::exception& e) {
        trade.trade_id.clear(); // Mark as invalid
    }
    
    return trade;
}

MarketDataRecord DataPersistence::parseMarketDataRecord(const std::string& line) {
    MarketDataRecord data;
    
    std::stringstream ss(line);
    std::string item;
    
    try {
        std::getline(ss, data.timestamp, ',');
        std::getline(ss, data.symbol, ',');
        
        std::getline(ss, item, ',');
        data.price = std::stod(item);
        
        std::getline(ss, item, ',');
        data.volume = std::stod(item);
        
        std::getline(ss, item, ',');
        data.bid = std::stod(item);
        
        std::getline(ss, item, ',');
        data.ask = std::stod(item);
        
        std::getline(ss, item, ',');
        data.spread = std::stod(item);
        
    } catch (const std::exception& e) {
        data.symbol.clear(); // Mark as invalid
    }
    
    return data;
}

double DataPersistence::calculateMaxDrawdown(const std::vector<TradeRecord>& trades) {
    if (trades.empty()) return 0.0;
    
    double running_pnl = 0.0;
    double peak_pnl = 0.0;
    double max_drawdown = 0.0;
    
    for (const auto& trade : trades) {
        running_pnl += trade.pnl;
        
        if (running_pnl > peak_pnl) {
            peak_pnl = running_pnl;
        }
        
        double drawdown = peak_pnl - running_pnl;
        max_drawdown = std::max(max_drawdown, drawdown);
    }
    
    return max_drawdown;
}

std::string DataPersistence::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void DataPersistence::generatePerformanceReport(
    std::ofstream& file,
    const std::string& start_date,
    const std::string& end_date
) {
    auto metrics = calculatePerformanceMetrics(start_date, end_date);
    
    file << "## Performance Summary\n\n";
    file << "Total Trades: " << metrics.total_trades << "\n";
    file << "Total PnL: $" << std::fixed << std::setprecision(2) << metrics.total_pnl << "\n";
    file << "Total Volume: $" << std::fixed << std::setprecision(0) << metrics.total_volume << "\n";
    file << "Win Rate: " << std::fixed << std::setprecision(1) << (metrics.win_rate * 100) << "%\n";
    file << "Average PnL per Trade: $" << std::fixed << std::setprecision(2) << metrics.average_pnl_per_trade << "\n";
    file << "Max Profit: $" << std::fixed << std::setprecision(2) << metrics.max_profit << "\n";
    file << "Max Loss: $" << std::fixed << std::setprecision(2) << metrics.max_loss << "\n";
    file << "Max Drawdown: $" << std::fixed << std::setprecision(2) << metrics.max_drawdown << "\n";
    file << "Sharpe Ratio: " << std::fixed << std::setprecision(3) << metrics.sharpe_ratio << "\n";
    file << "Average Execution Time: " << std::fixed << std::setprecision(1) << metrics.average_execution_time << " ms\n\n";
}

void DataPersistence::generateTradeReport(
    std::ofstream& file,
    const std::string& start_date,
    const std::string& end_date
) {
    auto trades = loadTradeHistory(start_date, end_date);
    
    file << "## Trade Details\n\n";
    file << "| Timestamp | Trade ID | Symbol | Side | Quantity | Price | PnL | Strategy | Exec Time |\n";
    file << "|-----------|----------|--------|------|----------|-------|-----|----------|-----------|";
    
    for (const auto& trade : trades) {
        file << "| " << trade.timestamp
             << " | " << trade.trade_id
             << " | " << trade.symbol
             << " | " << trade.side
             << " | " << std::fixed << std::setprecision(4) << trade.quantity
             << " | $" << std::fixed << std::setprecision(2) << trade.price
             << " | $" << std::fixed << std::setprecision(2) << trade.pnl
             << " | " << trade.strategy
             << " | " << std::fixed << std::setprecision(1) << trade.execution_time_ms << "ms |\n";
    }
    
    file << "\n";
}

void DataPersistence::generateRiskReport(
    std::ofstream& file,
    const std::string& start_date,
    const std::string& end_date
) {
    file << "## Risk Analysis\n\n";
    file << "Risk metrics and analysis would be generated here based on position data and market data.\n\n";
    
    // In a real implementation, this would calculate VaR, expected shortfall, etc.
    // from the historical data and current positions
}

} // namespace Data
} // namespace SyntheticArbitrage
