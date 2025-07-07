#include "ConfigManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>

using json = nlohmann::json;

namespace arbitrage {
namespace utils {

ConfigManager::ConfigManager(const std::string& config_file_path, bool enable_hot_reload)
    : config_file_path_(config_file_path)
    , enable_hot_reload_(enable_hot_reload)
    , should_stop_watching_(false)
    , config_data_(new json())
{
    // Load initial configuration
    if (!load()) {
        throw std::runtime_error("Failed to load configuration from: " + config_file_path_);
    }
    
    // Start hot reload thread if enabled
    if (enable_hot_reload_) {
        hot_reload_thread_ = std::thread(&ConfigManager::hotReloadWorker, this);
    }
}

ConfigManager::~ConfigManager() {
    should_stop_watching_ = true;
    if (hot_reload_thread_.joinable()) {
        hot_reload_thread_.join();
    }
    delete static_cast<json*>(config_data_);
}

bool ConfigManager::load() {
    try {
        std::ifstream file(config_file_path_);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open config file: " << config_file_path_ << std::endl;
            return false;
        }
        
        json new_config;
        file >> new_config;
        
        {
            std::unique_lock<std::mutex> lock(config_mutex_);
            *static_cast<json*>(config_data_) = std::move(new_config);
            last_modified_ = std::filesystem::last_write_time(config_file_path_);
        }
        
        std::cout << "Configuration loaded successfully from: " << config_file_path_ << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::reload() {
    std::cout << "Reloading configuration..." << std::endl;
    bool success = load();
    
    if (success) {
        // Notify callbacks of configuration change
        notifyCallbacks("config", "reloaded");
    }
    
    return success;
}

std::string ConfigManager::getString(const std::string& key, const std::string& default_value) const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    const json* value = static_cast<const json*>(getNestedValue(key));
    if (value && value->is_string()) {
        return value->get<std::string>();
    }
    
    return default_value;
}

int ConfigManager::getInt(const std::string& key, int default_value) const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    const json* value = static_cast<const json*>(getNestedValue(key));
    if (value && value->is_number_integer()) {
        return value->get<int>();
    }
    
    return default_value;
}

double ConfigManager::getDouble(const std::string& key, double default_value) const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    const json* value = static_cast<const json*>(getNestedValue(key));
    if (value && value->is_number()) {
        return value->get<double>();
    }
    
    return default_value;
}

bool ConfigManager::getBool(const std::string& key, bool default_value) const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    const json* value = static_cast<const json*>(getNestedValue(key));
    if (value && value->is_boolean()) {
        return value->get<bool>();
    }
    
    return default_value;
}

bool ConfigManager::hasKey(const std::string& key) const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    return getNestedValue(key) != nullptr;
}

void ConfigManager::registerChangeCallback(ConfigChangeCallback callback) {
    change_callbacks_.push_back(std::move(callback));
}

bool ConfigManager::validate() const {
    std::unique_lock<std::mutex> lock(config_mutex_);
    const json& config = *static_cast<const json*>(config_data_);
    
    // Basic validation - check for required keys
    std::vector<std::string> required_keys = {
        "exchanges.binance.enabled",
        "exchanges.okx.enabled", 
        "exchanges.bybit.enabled",
        "trading.min_profit_threshold",
        "trading.max_capital_per_trade",
        "risk.max_position_size",
        "performance.detection_latency_ms",
        "performance.target_throughput"
    };
    
    for (const auto& key : required_keys) {
        if (!hasKey(key)) {
            std::cerr << "Validation error: Missing required key: " << key << std::endl;
            return false;
        }
    }
    
    // Validate value ranges
    double min_profit = getDouble("trading.min_profit_threshold");
    if (min_profit <= 0.0 || min_profit > 1.0) {
        std::cerr << "Validation error: min_profit_threshold must be between 0 and 1" << std::endl;
        return false;
    }
    
    int max_capital = getInt("trading.max_capital_per_trade");
    if (max_capital <= 0) {
        std::cerr << "Validation error: max_capital_per_trade must be positive" << std::endl;
        return false;
    }
    
    return true;
}

std::filesystem::file_time_type ConfigManager::getLastModified() const {
    return last_modified_;
}

void ConfigManager::hotReloadWorker() {
    while (!should_stop_watching_) {
        try {
            if (std::filesystem::exists(config_file_path_)) {
                auto current_modified = std::filesystem::last_write_time(config_file_path_);
                
                if (current_modified != last_modified_) {
                    std::cout << "Configuration file changed, reloading..." << std::endl;
                    reload();
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in hot reload worker: " << e.what() << std::endl;
        }
        
        // Check every 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void ConfigManager::notifyCallbacks(const std::string& key, const std::string& value) {
    for (const auto& callback : change_callbacks_) {
        try {
            callback(key, value);
        } catch (const std::exception& e) {
            std::cerr << "Error in config change callback: " << e.what() << std::endl;
        }
    }
}

const void* ConfigManager::getNestedValue(const std::string& key) const {
    const json& config = *static_cast<const json*>(config_data_);
    
    // Split key by dots
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        keys.push_back(item);
    }
    
    const json* current = &config;
    for (const auto& k : keys) {
        if (current->contains(k)) {
            current = &(*current)[k];
        } else {
            return nullptr;
        }
    }
    
    return current;
}

} // namespace utils
} // namespace arbitrage
