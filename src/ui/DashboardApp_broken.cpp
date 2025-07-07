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
    if (running_) {
        running_ = false;
        http_server_->stop();
        
        if (update_thread_.joinable()) {
            update_thread_.join();
        }
        
        utils::Logger::info("Dashboard stopped");
    }
}

bool DashboardApp::isRunning() const {
    return running_ && http_server_->isRunning();
}

void DashboardApp::updateMarketData(const std::vector<data::MarketDataPoint>& data) {
    data_exporter_->updateMarketData(data);
}

void DashboardApp::updatePricingResults(const std::vector<core::PricingResult>& results) {
    data_exporter_->updatePricingResults(results);
}

void DashboardApp::updateArbitrageOpportunities(const std::vector<core::ArbitrageOpportunity>& opportunities) {
    data_exporter_->updateArbitrageOpportunities(opportunities);
}

std::string DashboardApp::getDashboardUrl() const {
    return "http://localhost:" + std::to_string(port_);
}

void DashboardApp::setupRoutes() {
    // Health check endpoint
    http_server_->addRoute("GET", "/health", 
        [this](const HttpRequest& req) { return handleHealthCheck(req); });
    
    // API endpoints
    http_server_->addRoute("GET", "/api/status", 
        [this](const HttpRequest& req) { return handleSystemStatus(req); });
    
    http_server_->addRoute("GET", "/api/market-data", 
        [this](const HttpRequest& req) { return handleMarketData(req); });
    
    http_server_->addRoute("GET", "/api/pricing-results", 
        [this](const HttpRequest& req) { return handlePricingResults(req); });
    
    http_server_->addRoute("GET", "/api/arbitrage-opportunities", 
        [this](const HttpRequest& req) { return handleArbitrageOpportunities(req); });
    
    http_server_->addRoute("GET", "/api/performance-metrics", 
        [this](const HttpRequest& req) { return handlePerformanceMetrics(req); });
    
    http_server_->addRoute("GET", "/api/risk-metrics", 
        [this](const HttpRequest& req) { return handleRiskMetrics(req); });
    
    http_server_->addRoute("GET", "/api/configuration", 
        [this](const HttpRequest& req) { return handleConfiguration(req); });
    
    http_server_->addRoute("GET", "/api/dashboard-data", 
        [this](const HttpRequest& req) { return handleDashboardData(req); });
    
    // Static files (for serving dashboard HTML/JS/CSS)
    http_server_->addRoute("GET", "/", 
        [this](const HttpRequest& req) { return handleStaticFiles(req); });
    
    utils::Logger::info("Dashboard routes configured");
}

void DashboardApp::runUpdateLoop() {
    while (running_) {
        // Generate demo data for testing
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
        
        // Update data exporter
        data_exporter_->updateMarketData(demo_market_data);
        data_exporter_->updatePricingResults(demo_pricing_results);
        data_exporter_->updateArbitrageOpportunities(demo_opportunities);
        
        // Sleep for update interval
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

HttpResponse DashboardApp::handleHealthCheck(const HttpRequest& request) {
    return createJsonResponse(nlohmann::json{
        {"status", "healthy"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    });
}

HttpResponse DashboardApp::handleSystemStatus(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportSystemStatus());
}

HttpResponse DashboardApp::handleMarketData(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportMarketData());
}

HttpResponse DashboardApp::handlePricingResults(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportPricingResults());
}

HttpResponse DashboardApp::handleArbitrageOpportunities(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportArbitrageOpportunities());
}

HttpResponse DashboardApp::handlePerformanceMetrics(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportPerformanceMetrics());
}

HttpResponse DashboardApp::handleRiskMetrics(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportRiskMetrics());
}

HttpResponse DashboardApp::handleConfiguration(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportConfiguration());
}

HttpResponse DashboardApp::handleDashboardData(const HttpRequest& request) {
    return createJsonResponse(data_exporter_->exportDashboardData());
}

HttpResponse DashboardApp::handleStaticFiles(const HttpRequest& request) {
    // Serve a simple HTML dashboard
    std::string html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Arbitrage Engine Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
                 color: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; }
        .card { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; 
                box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .metric { padding: 15px; background: #f8f9fa; border-radius: 5px; text-align: center; }
        .metric-value { font-size: 2em; font-weight: bold; color: #667eea; }
        .metric-label { color: #666; margin-top: 5px; }
        .status-healthy { color: #28a745; }
        .status-warning { color: #ffc107; }
        .status-error { color: #dc3545; }
        .refresh-btn { background: #667eea; color: white; border: none; padding: 10px 20px; 
                      border-radius: 5px; cursor: pointer; margin-bottom: 20px; }
        .refresh-btn:hover { background: #5a6fd8; }
        .data-table { width: 100%; border-collapse: collapse; }
        .data-table th, .data-table td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
        .data-table th { background-color: #f8f9fa; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Synthetic Arbitrage Detection Engine</h1>
            <p>Real-time Dashboard - Phase 9 Implementation</p>
        </div>
        
        <button class="refresh-btn" onclick="refreshData()">Refresh Data</button>
        
        <div class="grid">
            <div class="card">
                <h3>System Status</h3>
                <div id="system-status">Loading...</div>
            </div>
            
            <div class="card">
                <h3>Performance Metrics</h3>
                <div id="performance-metrics">Loading...</div>
            </div>
            
            <div class="card">
                <h3>Risk Metrics</h3>
                <div id="risk-metrics">Loading...</div>
            </div>
        </div>
        
        <div class="card">
            <h3>Market Data</h3>
            <div id="market-data">Loading...</div>
        </div>
        
        <div class="card">
            <h3>Pricing Results</h3>
            <div id="pricing-results">Loading...</div>
        </div>
        
        <div class="card">
            <h3>Arbitrage Opportunities</h3>
            <div id="arbitrage-opportunities">Loading...</div>
        </div>
    </div>

    <script>
        async function fetchData(endpoint) {
            try {
                const response = await fetch('/api/' + endpoint);
                return await response.json();
            } catch (error) {
                console.error('Error fetching ' + endpoint + ':', error);
                return { error: error.message };
            }
        }
        
        function formatNumber(num, decimals = 2) {
            return parseFloat(num).toFixed(decimals);
        }
        
        function formatCurrency(num) {
            return '$' + formatNumber(num, 2);
        }
        
        function formatPercent(num) {
            return formatNumber(num, 2) + '%';
        }
        
        async function updateSystemStatus() {
            const data = await fetchData('status');
            const statusHtml = `
                <div class="metric">
                    <div class="metric-value ${data.healthy ? 'status-healthy' : 'status-error'}">
                        ${data.healthy ? '✅ Healthy' : '❌ Error'}
                    </div>
                    <div class="metric-label">System Status</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${Math.floor(data.uptime_seconds / 60)}m</div>
                    <div class="metric-label">Uptime</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${formatPercent(data.success_rate)}</div>
                    <div class="metric-label">Success Rate</div>
                </div>
            `;
            document.getElementById('system-status').innerHTML = statusHtml;
        }
        
        async function updatePerformanceMetrics() {
            const data = await fetchData('performance-metrics');
            const metricsHtml = `
                <div class="metric">
                    <div class="metric-value">${data.total_calculations}</div>
                    <div class="metric-label">Total Calculations</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${formatNumber(data.average_calculation_time_ms, 3)}ms</div>
                    <div class="metric-label">Avg Calc Time</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${formatNumber(data.calculations_per_second, 0)}/s</div>
                    <div class="metric-label">Throughput</div>
                </div>
            `;
            document.getElementById('performance-metrics').innerHTML = metricsHtml;
        }
        
        async function updateRiskMetrics() {
            const data = await fetchData('risk-metrics');
            const riskHtml = `
                <div class="metric">
                    <div class="metric-value">${formatNumber(data.max_risk_score, 3)}</div>
                    <div class="metric-label">Max Risk Score</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${formatCurrency(data.total_exposure)}</div>
                    <div class="metric-label">Total Exposure</div>
                </div>
                <div class="metric">
                    <div class="metric-value">${data.active_opportunities}</div>
                    <div class="metric-label">Active Opportunities</div>
                </div>
            `;
            document.getElementById('risk-metrics').innerHTML = riskHtml;
        }
        
        async function updateMarketData() {
            const data = await fetchData('market-data');
            if (data.data && data.data.length > 0) {
                let tableHtml = `
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th>Symbol</th>
                                <th>Exchange</th>
                                <th>Bid</th>
                                <th>Ask</th>
                                <th>Last</th>
                                <th>Volume</th>
                                <th>Funding Rate</th>
                            </tr>
                        </thead>
                        <tbody>
                `;
                
                data.data.forEach(item => {
                    tableHtml += `
                        <tr>
                            <td>${item.symbol}</td>
                            <td>${item.exchange}</td>
                            <td>${formatCurrency(item.bid)}</td>
                            <td>${formatCurrency(item.ask)}</td>
                            <td>${formatCurrency(item.last)}</td>
                            <td>${formatNumber(item.volume)}</td>
                            <td>${formatPercent(item.funding_rate * 100)}</td>
                        </tr>
                    `;
                });
                
                tableHtml += '</tbody></table>';
                document.getElementById('market-data').innerHTML = tableHtml;
            } else {
                document.getElementById('market-data').innerHTML = '<p>No market data available</p>';
            }
        }
        
        async function updatePricingResults() {
            const data = await fetchData('pricing-results');
            if (data.data && data.data.length > 0) {
                let tableHtml = `
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th>Instrument</th>
                                <th>Synthetic Price</th>
                                <th>Confidence</th>
                                <th>Model</th>
                                <th>Calc Time</th>
                                <th>Status</th>
                            </tr>
                        </thead>
                        <tbody>
                `;
                
                data.data.forEach(item => {
                    tableHtml += `
                        <tr>
                            <td>${item.instrument_id}</td>
                            <td>${formatCurrency(item.synthetic_price)}</td>
                            <td>${formatPercent(item.confidence * 100)}</td>
                            <td>${item.model_name}</td>
                            <td>${formatNumber(item.calculation_time_ms, 2)}ms</td>
                            <td>${item.success ? '✅ Success' : '❌ Failed'}</td>
                        </tr>
                    `;
                });
                
                tableHtml += '</tbody></table>';
                document.getElementById('pricing-results').innerHTML = tableHtml;
            } else {
                document.getElementById('pricing-results').innerHTML = '<p>No pricing results available</p>';
            }
        }
        
        async function updateArbitrageOpportunities() {
            const data = await fetchData('arbitrage-opportunities');
            if (data.data && data.data.length > 0) {
                let tableHtml = `
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th>ID</th>
                                <th>Instruments</th>
                                <th>Exchanges</th>
                                <th>Expected Profit</th>
                                <th>Profit %</th>
                                <th>Required Capital</th>
                                <th>Risk Score</th>
                                <th>Confidence</th>
                            </tr>
                        </thead>
                        <tbody>
                `;
                
                data.data.forEach(item => {
                    tableHtml += `
                        <tr>
                            <td>${item.id}</td>
                            <td>${item.instrument1} / ${item.instrument2}</td>
                            <td>${item.exchange1} / ${item.exchange2}</td>
                            <td>${formatCurrency(item.expected_profit)}</td>
                            <td>${formatPercent(item.profit_percentage)}</td>
                            <td>${formatCurrency(item.required_capital)}</td>
                            <td>${formatNumber(item.risk_score, 3)}</td>
                            <td>${formatPercent(item.confidence * 100)}</td>
                        </tr>
                    `;
                });
                
                tableHtml += '</tbody></table>';
                document.getElementById('arbitrage-opportunities').innerHTML = tableHtml;
            } else {
                document.getElementById('arbitrage-opportunities').innerHTML = '<p>No arbitrage opportunities detected</p>';
            }
        }
        
        async function refreshData() {
            await Promise.all([
                updateSystemStatus(),
                updatePerformanceMetrics(),
                updateRiskMetrics(),
                updateMarketData(),
                updatePricingResults(),
                updateArbitrageOpportunities()
            ]);
        }
        
        // Initial load
        refreshData();
        
        // Auto-refresh every 5 seconds
        setInterval(refreshData, 5000);
    </script>
</body>
</html>
    )";
    
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "text/html";
    response.body = html;
    return response;
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
