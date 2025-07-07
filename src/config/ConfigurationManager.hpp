#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>
#include <shared_mutex>
#include <vector>
#include <filesystem>

namespace arbitrage {
namespace config {

// Forward declaration of helper function
std::vector<std::string> splitKey(const std::string& key);

class ConfigurationManager {
public:
    using ConfigChangeCallback = std::function<void(const std::string&, const nlohmann::json&)>;
    
    static ConfigurationManager& getInstance();
    
    // Core configuration methods
    bool loadConfiguration(const std::string& config_file);
    bool reloadConfiguration();
    bool saveConfiguration();
    
    // Hot reload functionality
    void enableHotReload(bool enable = true);
    void setReloadInterval(std::chrono::milliseconds interval);
    
    // Configuration access
    template<typename T>
    T getValue(const std::string& key, const T& default_value = T{}) const;
    
    template<typename T>
    bool setValue(const std::string& key, const T& value);
    
    // Configuration validation
    bool validateConfiguration() const;
    std::vector<std::string> getValidationErrors() const;
    
    // Change notifications
    void registerCallback(const std::string& section, ConfigChangeCallback callback);
    void unregisterCallback(const std::string& section);
    
    // Environment-specific configurations
    void setEnvironment(const std::string& environment);
    std::string getCurrentEnvironment() const;
    
    // Configuration metadata
    nlohmann::json getConfigurationSchema() const;
    nlohmann::json getFullConfiguration() const;
    std::filesystem::file_time_type getLastModified() const;
    
private:
    ConfigurationManager();
    ~ConfigurationManager();
    
    // Hot reload thread
    void hotReloadWorker();
    void notifyConfigChange(const std::string& section, const nlohmann::json& new_value);
    
    // Configuration validation helpers
    bool validateExchangeConfig() const;
    bool validateTradingConfig() const;
    bool validateRiskConfig() const;
    bool validatePerformanceConfig() const;
    
    nlohmann::json config_;
    std::string config_file_;
    std::string current_environment_;
    
    mutable std::shared_mutex config_mutex_;
    std::unordered_map<std::string, ConfigChangeCallback> callbacks_;
    
    std::atomic<bool> hot_reload_enabled_;
    std::chrono::milliseconds reload_interval_;
    std::thread hot_reload_thread_;
    std::atomic<bool> running_;
    
    std::filesystem::file_time_type last_modified_;
    mutable std::vector<std::string> validation_errors_;
};

// Template implementations
template<typename T>
T ConfigurationManager::getValue(const std::string& key, const T& default_value) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    
    try {
        // Support nested keys like "trading.min_profit_threshold"
        auto keys = splitKey(key);
        nlohmann::json current = config_;
        
        for (const auto& k : keys) {
            if (current.contains(k)) {
                current = current[k];
            } else {
                return default_value;
            }
        }
        
        return current.get<T>();
    } catch (const std::exception&) {
        return default_value;
    }
}

template<typename T>
bool ConfigurationManager::setValue(const std::string& key, const T& value) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);
    
    try {
        auto keys = splitKey(key);
        nlohmann::json* current = &config_;
        
        // Navigate to the parent object
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                (*current)[keys[i]] = nlohmann::json::object();
            }
            current = &(*current)[keys[i]];
        }
        
        // Set the value
        (*current)[keys.back()] = value;
        
        // Notify callbacks
        auto section = keys[0]; // First part is typically the section
        notifyConfigChange(section, (*current));
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace config
} // namespace arbitrage
