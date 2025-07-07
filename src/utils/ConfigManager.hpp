#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <mutex>

namespace arbitrage {
namespace utils {

/**
 * @brief Configuration manager that supports JSON/YAML loading with hot reload capability
 * 
 * This class provides thread-safe configuration management with the ability to:
 * - Load configuration from JSON/YAML files
 * - Hot reload configuration changes without restart
 * - Validate configuration parameters
 * - Notify components of configuration changes via callbacks
 */
class ConfigManager {
public:
    using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& value)>;
    
    /**
     * @brief Constructor
     * @param config_file_path Path to the configuration file
     * @param enable_hot_reload Enable automatic file watching and reloading
     */
    explicit ConfigManager(const std::string& config_file_path, bool enable_hot_reload = true);
    
    /**
     * @brief Destructor - stops hot reload thread if running
     */
    ~ConfigManager();
    
    /**
     * @brief Load configuration from file
     * @return true if successful, false otherwise
     */
    bool load();
    
    /**
     * @brief Reload configuration from file
     * @return true if successful, false otherwise
     */
    bool reload();
    
    /**
     * @brief Get string value from configuration
     * @param key Configuration key (supports nested keys with dot notation, e.g., "exchange.binance.api_key")
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const;
    
    /**
     * @brief Get integer value from configuration
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    int getInt(const std::string& key, int default_value = 0) const;
    
    /**
     * @brief Get double value from configuration
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    double getDouble(const std::string& key, double default_value = 0.0) const;
    
    /**
     * @brief Get boolean value from configuration
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    bool getBool(const std::string& key, bool default_value = false) const;
    
    /**
     * @brief Check if configuration key exists
     * @param key Configuration key
     * @return true if key exists, false otherwise
     */
    bool hasKey(const std::string& key) const;
    
    /**
     * @brief Register callback for configuration changes
     * @param callback Function to call when configuration changes
     */
    void registerChangeCallback(ConfigChangeCallback callback);
    
    /**
     * @brief Validate configuration against schema
     * @return true if configuration is valid, false otherwise
     */
    bool validate() const;
    
    /**
     * @brief Get last modification time of config file
     * @return Last write time of the configuration file
     */
    std::filesystem::file_time_type getLastModified() const;

private:
    std::string config_file_path_;
    bool enable_hot_reload_;
    std::atomic<bool> should_stop_watching_;
    std::thread hot_reload_thread_;
    
    mutable std::mutex config_mutex_;
    void* config_data_; // Will hold nlohmann::json* to avoid header dependency
    
    std::vector<ConfigChangeCallback> change_callbacks_;
    std::filesystem::file_time_type last_modified_;
    
    /**
     * @brief Hot reload worker thread function
     */
    void hotReloadWorker();
    
    /**
     * @brief Notify all registered callbacks of configuration change
     * @param key Changed configuration key
     * @param value New value
     */
    void notifyCallbacks(const std::string& key, const std::string& value);
    
    /**
     * @brief Parse nested key (e.g., "exchange.binance.api_key")
     * @param key Dot-separated configuration key
     * @return Pointer to the JSON value or nullptr if not found
     */
    const void* getNestedValue(const std::string& key) const;
};

} // namespace utils
} // namespace arbitrage
