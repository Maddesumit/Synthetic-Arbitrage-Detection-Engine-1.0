#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <queue>
#include <nlohmann/json.hpp>

namespace arbitrage {
namespace api {

// Forward declarations
class HttpServer;
class WebSocketServer;
class AuthenticationManager;
class RateLimiter;
class DataProvider;
class RiskManager;
class TradeEngine;

// Request structures for compatibility
struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::string body;
};

// WebSocket connection structure
struct WebSocketConnection {
    std::string id;
    bool active = false;
    std::chrono::system_clock::time_point connected_at;
    std::vector<std::string> message_queue;
};

// API request/response structures
struct ApiRequest {
    std::string method;           // GET, POST, PUT, DELETE
    std::string path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::string body;
    std::string client_ip;
    std::chrono::system_clock::time_point timestamp;
    std::string user_id;          // Set after authentication
    std::vector<std::string> roles; // Set after authentication
};

struct ApiResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds processing_time{0};
};

// WebSocket message structures
struct WebSocketMessage {
    std::string connection_id;
    std::string message_type;
    nlohmann::json payload;
    std::chrono::system_clock::time_point timestamp;
};

struct WebSocketSubscription {
    std::string connection_id;
    std::string channel;
    nlohmann::json filters;
    std::chrono::system_clock::time_point subscribed_at;
};

// API endpoint handler type
using ApiHandler = std::function<ApiResponse(const ApiRequest&)>;
using WebSocketHandler = std::function<void(const WebSocketMessage&)>;

class ApiServer {
public:
    ApiServer(int http_port = 8080, int websocket_port = 8081);
    ~ApiServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const;
    
    // HTTP endpoint registration
    void registerEndpoint(const std::string& method, const std::string& path, ApiHandler handler);
    void registerMiddleware(std::function<bool(ApiRequest&, ApiResponse&)> middleware);
    
    // WebSocket channel registration
    void registerWebSocketChannel(const std::string& channel, WebSocketHandler handler);
    void broadcastToChannel(const std::string& channel, const nlohmann::json& message);
    void sendToConnection(const std::string& connection_id, const nlohmann::json& message);
    
    // Authentication and security
    void setAuthenticationManager(std::shared_ptr<AuthenticationManager> auth_manager);
    void enableRateLimit(std::shared_ptr<RateLimiter> rate_limiter);
    void enableCORS(bool enable = true, const std::vector<std::string>& allowed_origins = {"*"});
    
    // Monitoring and metrics
    struct ServerMetrics {
        std::atomic<size_t> total_requests{0};
        std::atomic<size_t> successful_requests{0};
        std::atomic<size_t> failed_requests{0};
        std::atomic<size_t> active_connections{0};
        std::atomic<size_t> total_websocket_messages{0};
        std::chrono::system_clock::time_point server_start_time;
        std::chrono::milliseconds average_response_time{0};
    };
    
    struct ServerMetricsSnapshot {
        size_t total_requests{0};
        size_t successful_requests{0};
        size_t failed_requests{0};
        size_t active_connections{0};
        size_t total_websocket_messages{0};
        std::chrono::system_clock::time_point server_start_time;
        std::chrono::milliseconds average_response_time{0};
    };
    
    ServerMetrics& getMetrics() const;
    ServerMetricsSnapshot getMetricsSnapshot() const;
    void resetMetrics();
    
    // Configuration
    void setMaxRequestSize(size_t max_size);
    void setRequestTimeout(std::chrono::seconds timeout);
    void enableRequestLogging(bool enable = true);
    
private:
    std::unique_ptr<HttpServer> http_server_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    std::shared_ptr<AuthenticationManager> auth_manager_;
    std::shared_ptr<RateLimiter> rate_limiter_;
    
    int http_port_;
    int websocket_port_;
    std::atomic<bool> running_;
    
    // Endpoint management
    std::unordered_map<std::string, std::unordered_map<std::string, ApiHandler>> endpoints_;
    std::vector<std::function<bool(ApiRequest&, ApiResponse&)>> middlewares_;
    
    // WebSocket management
    std::unordered_map<std::string, WebSocketHandler> websocket_channels_;
    std::unordered_map<std::string, std::vector<WebSocketSubscription>> channel_subscriptions_;
    std::mutex websocket_mutex_;
    
    // Configuration
    size_t max_request_size_ = 1024 * 1024; // 1MB
    std::chrono::seconds request_timeout_{30};
    bool request_logging_enabled_ = true;
    bool cors_enabled_ = false;
    std::vector<std::string> cors_allowed_origins_;
    
    // Metrics
    mutable ServerMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Internal handlers
    ApiResponse handleRequest(const ApiRequest& request);
    void handleWebSocketMessage(const WebSocketMessage& message);
    bool authenticateRequest(ApiRequest& request, ApiResponse& response);
    bool checkRateLimit(const ApiRequest& request, ApiResponse& response);
    void logRequest(const ApiRequest& request, const ApiResponse& response);
    
    // CORS handling
    void addCORSHeaders(ApiResponse& response, const ApiRequest& request);
    ApiResponse handlePreflight(const ApiRequest& request);
};

// REST API Endpoints Implementation
class RestApiEndpoints {
public:
    RestApiEndpoints(std::shared_ptr<class ArbitrageEngine> engine);
    
    void registerEndpoints(ApiServer& server);
    
private:
    std::shared_ptr<class ArbitrageEngine> engine_;
    
    // V1 API endpoints
    ApiResponse getOpportunities(const ApiRequest& request);
    ApiResponse getPositions(const ApiRequest& request);
    ApiResponse getRiskMetrics(const ApiRequest& request);
    ApiResponse getPerformanceMetrics(const ApiRequest& request);
    ApiResponse getConfiguration(const ApiRequest& request);
    ApiResponse updateConfiguration(const ApiRequest& request);
    ApiResponse getSystemHealth(const ApiRequest& request);
    
    // Historical data endpoints
    ApiResponse getHistoricalData(const ApiRequest& request);
    ApiResponse getAnalytics(const ApiRequest& request);
    ApiResponse exportData(const ApiRequest& request);
    
    // Control endpoints
    ApiResponse startSystem(const ApiRequest& request);
    ApiResponse stopSystem(const ApiRequest& request);
    ApiResponse resetSystem(const ApiRequest& request);
    
    // Helper methods
    nlohmann::json parseQueryParameters(const std::string& query_string);
    ApiResponse createJsonResponse(const nlohmann::json& data, int status_code = 200);
    ApiResponse createErrorResponse(const std::string& error, int status_code = 400);
    
    // Validation helpers
    bool validateTimeRange(const std::string& start_time, const std::string& end_time);
    bool validateExchange(const std::string& exchange);
    bool validateSymbol(const std::string& symbol);
};

// WebSocket Streaming API
class WebSocketStreamingApi {
public:
    WebSocketStreamingApi(std::shared_ptr<class ArbitrageEngine> engine);
    
    void registerChannels(ApiServer& server);
    
    // Channel handlers
    void handleOpportunityFeed(const WebSocketMessage& message);
    void handlePositionUpdates(const WebSocketMessage& message);
    void handleRiskAlerts(const WebSocketMessage& message);
    void handleSystemStatus(const WebSocketMessage& message);
    void handleMarketData(const WebSocketMessage& message);
    
    // Subscription management
    void subscribeToOpportunities(const std::string& connection_id, const nlohmann::json& filters);
    void subscribeToPositions(const std::string& connection_id, const nlohmann::json& filters);
    void subscribeToRiskAlerts(const std::string& connection_id, const nlohmann::json& filters);
    
    // Data broadcasting
    void broadcastOpportunity(const struct ArbitrageOpportunity& opportunity);
    void broadcastPositionUpdate(const struct Position& position);
    void broadcastRiskAlert(const struct RiskAlert& alert);
    void broadcastSystemStatus(const nlohmann::json& status);
    
private:
    std::shared_ptr<class ArbitrageEngine> engine_;
    ApiServer* server_ = nullptr;
    
    // Subscription tracking
    struct SubscriptionFilter {
        std::vector<std::string> exchanges;
        std::vector<std::string> symbols;
        double min_profit_threshold = 0.0;
        double max_risk_score = 1.0;
    };
    
    std::unordered_map<std::string, SubscriptionFilter> opportunity_subscriptions_;
    std::unordered_map<std::string, SubscriptionFilter> position_subscriptions_;
    std::unordered_map<std::string, SubscriptionFilter> risk_subscriptions_;
    std::mutex subscription_mutex_;
    
    // Message formatting
    nlohmann::json formatOpportunity(const struct ArbitrageOpportunity& opportunity);
    nlohmann::json formatPosition(const struct Position& position);
    nlohmann::json formatRiskAlert(const struct RiskAlert& alert);
    
    // Filter checking
    bool matchesFilter(const struct ArbitrageOpportunity& opportunity, 
                      const SubscriptionFilter& filter);
    bool matchesFilter(const struct Position& position,
                      const SubscriptionFilter& filter);
};

// Authentication and authorization
class AuthenticationManager {
public:
    struct User {
        std::string user_id;
        std::string username;
        std::string email;
        std::vector<std::string> roles;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point last_login;
        bool is_active = true;
    };
    
    struct ApiKey {
        std::string key_id;
        std::string key_hash;
        std::string user_id;
        std::vector<std::string> permissions;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point expires_at;
        bool is_active = true;
    };
    
    AuthenticationManager();
    
    // User management
    bool createUser(const std::string& username, const std::string& email,
                   const std::vector<std::string>& roles);
    bool updateUser(const std::string& user_id, const User& updated_user);
    bool deleteUser(const std::string& user_id);
    std::vector<User> getAllUsers() const;
    
    // API key management
    std::string generateApiKey(const std::string& user_id,
                             const std::vector<std::string>& permissions,
                             std::chrono::hours validity_period = std::chrono::hours(24 * 30));
    bool revokeApiKey(const std::string& key_id);
    std::vector<ApiKey> getUserApiKeys(const std::string& user_id) const;
    
    // Authentication
    bool authenticateApiKey(const std::string& api_key, User& user) const;
    bool authenticateBasicAuth(const std::string& username, const std::string& password, User& user) const;
    
    // Authorization
    bool hasPermission(const User& user, const std::string& permission) const;
    bool hasRole(const User& user, const std::string& role) const;
    
    // Session management
    std::string createSession(const std::string& user_id);
    bool validateSession(const std::string& session_token, User& user) const;
    void destroySession(const std::string& session_token);
    
private:
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, ApiKey> api_keys_;
    std::unordered_map<std::string, std::string> sessions_; // token -> user_id
    mutable std::shared_mutex auth_mutex_;
    
    // Helper methods
    std::string generateKeyId() const;
    std::string hashApiKey(const std::string& key) const;
    std::string generateSessionToken() const;
    bool isKeyExpired(const ApiKey& key) const;
};

// Rate limiting
class RateLimiter {
public:
    struct RateLimit {
        size_t requests_per_minute = 60;
        size_t requests_per_hour = 1000;
        size_t requests_per_day = 10000;
    };
    
    struct ClientUsage {
        std::chrono::system_clock::time_point window_start;
        size_t requests_this_minute = 0;
        size_t requests_this_hour = 0;
        size_t requests_this_day = 0;
    };
    
    RateLimiter();
    
    // Rate limit configuration
    void setGlobalRateLimit(const RateLimit& limit);
    void setUserRateLimit(const std::string& user_id, const RateLimit& limit);
    void setIpRateLimit(const std::string& ip_address, const RateLimit& limit);
    
    // Rate limit checking
    bool checkRateLimit(const std::string& client_id, const std::string& ip_address);
    RateLimit getRemainingRequests(const std::string& client_id) const;
    
    // Configuration
    void setCleanupInterval(std::chrono::minutes interval);
    
private:
    RateLimit global_rate_limit_;
    std::unordered_map<std::string, RateLimit> user_rate_limits_;
    std::unordered_map<std::string, RateLimit> ip_rate_limits_;
    
    std::unordered_map<std::string, ClientUsage> client_usage_;
    mutable std::mutex usage_mutex_;
    
    std::chrono::minutes cleanup_interval_{10};
    std::thread cleanup_thread_;
    std::atomic<bool> running_{false};
    
    // Helper methods
    void cleanupExpiredEntries();
    void updateUsage(const std::string& client_id);
    bool isWindowExpired(const ClientUsage& usage) const;
};

// Integration connectors
class IntegrationManager {
public:
    struct ExternalSystem {
        std::string system_id;
        std::string name;
        std::string endpoint_url;
        std::string auth_type; // "api_key", "basic", "oauth", "none"
        std::unordered_map<std::string, std::string> auth_credentials;
        bool is_active = true;
        std::chrono::seconds timeout{30};
    };
    
    IntegrationManager();
    
    // System registration
    bool registerExternalSystem(const ExternalSystem& system);
    bool updateExternalSystem(const std::string& system_id, const ExternalSystem& system);
    bool removeExternalSystem(const std::string& system_id);
    
    // Data pushing
    bool pushOpportunityData(const std::string& system_id, const nlohmann::json& data);
    bool pushRiskData(const std::string& system_id, const nlohmann::json& data);
    bool pushPerformanceData(const std::string& system_id, const nlohmann::json& data);
    
    // Health monitoring
    struct SystemHealth {
        std::string system_id;
        bool is_connected;
        std::chrono::system_clock::time_point last_successful_call;
        std::chrono::system_clock::time_point last_failed_call;
        size_t consecutive_failures = 0;
        std::chrono::milliseconds average_response_time{0};
    };
    
    std::vector<SystemHealth> getSystemHealth() const;
    bool testConnection(const std::string& system_id);
    
private:
    std::unordered_map<std::string, ExternalSystem> external_systems_;
    std::unordered_map<std::string, SystemHealth> system_health_;
    mutable std::shared_mutex systems_mutex_;
    
    // HTTP client functionality
    bool makeHttpRequest(const std::string& method, const std::string& url,
                        const std::unordered_map<std::string, std::string>& headers,
                        const std::string& body, std::string& response);
    
    std::unordered_map<std::string, std::string> buildAuthHeaders(const ExternalSystem& system);
};

// Legacy ApiFramework class for backward compatibility
class ApiFramework {
public:
    ApiFramework(int port = 8080);
    ~ApiFramework();

    // Server lifecycle
    bool start();
    void stop();

    // Component registration
    void registerDataProvider(std::shared_ptr<DataProvider> provider);
    void registerRiskManager(std::shared_ptr<RiskManager> risk_manager);
    void registerTradeEngine(std::shared_ptr<TradeEngine> trade_engine);

    // Request handling
    std::string handleHttpRequest(const HttpRequest& request);

    // WebSocket functionality
    void broadcastWebSocketMessage(const std::string& message);
    bool addWebSocketConnection(const std::string& connection_id);
    void removeWebSocketConnection(const std::string& connection_id);

private:
    int port_;
    std::atomic<bool> running_;
    
    // Registered components
    std::shared_ptr<DataProvider> data_provider_;
    std::shared_ptr<RiskManager> risk_manager_;
    std::shared_ptr<TradeEngine> trade_engine_;
    
    // Thread management
    std::thread server_thread_;
    std::thread websocket_thread_;
    
    // Endpoint management
    std::unordered_map<std::string, std::function<std::string(const HttpRequest&)>> endpoints_;
    mutable std::mutex endpoint_mutex_;
    mutable std::mutex data_mutex_;
    mutable std::mutex websocket_mutex_;
    
    // WebSocket connections
    std::unordered_map<std::string, WebSocketConnection> websocket_connections_;
    std::queue<std::string> websocket_message_queue_;
    
    // Internal methods
    void initializeEndpoints();
    void serverLoop();
    void processWebSocketMessages();
    std::string handleGetMarketData(const HttpRequest& request);
    std::string handleGetSymbolData(const HttpRequest& request);
    std::string handleCreateTrade(const HttpRequest& request);
    std::string handleGetTrades(const HttpRequest& request);
    std::string handleGetTrade(const HttpRequest& request);
    std::string handleGetPositions(const HttpRequest& request);
    std::string handleClosePosition(const HttpRequest& request);
    std::string handleGetRiskMetrics(const HttpRequest& request);
    std::string handleStressTest(const HttpRequest& request);
    std::string handleGetPerformanceMetrics(const HttpRequest& request);
    std::string handleGetArbitrageOpportunities(const HttpRequest& request);
    std::string handleGetSystemStatus(const HttpRequest& request);
    std::string handleUpdateConfig(const HttpRequest& request);
    std::string createErrorResponse(int status_code, const std::string& message);
    std::string createSuccessResponse(const nlohmann::json& data);
    std::string getCurrentTimestamp();
    
    // Helper functions
    std::string getQueryParameter(const HttpRequest& request, const std::string& param);
    std::string extractPathParameter(const std::string& path, const std::string& placeholder);
};

} // namespace api
} // namespace arbitrage
