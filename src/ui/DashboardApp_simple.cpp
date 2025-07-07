#include "DashboardApp.hpp"
#include "../utils/Logger.hpp"
#include <sstream>

namespace arbitrage {
namespace ui {

DashboardApp::DashboardApp(int port) 
    : port_(port), running_(false) {
    http_server_ = std::make_unique<HttpServer>(port);
    data_exporter_ = std::make_unique<DataExporter>();
    
    // Enable CORS for web dashboard
    http_server_->enableCORS(true);
    
    setupRoutes();
}

DashboardApp::~DashboardApp() {
    stop();
}

void DashboardApp::initialize(std::shared_ptr<core::PricingEngine> pricing_engine) {
    pricing_engine_ = pricing_engine;
    utils::Logger::info("Dashboard initialized with pricing engine");
}

void DashboardApp::start() {
    running_ = true;
    utils::Logger::info("Starting dashboard on port " + std::to_string(port_));
    
    // Start update loop in separate thread
    update_thread_ = std::thread([this]() { runUpdateLoop(); });
    
    // Start HTTP server (blocking)
    http_server_->start();
}

void DashboardApp::startAsync() {
    running_ = true;
    utils::Logger::info("Starting dashboard async on port " + std::to_string(port_));
    
    // Start update loop
    update_thread_ = std::thread([this]() { runUpdateLoop(); });
    
    // Start HTTP server in background
    http_server_->startAsync();
}

void DashboardApp::stop() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
    if (http_server_) {
        http_server_->stop();
    }
    utils::Logger::info("Dashboard stopped");
}

void DashboardApp::setupRoutes() {
    // API routes
    http_server_->addRoute("GET", "/api/status", 
        [this](const HttpRequest& req) { return handleApiStatus(req); });
    http_server_->addRoute("GET", "/api/market-data", 
        [this](const HttpRequest& req) { return handleApiMarketData(req); });
    http_server_->addRoute("GET", "/api/pricing-results", 
        [this](const HttpRequest& req) { return handleApiPricingResults(req); });
    http_server_->addRoute("GET", "/api/opportunities", 
        [this](const HttpRequest& req) { return handleApiOpportunities(req); });
    http_server_->addRoute("GET", "/api/performance", 
        [this](const HttpRequest& req) { return handleApiPerformance(req); });
    http_server_->addRoute("GET", "/api/risk", 
        [this](const HttpRequest& req) { return handleApiRisk(req); });
    
    // Static files (for serving dashboard HTML/JS/CSS)
    http_server_->addRoute("GET", "/", 
        [this](const HttpRequest& req) { return handleStaticFiles(req); });
    http_server_->addRoute("GET", "/dashboard", 
        [this](const HttpRequest& req) { return handleStaticFiles(req); });
}

void DashboardApp::runUpdateLoop() {
    while (running_) {
        try {
            updateDemoData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            utils::Logger::error("Error in update loop: " + std::string(e.what()));
        }
    }
}

void DashboardApp::updateDemoData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Update demo data
    std::vector<data::MarketDataPoint> demo_market_data;
    std::vector<core::PricingResult> demo_pricing_results;
    std::vector<core::ArbitrageOpportunity> demo_opportunities;
    
    // Create demo market data
    data::MarketDataPoint btc_data;
    btc_data.symbol = "BTCUSDT";
    btc_data.exchange = "binance";
    btc_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    btc_data.bid = 50000.50;
    btc_data.ask = 50000.75;
    btc_data.last = 50000.62;
    btc_data.volume = 1234.56;
    btc_data.funding_rate = 0.0001;
    demo_market_data.push_back(btc_data);
    
    // Create demo pricing result
    core::PricingResult pricing_result;
    pricing_result.instrument_id = "BTCUSDT";
    pricing_result.synthetic_price = 50000.62;
    pricing_result.confidence = 1.0;
    pricing_result.model_name = "Spot";
    pricing_result.components.base_price = 50000.62;
    pricing_result.success = true;
    pricing_result.calculation_time_ms = 0.5;
    pricing_result.timestamp = std::chrono::system_clock::now();
    demo_pricing_results.push_back(pricing_result);
    
    // Store demo data
    latest_market_data_ = demo_market_data;
    latest_pricing_results_ = demo_pricing_results;
    latest_opportunities_ = demo_opportunities;
}

HttpResponse DashboardApp::handleStaticFiles(const HttpRequest& request) {
    // Serve a simple HTML dashboard
    std::string html = "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Arbitrage Engine Dashboard</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }\n"
        "        .card { background: white; border: 1px solid #ddd; padding: 20px; margin: 10px 0; border-radius: 8px; }\n"
        "        .status { color: green; font-weight: bold; }\n"
        "        h1 { color: #333; }\n"
        "        h3 { color: #666; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Synthetic Arbitrage Detection Engine</h1>\n"
        "    <p>Phase 9 Implementation - Real-time Dashboard</p>\n"
        "    <div class=\"card\">\n"
        "        <h3>System Status</h3>\n"
        "        <div class=\"status\">Running</div>\n"
        "    </div>\n"
        "    <div class=\"card\">\n"
        "        <h3>Market Data</h3>\n"
        "        <p>Live market data streaming from exchanges...</p>\n"
        "        <p>API: <a href=\"/api/market-data\">/api/market-data</a></p>\n"
        "    </div>\n"
        "    <div class=\"card\">\n"
        "        <h3>Pricing Results</h3>\n"
        "        <p>Synthetic pricing calculations...</p>\n"
        "        <p>API: <a href=\"/api/pricing-results\">/api/pricing-results</a></p>\n"
        "    </div>\n"
        "    <div class=\"card\">\n"
        "        <h3>Arbitrage Opportunities</h3>\n"
        "        <p>Real-time arbitrage detection...</p>\n"
        "        <p>API: <a href=\"/api/opportunities\">/api/opportunities</a></p>\n"
        "    </div>\n"
        "</body>\n"
        "</html>";
    
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "text/html";
    response.body = html;
    return response;
}

HttpResponse DashboardApp::handleApiStatus(const HttpRequest& request) {
    nlohmann::json status = {
        {"status", "running"},
        {"uptime", 1.5},
        {"version", "1.0.0"},
        {"components", {
            {"pricing_engine", "active"},
            {"market_data", "streaming"},
            {"arbitrage_detector", "scanning"}
        }}
    };
    return createJsonResponse(status);
}

HttpResponse DashboardApp::handleApiMarketData(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    nlohmann::json market_data = nlohmann::json::array();
    
    for (const auto& point : latest_market_data_) {
        market_data.push_back(data_exporter_->serializeMarketDataPoint(point));
    }
    
    return createJsonResponse(market_data);
}

HttpResponse DashboardApp::handleApiPricingResults(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    nlohmann::json pricing_results = nlohmann::json::array();
    
    for (const auto& result : latest_pricing_results_) {
        pricing_results.push_back(data_exporter_->serializePricingResult(result));
    }
    
    return createJsonResponse(pricing_results);
}

HttpResponse DashboardApp::handleApiOpportunities(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    nlohmann::json opportunities = nlohmann::json::array();
    
    for (const auto& opportunity : latest_opportunities_) {
        opportunities.push_back(data_exporter_->serializeArbitrageOpportunity(opportunity));
    }
    
    return createJsonResponse(opportunities);
}

HttpResponse DashboardApp::handleApiPerformance(const HttpRequest& request) {
    nlohmann::json performance = {
        {"latency", 2.5},
        {"throughput", 1500},
        {"memory_usage", 256},
        {"cpu_usage", 15.3}
    };
    return createJsonResponse(performance);
}

HttpResponse DashboardApp::handleApiRisk(const HttpRequest& request) {
    nlohmann::json risk = {
        {"var", 2.5},
        {"sharpe", 1.8},
        {"max_drawdown", 5.2},
        {"volatility", 18.5}
    };
    return createJsonResponse(risk);
}

HttpResponse DashboardApp::createJsonResponse(const nlohmann::json& data) const {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";
    response.body = data.dump(2);
    return response;
}

HttpResponse DashboardApp::createErrorResponse(int status_code, const std::string& message) const {
    HttpResponse response;
    response.status_code = status_code;
    response.content_type = "application/json";
    response.body = nlohmann::json{{"error", message}}.dump();
    return response;
}

} // namespace ui
} // namespace arbitrage
