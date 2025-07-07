#include "ErrorHandler.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iomanip>

namespace arbitrage {
namespace utils {

// ArbitrageException implementation
ArbitrageException::ArbitrageException(const std::string& message)
    : message_(message)
    , timestamp_(std::chrono::system_clock::now())
{
}

const char* ArbitrageException::what() const noexcept {
    return message_.c_str();
}

const std::string& ArbitrageException::getMessage() const noexcept {
    return message_;
}

std::chrono::system_clock::time_point ArbitrageException::getTimestamp() const noexcept {
    return timestamp_;
}

// Derived exception implementations
ConfigException::ConfigException(const std::string& message)
    : ArbitrageException("Configuration Error: " + message)
{
}

NetworkException::NetworkException(const std::string& message)
    : ArbitrageException("Network Error: " + message)
{
}

DataParsingException::DataParsingException(const std::string& message)
    : ArbitrageException("Data Parsing Error: " + message)
{
}

PricingException::PricingException(const std::string& message)
    : ArbitrageException("Pricing Error: " + message)
{
}

RiskException::RiskException(const std::string& message)
    : ArbitrageException("Risk Management Error: " + message)
{
}

// ErrorHandler implementation
void ErrorHandler::handleException(const std::exception& e, 
                                 const std::string& context, 
                                 bool should_throw) {
    std::string formatted_error = formatError(e.what(), context);
    
    // Log the error based on exception type
    if (dynamic_cast<const ConfigException*>(&e)) {
        LOG_ERROR("CONFIG ERROR: {}", formatted_error);
    } else if (dynamic_cast<const NetworkException*>(&e)) {
        LOG_ERROR("NETWORK ERROR: {}", formatted_error);
    } else if (dynamic_cast<const DataParsingException*>(&e)) {
        LOG_ERROR("DATA PARSING ERROR: {}", formatted_error);
    } else if (dynamic_cast<const PricingException*>(&e)) {
        LOG_ERROR("PRICING ERROR: {}", formatted_error);
    } else if (dynamic_cast<const RiskException*>(&e)) {
        LOG_ERROR("RISK ERROR: {}", formatted_error);
    } else {
        LOG_ERROR("GENERAL ERROR: {}", formatted_error);
    }
    
    if (should_throw) {
        throw;
    }
}

void ErrorHandler::logError(const std::string& error_message, 
                           const std::string& context,
                           int error_code) {
    std::string formatted_error = formatError(error_message, context);
    
    if (error_code != 0) {
        LOG_ERROR("{} (Error Code: {})", formatted_error, error_code);
    } else {
        LOG_ERROR("{}", formatted_error);
    }
}

std::string ErrorHandler::formatError(const std::string& message, 
                                     const std::string& context) {
    std::ostringstream oss;
    
    if (!context.empty()) {
        oss << "[" << context << "] ";
    }
    
    oss << message;
    
    return oss.str();
}

} // namespace utils
} // namespace arbitrage
