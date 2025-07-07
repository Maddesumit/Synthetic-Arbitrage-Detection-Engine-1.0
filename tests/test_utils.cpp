#include <catch2/catch_test_macros.hpp>
#include "utils/ConfigManager.hpp"
#include "utils/Logger.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/ThreadUtils.hpp"
#include <filesystem>
#include <fstream>

using namespace arbitrage::utils;

// Test configuration manager
TEST_CASE("ConfigManager basic functionality", "[config]") {
    // Create temporary config file
    const std::string config_path = "test_config.json";
    
    // Write test config
    std::ofstream config_file(config_path);
    if (!config_file.is_open()) {
        FAIL("Could not create test config file");
    }
    config_file << R"({
        "exchanges": {
            "binance": {
                "enabled": true,
                "api_key": "test_key"
            }
        },
        "trading": {
            "min_profit_threshold": 0.001,
            "max_capital_per_trade": 10000
        },
        "risk": {
            "max_position_size": 1000000
        },
        "performance": {
            "detection_latency_ms": 10,
            "target_throughput": 2000
        }
    })";
    config_file.close();
    
    SECTION("Load and read configuration") {
        try {
            ConfigManager config(config_path, false); // Disable hot reload for testing
            
            REQUIRE(config.getBool("exchanges.binance.enabled") == true);
            REQUIRE(config.getString("exchanges.binance.api_key") == "test_key");
            REQUIRE(config.getDouble("trading.min_profit_threshold") == 0.001);
            REQUIRE(config.getInt("trading.max_capital_per_trade") == 10000);
        } catch (const std::exception& e) {
            FAIL("ConfigManager failed: " << e.what());
        }
    }
    
    SECTION("Default values") {
        try {
            ConfigManager config(config_path, false);
            
            REQUIRE(config.getString("nonexistent.key", "default") == "default");
            REQUIRE(config.getInt("nonexistent.key", 42) == 42);
            REQUIRE(config.getDouble("nonexistent.key", 3.14) == 3.14);
            REQUIRE(config.getBool("nonexistent.key", true) == true);
        } catch (const std::exception& e) {
            FAIL("ConfigManager default values failed: " << e.what());
        }
    }
    
    SECTION("Key existence") {
        try {
            ConfigManager config(config_path, false);
            
            REQUIRE(config.hasKey("exchanges.binance.enabled") == true);
            REQUIRE(config.hasKey("nonexistent.key") == false);
        } catch (const std::exception& e) {
            FAIL("ConfigManager key existence failed: " << e.what());
        }
    }
    
    SECTION("Configuration validation") {
        try {
            ConfigManager config(config_path, false);
            
            REQUIRE(config.validate() == true);
        } catch (const std::exception& e) {
            FAIL("ConfigManager validation failed: " << e.what());
        }
    }
    
    // Cleanup
    std::filesystem::remove(config_path);
}

// Test logging system
TEST_CASE("Logger functionality", "[logging]") {
    SECTION("Logger initialization") {
        try {
            // Ensure the test_logs directory exists
            std::filesystem::create_directories("test_logs");
            
            REQUIRE_NOTHROW(Logger::initialize("test_logs/test.log"));
            
            auto main_logger = Logger::get();
            REQUIRE(main_logger != nullptr);
            
            auto perf_logger = Logger::getPerformanceLogger();
            REQUIRE(perf_logger != nullptr);
            
            Logger::shutdown();
        } catch (const std::exception& e) {
            FAIL("Logger test failed: " << e.what());
        }
    }
}

// Test error handling
TEST_CASE("Error handling", "[error]") {
    try {
        std::filesystem::create_directories("test_logs");
        Logger::initialize("test_logs/error_test.log");
        
        SECTION("Custom exceptions") {
            REQUIRE_THROWS_AS(throw ConfigException("Test config error"), ConfigException);
            REQUIRE_THROWS_AS(throw NetworkException("Test network error"), NetworkException);
            REQUIRE_THROWS_AS(throw PricingException("Test pricing error"), PricingException);
        }
        
        SECTION("Error handler") {
            ConfigException e("Test error");
            REQUIRE_NOTHROW(ErrorHandler::handleException(e, "Test context", false));
            REQUIRE_NOTHROW(ErrorHandler::logError("Test error message", "Test context"));
        }
        
        Logger::shutdown();
    } catch (const std::exception& e) {
        FAIL("Error handling test failed: " << e.what());
    }
}

// Test thread utilities
TEST_CASE("Thread utilities", "[threading]") {
    SECTION("ThreadSafeQueue") {
        try {
            ThreadSafeQueue<int> queue;
            
            // Test push/pop
            queue.push(42);
            REQUIRE(queue.size() == 1);
            REQUIRE(!queue.empty());
            
            int value;
            REQUIRE(queue.tryPop(value) == true);
            REQUIRE(value == 42);
            REQUIRE(queue.empty());
        } catch (const std::exception& e) {
            FAIL("ThreadSafeQueue test failed: " << e.what());
        }
    }
    
    SECTION("ThreadPool") {
        try {
            ThreadPool pool(2);
            
            std::atomic<int> counter{0};
            std::vector<std::future<void>> futures;
            
            // Submit tasks and collect futures
            for (int i = 0; i < 10; ++i) {
                futures.push_back(pool.submitWithResult([&counter]() { counter++; }));
            }
            
            // Wait for all tasks to complete
            for (auto& future : futures) {
                future.wait();
            }
            
            REQUIRE(counter.load() == 10);
        } catch (const std::exception& e) {
            FAIL("ThreadPool test failed: " << e.what());
        }
    }
    
    SECTION("Timer") {
        try {
            Timer timer;
            timer.start();
            
            // Brief pause to ensure timer measurement
            auto start = std::chrono::high_resolution_clock::now();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count() < 1) {
                // Busy wait for 1ms
            }
            
            auto elapsed_ms = timer.stopMilliseconds();
            REQUIRE(elapsed_ms >= 1);
        } catch (const std::exception& e) {
            FAIL("Timer test failed: " << e.what());
        }
    }
}

// Test data generators for development
TEST_CASE("Mock data generators", "[mock]") {
    SECTION("Generate mock orderbook data") {
        // This will be expanded when we create market data structures
        REQUIRE(true); // Placeholder
    }
    
    SECTION("Generate mock price data") {
        // This will be expanded when we create pricing structures
        REQUIRE(true); // Placeholder
    }
}
