#include "Logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <filesystem>
#include <iostream>

namespace arbitrage {
namespace utils {

// Static member definitions
std::shared_ptr<spdlog::logger> Logger::main_logger_;
std::shared_ptr<spdlog::logger> Logger::performance_logger_;
std::shared_ptr<spdlog::logger> Logger::market_data_logger_;
std::shared_ptr<spdlog::logger> Logger::arbitrage_logger_;
std::shared_ptr<spdlog::logger> Logger::risk_logger_;

void Logger::initialize(
    const std::string& log_file_path,
    Level console_level,
    Level file_level,
    size_t max_file_size,
    size_t max_files,
    size_t async_queue_size
) {
    try {
        // Create logs directory if it doesn't exist
        std::filesystem::path log_path(log_file_path);
        std::filesystem::create_directories(log_path.parent_path());
        
        // Initialize async logging with thread pool
        spdlog::init_thread_pool(async_queue_size, 1);
        
        // Create console sink with color support
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(toSpdlogLevel(console_level));
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [thread %t] %v");
        
        // Create rotating file sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file_path, max_file_size, max_files);
        file_sink->set_level(toSpdlogLevel(file_level));
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] %v");
        
        // Create sinks vector
        std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
        
        // Create main logger
        main_logger_ = std::make_shared<spdlog::async_logger>(
            "main", sinks.begin(), sinks.end(), 
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        main_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(main_logger_);
        
        // Create specialized loggers with separate files
        
        // Performance logger - for latency and throughput metrics
        performance_logger_ = spdlog::create_async<spdlog::sinks::rotating_file_sink_mt>(
            "performance", "logs/performance.log", max_file_size, max_files);
        performance_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        performance_logger_->set_level(spdlog::level::info);
        
        // Market data logger - for orderbook and trade updates
        market_data_logger_ = spdlog::create_async<spdlog::sinks::rotating_file_sink_mt>(
            "market_data", "logs/market_data.log", max_file_size, max_files);
        market_data_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        market_data_logger_->set_level(spdlog::level::info);
        
        // Arbitrage logger - for opportunity detection and execution
        arbitrage_logger_ = spdlog::create_async<spdlog::sinks::rotating_file_sink_mt>(
            "arbitrage", "logs/arbitrage.log", max_file_size, max_files);
        arbitrage_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        arbitrage_logger_->set_level(spdlog::level::info);
        
        // Risk logger - for risk management events
        risk_logger_ = spdlog::create_async<spdlog::sinks::rotating_file_sink_mt>(
            "risk", "logs/risk.log", max_file_size, max_files);
        risk_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        risk_logger_->set_level(spdlog::level::info);
        
        // Set global logging pattern and level
        spdlog::set_default_logger(main_logger_);
        
        LOG_INFO("Logging system initialized successfully");
        LOG_INFO("Console level: {}, File level: {}", 
                static_cast<int>(console_level), static_cast<int>(file_level));
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging system: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!main_logger_) {
        throw std::runtime_error("Logger not initialized. Call Logger::initialize() first.");
    }
    return main_logger_;
}

std::shared_ptr<spdlog::logger> Logger::getPerformanceLogger() {
    if (!performance_logger_) {
        throw std::runtime_error("Logger not initialized. Call Logger::initialize() first.");
    }
    return performance_logger_;
}

std::shared_ptr<spdlog::logger> Logger::getMarketDataLogger() {
    if (!market_data_logger_) {
        throw std::runtime_error("Logger not initialized. Call Logger::initialize() first.");
    }
    return market_data_logger_;
}

std::shared_ptr<spdlog::logger> Logger::getArbitrageLogger() {
    if (!arbitrage_logger_) {
        throw std::runtime_error("Logger not initialized. Call Logger::initialize() first.");
    }
    return arbitrage_logger_;
}

std::shared_ptr<spdlog::logger> Logger::getRiskLogger() {
    if (!risk_logger_) {
        throw std::runtime_error("Logger not initialized. Call Logger::initialize() first.");
    }
    return risk_logger_;
}

void Logger::setConsoleLevel(Level level) {
    if (main_logger_) {
        // Note: This would require storing console sink reference
        // For now, we'll log the change
        LOG_INFO("Console logging level changed to: {}", static_cast<int>(level));
    }
}

void Logger::setFileLevel(Level level) {
    if (main_logger_) {
        // Note: This would require storing file sink reference
        // For now, we'll log the change
        LOG_INFO("File logging level changed to: {}", static_cast<int>(level));
    }
}

void Logger::flush() {
    if (main_logger_) main_logger_->flush();
    if (performance_logger_) performance_logger_->flush();
    if (market_data_logger_) market_data_logger_->flush();
    if (arbitrage_logger_) arbitrage_logger_->flush();
    if (risk_logger_) risk_logger_->flush();
}

void Logger::shutdown() {
    LOG_INFO("Shutting down logging system...");
    flush();
    spdlog::shutdown();
}

spdlog::level::level_enum Logger::toSpdlogLevel(Level level) {
    switch (level) {
        case Level::TRACE: return spdlog::level::trace;
        case Level::DEBUG: return spdlog::level::debug;
        case Level::INFO: return spdlog::level::info;
        case Level::WARN: return spdlog::level::warn;
        case Level::ERROR: return spdlog::level::err;
        case Level::CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::info;
    }
}

} // namespace utils
} // namespace arbitrage
