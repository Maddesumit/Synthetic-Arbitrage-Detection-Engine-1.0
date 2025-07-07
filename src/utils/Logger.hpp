#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <memory>
#include <string>

namespace arbitrage {
namespace utils {

/**
 * @brief High-performance logging system using spdlog
 * 
 * Features:
 * - Multiple log levels (trace, debug, info, warn, error, critical)
 * - Async logging for high-performance applications
 * - File rotation to manage disk space
 * - Thread-safe operations
 * - Structured logging with formatting
 * - Performance metrics logging
 */
class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5
    };
    
    /**
     * @brief Initialize the logging system
     * @param log_file_path Path to the log file
     * @param console_level Console logging level
     * @param file_level File logging level
     * @param max_file_size Maximum file size before rotation (in bytes)
     * @param max_files Maximum number of rotated files to keep
     * @param async_queue_size Size of async logging queue
     */
    static void initialize(
        const std::string& log_file_path = "logs/arbitrage_engine.log",
        Level console_level = Level::INFO,
        Level file_level = Level::DEBUG,
        size_t max_file_size = 1024 * 1024 * 10, // 10MB
        size_t max_files = 5,
        size_t async_queue_size = 8192
    );
    
    /**
     * @brief Get the main logger instance
     */
    static std::shared_ptr<spdlog::logger> get();
    
    /**
     * @brief Get performance logger for latency/throughput metrics
     */
    static std::shared_ptr<spdlog::logger> getPerformanceLogger();
    
    /**
     * @brief Get market data logger for trade/orderbook updates
     */
    static std::shared_ptr<spdlog::logger> getMarketDataLogger();
    
    /**
     * @brief Get arbitrage logger for opportunity detection
     */
    static std::shared_ptr<spdlog::logger> getArbitrageLogger();
    
    /**
     * @brief Get risk logger for risk management events
     */
    static std::shared_ptr<spdlog::logger> getRiskLogger();
    
    /**
     * @brief Set console logging level
     */
    static void setConsoleLevel(Level level);
    
    /**
     * @brief Set file logging level
     */
    static void setFileLevel(Level level);
    
    /**
     * @brief Flush all loggers
     */
    static void flush();
    
    /**
     * @brief Shutdown logging system (call on exit)
     */
    static void shutdown();
    
    // Convenient static methods for logging
    template<typename... Args>
    static void trace(const std::string& msg, Args&&... args) {
        get()->trace(msg, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void debug(const std::string& msg, Args&&... args) {
        get()->debug(msg, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(const std::string& msg, Args&&... args) {
        get()->info(msg, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(const std::string& msg, Args&&... args) {
        get()->warn(msg, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(const std::string& msg, Args&&... args) {
        get()->error(msg, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void critical(const std::string& msg, Args&&... args) {
        get()->critical(msg, std::forward<Args>(args)...);
    }

private:
    static std::shared_ptr<spdlog::logger> main_logger_;
    static std::shared_ptr<spdlog::logger> performance_logger_;
    static std::shared_ptr<spdlog::logger> market_data_logger_;
    static std::shared_ptr<spdlog::logger> arbitrage_logger_;
    static std::shared_ptr<spdlog::logger> risk_logger_;
    
    static spdlog::level::level_enum toSpdlogLevel(Level level);
};

} // namespace utils
} // namespace arbitrage

// Convenient macros for logging
#define LOG_TRACE(...) arbitrage::utils::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) arbitrage::utils::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) arbitrage::utils::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) arbitrage::utils::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) arbitrage::utils::Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) arbitrage::utils::Logger::get()->critical(__VA_ARGS__)

// Specialized logging macros
#define LOG_PERFORMANCE(...) arbitrage::utils::Logger::getPerformanceLogger()->info(__VA_ARGS__)
#define LOG_MARKET_DATA(...) arbitrage::utils::Logger::getMarketDataLogger()->info(__VA_ARGS__)
#define LOG_ARBITRAGE(...) arbitrage::utils::Logger::getArbitrageLogger()->info(__VA_ARGS__)
#define LOG_RISK(...) arbitrage::utils::Logger::getRiskLogger()->info(__VA_ARGS__)
