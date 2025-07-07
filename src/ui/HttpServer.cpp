#include "HttpServer.hpp"
#include "../utils/Logger.hpp"
#include <thread>
#include <atomic>
#include <sstream>
#include <regex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace arbitrage {
namespace ui {

/**
 * @brief Simple HTTP server implementation using sockets
 * For production, consider using httplib, crow, or beast
 */
class HttpServer::Impl {
public:
    Impl(int port, HttpServer* parent) : port_(port), running_(false), parent_(parent) {}
    
    void start() {
        running_ = true;
        
        // Create socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            utils::Logger::error("Failed to create socket");
            return;
        }
        
        // Set socket options
        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind socket
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            utils::Logger::error("Failed to bind socket to port " + std::to_string(port_));
            close(server_socket_);
            return;
        }
        
        // Listen for connections
        if (listen(server_socket_, 10) < 0) {
            utils::Logger::error("Failed to listen on socket");
            close(server_socket_);
            return;
        }
        
        utils::Logger::info("HTTP Server listening on port " + std::to_string(port_));
        
        // Accept connections
        while (running_) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            
            int client_socket = accept(server_socket_, (struct sockaddr*)&client_address, &client_len);
            if (client_socket < 0) {
                if (running_) {
                    utils::Logger::error("Failed to accept connection");
                }
                continue;
            }
            
            // Handle request in separate thread
            std::thread([this, client_socket]() {
                handleConnection(client_socket);
            }).detach();
        }
    }
    
    void startAsync() {
        server_thread_ = std::thread([this]() { start(); });
    }
    
    void stop() {
        running_ = false;
        if (server_socket_ >= 0) {
            close(server_socket_);
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        utils::Logger::info("HTTP Server stopping");
    }
    
    bool isRunning() const { return running_; }
    
private:
    void handleConnection(int client_socket) {
        char buffer[4096];
        int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            close(client_socket);
            return;
        }
        
        buffer[bytes_read] = '\0';
        
        // Parse HTTP request
        HttpRequest request = parseRequest(std::string(buffer));
        
        // Handle request
        HttpResponse response = parent_->handleRequest(request);
        
        // Send response
        std::string http_response = formatResponse(response);
        send(client_socket, http_response.c_str(), http_response.length(), 0);
        
        close(client_socket);
    }
    
    HttpRequest parseRequest(const std::string& raw_request) {
        HttpRequest request;
        std::istringstream stream(raw_request);
        std::string line;
        
        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream request_line(line);
            std::string full_path;
            request_line >> request.method >> full_path;
            
            // Parse query parameters if present
            size_t query_pos = full_path.find('?');
            if (query_pos != std::string::npos) {
                request.path = full_path.substr(0, query_pos);
                std::string query_string = full_path.substr(query_pos + 1);
                
                // Parse query parameters
                std::istringstream query_stream(query_string);
                std::string param;
                while (std::getline(query_stream, param, '&')) {
                    size_t eq_pos = param.find('=');
                    if (eq_pos != std::string::npos) {
                        std::string key = param.substr(0, eq_pos);
                        std::string value = param.substr(eq_pos + 1);
                        request.query_params[key] = value;
                    }
                }
            } else {
                request.path = full_path;
            }
        }
        
        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t\r") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r") + 1);
                request.headers[key] = value;
            }
        }
        
        return request;
    }
    
    std::string formatResponse(const HttpResponse& response) {
        std::ostringstream stream;
        stream << "HTTP/1.1 " << response.status_code << " OK\r\n";
        
        // Add default headers
        stream << "Content-Type: " << (response.headers.count("Content-Type") ? 
                                      response.headers.at("Content-Type") : "text/html") << "\r\n";
        stream << "Content-Length: " << response.body.length() << "\r\n";
        stream << "Connection: close\r\n";
        
        // Add custom headers
        for (const auto& header : response.headers) {
            if (header.first != "Content-Type") {
                stream << header.first << ": " << header.second << "\r\n";
            }
        }
        
        stream << "\r\n" << response.body;
        return stream.str();
    }
    
    int port_;
    int server_socket_ = -1;
    std::atomic<bool> running_;
    std::thread server_thread_;
    HttpServer* parent_;
};

HttpServer::HttpServer(int port) : port_(port), cors_enabled_(false) {
    pimpl_ = std::make_unique<Impl>(port, this);
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    routes_[method][path] = std::move(handler);
    utils::Logger::debug("Added route: " + method + " " + path);
}

void HttpServer::start() {
    pimpl_->start();
}

void HttpServer::startAsync() {
    pimpl_->startAsync();
}

void HttpServer::stop() {
    pimpl_->stop();
}

bool HttpServer::isRunning() const {
    return pimpl_->isRunning();
}

void HttpServer::enableCORS(bool enable) {
    cors_enabled_ = enable;
}

HttpResponse HttpServer::handleRequest(const HttpRequest& request) {
    // Handle CORS preflight
    if (cors_enabled_ && request.method == "OPTIONS") {
        return createCORSResponse();
    }
    
    // Find route handler
    auto method_it = routes_.find(request.method);
    if (method_it != routes_.end()) {
        auto path_it = method_it->second.find(request.path);
        if (path_it != method_it->second.end()) {
            auto response = path_it->second(request);
            
            // Add CORS headers if enabled
            if (cors_enabled_) {
                response.headers["Access-Control-Allow-Origin"] = "*";
                response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
                response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
            }
            
            return response;
        }
    }
    
    // Route not found
    HttpResponse response;
    response.status_code = 404;
    response.body = R"({"error": "Not Found"})";
    return response;
}

HttpResponse HttpServer::createCORSResponse() const {
    HttpResponse response;
    response.status_code = 200;
    response.body = "";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    return response;
}

} // namespace ui
} // namespace arbitrage
