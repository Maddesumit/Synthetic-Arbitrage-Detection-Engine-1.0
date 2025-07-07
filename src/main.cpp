#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>

#include "utils/Logger.hpp"
#include "utils/ConfigManager.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/ThreadUtils.hpp"

using namespace arbitrage::utils;

// Global flag for graceful shutdown
std::atomic<bool> should_shutdown{false};

void signalHandler(int signal) {
    LOG_INFO("Received signal {}, initiating graceful shutdown...", signal);
    should_shutdown = true;
}

int main(int argc, char* argv[]) {
    try {
        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Determine config file path
        std::string config_path = "config/config.json";
        if (argc > 1) {
            config_path = argv[1];
        }
        
        std::cout << "Starting Synthetic Arbitrage Detection Engine..." << std::endl;
        std::cout << "Using configuration: " << config_path << std::endl;
        
        // Initialize configuration
        std::unique_ptr<ConfigManager> config;
        try {
            config = std::make_unique<ConfigManager>(config_path, true);
            std::cout << "Configuration loaded successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load configuration: " << e.what() << std::endl;
            return 1;
        }
        
        // Initialize logging system
        try {
            auto log_level_str = config->getString("logging.console_level", "info");
            Logger::Level console_level = Logger::Level::INFO;
            
            if (log_level_str == "trace") console_level = Logger::Level::TRACE;
            else if (log_level_str == "debug") console_level = Logger::Level::DEBUG;
            else if (log_level_str == "info") console_level = Logger::Level::INFO;
            else if (log_level_str == "warn") console_level = Logger::Level::WARN;
            else if (log_level_str == "error") console_level = Logger::Level::ERROR;
            else if (log_level_str == "critical") console_level = Logger::Level::CRITICAL;
            
            auto file_level_str = config->getString("logging.file_level", "debug");
            Logger::Level file_level = Logger::Level::DEBUG;
            
            if (file_level_str == "trace") file_level = Logger::Level::TRACE;
            else if (file_level_str == "debug") file_level = Logger::Level::DEBUG;
            else if (file_level_str == "info") file_level = Logger::Level::INFO;
            else if (file_level_str == "warn") file_level = Logger::Level::WARN;
            else if (file_level_str == "error") file_level = Logger::Level::ERROR;
            else if (file_level_str == "critical") file_level = Logger::Level::CRITICAL;
            
            Logger::initialize(
                config->getString("logging.log_directory", "logs") + "/arbitrage_engine.log",
                console_level,
                file_level,
                config->getInt("logging.max_file_size_mb", 10) * 1024 * 1024,
                config->getInt("logging.max_files", 5),
                config->getInt("logging.async_queue_size", 8192)
            );
            
            LOG_INFO("Logging system initialized successfully");
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize logging: " << e.what() << std::endl;
            return 1;
        }
        
        // Validate configuration
        if (!config->validate()) {
            LOG_ERROR("Configuration validation failed");
            return 1;
        }
        LOG_INFO("Configuration validation passed");
        
        // Log system information
        LOG_INFO("=== Synthetic Arbitrage Detection Engine ===");
        LOG_INFO("Version: {}", config->getString("system.version", "1.0.0"));
        LOG_INFO("Environment: {}", config->getString("system.environment", "development"));
        LOG_INFO("Hardware threads: {}", std::thread::hardware_concurrency());
        
        // Log configuration summary
        LOG_INFO("Configuration Summary:");
        LOG_INFO("  - Exchanges enabled: Binance={}, OKX={}, Bybit={}", 
                config->getBool("exchanges.binance.enabled"),
                config->getBool("exchanges.okx.enabled"), 
                config->getBool("exchanges.bybit.enabled"));
        LOG_INFO("  - Min profit threshold: {}%", 
                config->getDouble("trading.min_profit_threshold") * 100);
        LOG_INFO("  - Max capital per trade: ${}", 
                config->getInt("trading.max_capital_per_trade"));
        LOG_INFO("  - Target latency: {}ms", 
                config->getInt("performance.detection_latency_ms"));
        LOG_INFO("  - Target throughput: {} updates/sec", 
                config->getInt("performance.target_throughput"));
        
        // Initialize thread pool
        auto thread_count = std::max(2U, std::thread::hardware_concurrency() / 2);
        ThreadPool thread_pool(thread_count);
        LOG_INFO("Thread pool initialized with {} threads", thread_count);
        
        // Register configuration change callback
        config->registerChangeCallback([](const std::string& key, const std::string& value) {
            LOG_INFO("Configuration changed: {} = {}", key, value);
        });
        
        // Main application loop
        LOG_INFO("Starting main application loop...");
        LOG_INFO("Press Ctrl+C to stop gracefully");
        
        Timer uptime_timer;
        uptime_timer.start();
        
        while (!should_shutdown) {
            try {
                // Main application logic will be implemented in subsequent phases
                // For now, just sleep and log periodic status
                
                std::this_thread::sleep_for(std::chrono::seconds(10));
                
                auto uptime_seconds = uptime_timer.stopMilliseconds() / 1000;
                LOG_INFO("System running... Uptime: {}s, Pending tasks: {}", 
                        uptime_seconds, thread_pool.getPendingTasks());
                
                uptime_timer.start(); // Restart timer
                
            } catch (const std::exception& e) {
                ErrorHandler::handleException(e, "Main loop", false);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        // Graceful shutdown
        LOG_INFO("Initiating graceful shutdown...");
        
        // In future phases, we'll shutdown:
        // - WebSocket connections
        // - Market data processors
        // - Arbitrage detection engine
        // - Risk management system
        // - Database connections
        
        LOG_INFO("Shutdown complete. Total uptime: {}s", 
                uptime_timer.stopMilliseconds() / 1000);
        
        Logger::shutdown();
        std::cout << "Synthetic Arbitrage Detection Engine stopped successfully" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        Logger::shutdown();
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        Logger::shutdown();
        return 1;
    }
}
