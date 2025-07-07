#include "ConfigurationManager.hpp"
#include "../utils/Logger.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace arbitrage {
namespace config {

// Helper function to split configuration keys like "trading.min_profit_threshold"
std::vector<std::string> splitKey(const std::string& key) {
    std::vector<std::string> result;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

ConfigurationManager& ConfigurationManager::getInstance() {
    static ConfigurationManager instance;
    return instance;
}

ConfigurationManager::ConfigurationManager() 
    : current_environment_("development"),
      hot_reload_enabled_(false),
      reload_interval_(std::chrono::seconds(5)),
      running_(false) {
}

ConfigurationManager::~ConfigurationManager() {
    running_ = false;
    if (hot_reload_thread_.joinable()) {
        hot_reload_thread_.join();
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& config_file) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);
    
    try {
        config_file_ = config_file;
        
        if (!std::filesystem::exists(config_file)) {
            utils::Logger::error("Configuration file not found: " + config_file);
            return false;
        }
        
        std::ifstream file(config_file);
        if (!file.is_open()) {
            utils::Logger::error("Failed to open configuration file: " + config_file);
            return false;
        }
        
        file >> config_;
        last_modified_ = std::filesystem::last_write_time(config_file);
        
        utils::Logger::info("Configuration loaded successfully from: " + config_file);
        
        // Validate configuration after loading
        if (!validateConfiguration()) {
            utils::Logger::warn("Configuration validation failed");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        utils::Logger::error("Error loading configuration: " + std::string(e.what()));
        return false;
    }
}

bool ConfigurationManager::reloadConfiguration() {
    if (config_file_.empty()) {
        utils::Logger::error("No configuration file specified for reload");
        return false;
    }
    
    try {
        // Check if file has been modified
        auto current_modified = std::filesystem::last_write_time(config_file_);
        if (current_modified <= last_modified_) {
            return true; // No changes
        }
        
        // Load new configuration
        nlohmann::json new_config;
        std::ifstream file(config_file_);
        if (!file.is_open()) {
            utils::Logger::error("Failed to open configuration file for reload: " + config_file_);
            return false;
        }
        
        file >> new_config;
        
        // Validate new configuration
        auto old_config = config_;
        config_ = new_config;
        
        if (!validateConfiguration()) {
            utils::Logger::error("New configuration failed validation, reverting");
            config_ = old_config;
            return false;
        }
        
        last_modified_ = current_modified;
        utils::Logger::info("Configuration reloaded successfully");
        
        // Notify all callbacks about changes
        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        for (const auto& [section, callback] : callbacks_) {
            if (new_config.contains(section)) {
                callback(section, new_config[section]);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        utils::Logger::error("Error reloading configuration: " + std::string(e.what()));
        return false;
    }
}

bool ConfigurationManager::saveConfiguration() {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    
    if (config_file_.empty()) {
        utils::Logger::error("No configuration file specified for save");
        return false;
    }
    
    try {
        std::ofstream file(config_file_);
        if (!file.is_open()) {
            utils::Logger::error("Failed to open configuration file for writing: " + config_file_);
            return false;
        }
        
        file << config_.dump(2);
        utils::Logger::info("Configuration saved successfully to: " + config_file_);
        return true;
    } catch (const std::exception& e) {
        utils::Logger::error("Error saving configuration: " + std::string(e.what()));
        return false;
    }
}

void ConfigurationManager::enableHotReload(bool enable) {
    if (enable && !hot_reload_enabled_) {
        hot_reload_enabled_ = true;
        running_ = true;
        hot_reload_thread_ = std::thread(&ConfigurationManager::hotReloadWorker, this);
        utils::Logger::info("Hot reload enabled with interval: " + 
                          std::to_string(reload_interval_.count()) + "ms");
    } else if (!enable && hot_reload_enabled_) {
        hot_reload_enabled_ = false;
        running_ = false;
        if (hot_reload_thread_.joinable()) {
            hot_reload_thread_.join();
        }
        utils::Logger::info("Hot reload disabled");
    }
}

void ConfigurationManager::setReloadInterval(std::chrono::milliseconds interval) {
    reload_interval_ = interval;
    utils::Logger::info("Hot reload interval set to: " + std::to_string(interval.count()) + "ms");
}

void ConfigurationManager::hotReloadWorker() {
    while (running_) {
        std::this_thread::sleep_for(reload_interval_);
        
        if (!running_) break;
        
        reloadConfiguration();
    }
}

bool ConfigurationManager::validateConfiguration() const {
    validation_errors_.clear();
    
    bool valid = true;
    
    // Validate exchange configuration
    if (!validateExchangeConfig()) {
        valid = false;
    }
    
    // Validate trading configuration  
    if (!validateTradingConfig()) {
        valid = false;
    }
    
    // Validate risk configuration
    if (!validateRiskConfig()) {
        valid = false;
    }
    
    // Validate performance configuration
    if (!validatePerformanceConfig()) {
        valid = false;
    }
    
    return valid;
}

bool ConfigurationManager::validateExchangeConfig() const {
    if (!config_.contains("exchanges")) {
        validation_errors_.push_back("Missing 'exchanges' section");
        return false;
    }
    
    const auto& exchanges = config_["exchanges"];
    std::vector<std::string> required_exchanges = {"binance", "okx", "bybit"};
    
    for (const auto& exchange : required_exchanges) {
        if (!exchanges.contains(exchange)) {
            validation_errors_.push_back("Missing exchange configuration: " + exchange);
            return false;
        }
        
        const auto& ex_config = exchanges[exchange];
        std::vector<std::string> required_fields = {
            "enabled", "websocket_url", "api_url", "reconnect_interval_ms"
        };
        
        for (const auto& field : required_fields) {
            if (!ex_config.contains(field)) {
                validation_errors_.push_back("Missing field '" + field + 
                                           "' in exchange '" + exchange + "'");
                return false;
            }
        }
    }
    
    return true;
}

bool ConfigurationManager::validateTradingConfig() const {
    if (!config_.contains("trading")) {
        validation_errors_.push_back("Missing 'trading' section");
        return false;
    }
    
    const auto& trading = config_["trading"];
    std::vector<std::string> required_fields = {
        "min_profit_threshold", "max_capital_per_trade", "execution_timeout_ms"
    };
    
    for (const auto& field : required_fields) {
        if (!trading.contains(field)) {
            validation_errors_.push_back("Missing trading field: " + field);
            return false;
        }
    }
    
    // Validate ranges
    if (trading["min_profit_threshold"].get<double>() < 0.0001) {
        validation_errors_.push_back("min_profit_threshold too low");
        return false;
    }
    
    if (trading["max_capital_per_trade"].get<double>() <= 0) {
        validation_errors_.push_back("max_capital_per_trade must be positive");
        return false;
    }
    
    return true;
}

bool ConfigurationManager::validateRiskConfig() const {
    if (!config_.contains("risk")) {
        validation_errors_.push_back("Missing 'risk' section");
        return false;
    }
    
    const auto& risk = config_["risk"];
    std::vector<std::string> required_fields = {
        "max_position_size", "max_portfolio_exposure", "stop_loss_percentage"
    };
    
    for (const auto& field : required_fields) {
        if (!risk.contains(field)) {
            validation_errors_.push_back("Missing risk field: " + field);
            return false;
        }
    }
    
    return true;
}

bool ConfigurationManager::validatePerformanceConfig() const {
    if (!config_.contains("performance")) {
        validation_errors_.push_back("Missing 'performance' section");
        return false;
    }
    
    const auto& performance = config_["performance"];
    std::vector<std::string> required_fields = {
        "detection_latency_ms", "target_throughput", "max_memory_usage_mb"
    };
    
    for (const auto& field : required_fields) {
        if (!performance.contains(field)) {
            validation_errors_.push_back("Missing performance field: " + field);
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> ConfigurationManager::getValidationErrors() const {
    return validation_errors_;
}

void ConfigurationManager::registerCallback(const std::string& section, ConfigChangeCallback callback) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);
    callbacks_[section] = callback;
    utils::Logger::debug("Registered configuration callback for section: " + section);
}

void ConfigurationManager::unregisterCallback(const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);
    callbacks_.erase(section);
    utils::Logger::debug("Unregistered configuration callback for section: " + section);
}

void ConfigurationManager::notifyConfigChange(const std::string& section, const nlohmann::json& new_value) {
    auto it = callbacks_.find(section);
    if (it != callbacks_.end()) {
        try {
            it->second(section, new_value);
        } catch (const std::exception& e) {
            utils::Logger::error("Error in configuration callback for section '" + 
                               section + "': " + e.what());
        }
    }
}

void ConfigurationManager::setEnvironment(const std::string& environment) {
    current_environment_ = environment;
    utils::Logger::info("Environment set to: " + environment);
}

std::string ConfigurationManager::getCurrentEnvironment() const {
    return current_environment_;
}

nlohmann::json ConfigurationManager::getConfigurationSchema() const {
    // Return the expected schema for validation
    return nlohmann::json{
        {"exchanges", {
            {"type", "object"},
            {"required", nlohmann::json::array({"binance", "okx", "bybit"})},
            {"properties", {
                {"binance", {{"type", "object"}}},
                {"okx", {{"type", "object"}}},
                {"bybit", {{"type", "object"}}}
            }}
        }},
        {"trading", {
            {"type", "object"},
            {"required", nlohmann::json::array({"min_profit_threshold", "max_capital_per_trade"})}
        }},
        {"risk", {
            {"type", "object"},
            {"required", nlohmann::json::array({"max_position_size", "max_portfolio_exposure"})}
        }},
        {"performance", {
            {"type", "object"},
            {"required", nlohmann::json::array({"detection_latency_ms", "target_throughput"})}
        }}
    };
}

nlohmann::json ConfigurationManager::getFullConfiguration() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return config_;
}

std::filesystem::file_time_type ConfigurationManager::getLastModified() const {
    return last_modified_;
}

} // namespace config
} // namespace arbitrage
