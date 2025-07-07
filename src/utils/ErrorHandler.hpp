#pragma once

#include <exception>
#include <string>
#include <chrono>

namespace arbitrage {
namespace utils {

/**
 * @brief Base exception class for arbitrage engine
 */
class ArbitrageException : public std::exception {
public:
    explicit ArbitrageException(const std::string& message);
    const char* what() const noexcept override;
    
    const std::string& getMessage() const noexcept;
    std::chrono::system_clock::time_point getTimestamp() const noexcept;

private:
    std::string message_;
    std::chrono::system_clock::time_point timestamp_;
};

/**
 * @brief Configuration related exceptions
 */
class ConfigException : public ArbitrageException {
public:
    explicit ConfigException(const std::string& message);
};

/**
 * @brief Network and connectivity exceptions
 */
class NetworkException : public ArbitrageException {
public:
    explicit NetworkException(const std::string& message);
};

/**
 * @brief Market data parsing exceptions
 */
class DataParsingException : public ArbitrageException {
public:
    explicit DataParsingException(const std::string& message);
};

/**
 * @brief Pricing calculation exceptions
 */
class PricingException : public ArbitrageException {
public:
    explicit PricingException(const std::string& message);
};

/**
 * @brief Risk management exceptions
 */
class RiskException : public ArbitrageException {
public:
    explicit RiskException(const std::string& message);
};

/**
 * @brief Error handling utility functions
 */
class ErrorHandler {
public:
    /**
     * @brief Handle and log exceptions gracefully
     * @param e Exception to handle
     * @param context Additional context information
     * @param should_throw Whether to re-throw the exception
     */
    static void handleException(const std::exception& e, 
                              const std::string& context = "", 
                              bool should_throw = false);
    
    /**
     * @brief Log error with context
     * @param error_message Error description
     * @param context Where the error occurred
     * @param error_code Optional error code
     */
    static void logError(const std::string& error_message, 
                        const std::string& context = "",
                        int error_code = 0);
    
    /**
     * @brief Create error message with timestamp and context
     */
    static std::string formatError(const std::string& message, 
                                  const std::string& context = "");
};

} // namespace utils
} // namespace arbitrage
