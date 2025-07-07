#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

namespace arbitrage {
namespace ui {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;
    std::map<std::string, std::string> headers;
};

/**
 * @brief HTTP request structure
 */
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::string body;
};

/**
 * @brief HTTP route handler function type
 */
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief Simple HTTP server for data export and API endpoints
 */
class HttpServer {
public:
    HttpServer(int port = 8080);
    ~HttpServer();

    /**
     * @brief Add a route handler
     */
    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);

    /**
     * @brief Start the server (blocking)
     */
    void start();

    /**
     * @brief Start the server in background thread
     */
    void startAsync();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const;

    /**
     * @brief Enable CORS for web dashboard
     */
    void enableCORS(bool enable = true);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    int port_;
    bool cors_enabled_;
    std::map<std::string, std::map<std::string, RouteHandler>> routes_;
    
    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse createCORSResponse() const;
};

} // namespace ui
} // namespace arbitrage
