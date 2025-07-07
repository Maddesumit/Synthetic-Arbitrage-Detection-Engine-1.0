#pragma once

#include "HttpServer.hpp"
#include "DataExporter.hpp"
#include "../core/PricingEngine.hpp"
#include "../core/ArbitrageEngine.hpp"
#include "../core/RiskManager.hpp"
#include "../core/PositionManager.hpp"
#include "../data/MarketData.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace arbitrage {
namespace ui {

/**
 * @brief Main dashboard application that serves the web UI
 */
class DashboardApp {
public:
    DashboardApp(int port = 8080);
    ~DashboardApp();

    /**
     * @brief Initialize the dashboard with engine components
     */
    void initialize(std::shared_ptr<core::PricingEngine> pricing_engine);

    /**
     * @brief Initialize the dashboard with arbitrage engine
     */
    void initializeArbitrageEngine(std::shared_ptr<core::ArbitrageEngine> arbitrage_engine);

    /**
     * @brief Initialize the dashboard with risk management components
     */
    void initializeRiskManagement(std::shared_ptr<ArbitrageEngine::RiskManager> risk_manager,
                                  std::shared_ptr<ArbitrageEngine::PositionManager> position_manager);

    /**
     * @brief Start the dashboard server
     */
    void start();

    /**
     * @brief Start the dashboard server in background
     */
    void startAsync();

    /**
     * @brief Stop the dashboard server
     */
    void stop();

    /**
     * @brief Check if dashboard is running
     */
    bool isRunning() const;

    /**
     * @brief Update market data for display
     */
    void updateMarketData(const std::vector<data::MarketDataPoint>& data);

    /**
     * @brief Update pricing results for display
     */
    void updatePricingResults(const std::vector<core::PricingResult>& results);

    /**
     * @brief Update arbitrage opportunities for display
     */
    void updateArbitrageOpportunities(const std::vector<core::ArbitrageOpportunity>& opportunities);

    /**
     * @brief Update extended arbitrage opportunities for display
     */
    void updateExtendedArbitrageOpportunities(const std::vector<core::ArbitrageOpportunityExtended>& opportunities);

    /**
     * @brief Update positions for display
     */
    void updatePositions(const std::vector<ArbitrageEngine::Position>& positions);

    /**
     * @brief Update risk metrics for display
     */
    void updateRiskMetrics(const ArbitrageEngine::RiskMetrics& metrics);

    /**
     * @brief Start arbitrage detection
     */
    void startArbitrageDetection();

    /**
     * @brief Stop arbitrage detection
     */
    void stopArbitrageDetection();

    /**
     * @brief Get arbitrage engine performance metrics
     */
    nlohmann::json getArbitrageMetrics() const;

    /**
     * @brief Get dashboard URL
     */
    std::string getDashboardUrl() const;

private:
    std::unique_ptr<HttpServer> http_server_;
    std::unique_ptr<DataExporter> data_exporter_;
    std::shared_ptr<core::PricingEngine> pricing_engine_;
    std::shared_ptr<core::ArbitrageEngine> arbitrage_engine_;
    std::shared_ptr<ArbitrageEngine::RiskManager> risk_manager_;
    std::shared_ptr<ArbitrageEngine::PositionManager> position_manager_;
    
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> arbitrage_running_;
    std::thread update_thread_;
    std::thread arbitrage_thread_;
    
    // Data storage for demo
    std::mutex data_mutex_;
    std::vector<data::MarketDataPoint> latest_market_data_;
    std::vector<core::PricingResult> latest_pricing_results_;
    std::vector<core::ArbitrageOpportunity> latest_opportunities_;
    std::vector<core::ArbitrageOpportunityExtended> latest_extended_opportunities_;
    std::vector<ArbitrageEngine::Position> latest_positions_;
    ArbitrageEngine::RiskMetrics latest_risk_metrics_;
    std::chrono::system_clock::time_point last_update_time_;
    
    void setupRoutes();
    void runUpdateLoop();
    void runArbitrageLoop();
    void updateDemoData();
    
    // Route handlers
    HttpResponse handleApiStatus(const HttpRequest& request);
    HttpResponse handleApiMarketData(const HttpRequest& request);
    HttpResponse handleApiFilteredMarketData(const HttpRequest& request);
    HttpResponse handleApiPricingResults(const HttpRequest& request);
    HttpResponse handleApiFilteredPricingResults(const HttpRequest& request);
    HttpResponse handleApiOpportunities(const HttpRequest& request);
    HttpResponse handleApiExtendedOpportunities(const HttpRequest& request);
    HttpResponse handleApiArbitrageMetrics(const HttpRequest& request);
    HttpResponse handleApiArbitrageControl(const HttpRequest& request);
    HttpResponse handleApiPerformance(const HttpRequest& request);
    HttpResponse handleApiRisk(const HttpRequest& request);
    HttpResponse handleApiPositions(const HttpRequest& request);
    HttpResponse handleApiRiskMetrics(const HttpRequest& request);
    HttpResponse handleApiRiskAlerts(const HttpRequest& request);
    HttpResponse handleStaticFiles(const HttpRequest& request);
    
    // New advanced API handlers
    HttpResponse handleApiOrderBook(const HttpRequest& request);
    HttpResponse handleApiConnectionStatus(const HttpRequest& request);
    HttpResponse handleApiPortfolioMetrics(const HttpRequest& request);
    HttpResponse handleApiAdvancedRisk(const HttpRequest& request);
    
    // Phase 7: Performance Monitoring API handlers
    HttpResponse handleApiSystemMetrics(const HttpRequest& request);
    HttpResponse handleApiLatencyMetrics(const HttpRequest& request);
    HttpResponse handleApiThroughputMetrics(const HttpRequest& request);
    HttpResponse handleApiHealthStatus(const HttpRequest& request);
    HttpResponse handleApiBottlenecks(const HttpRequest& request);
    HttpResponse handleApiPerformanceHistory(const HttpRequest& request);
    HttpResponse handleApiExportReport(const HttpRequest& request);
    
    // Utility methods
    HttpResponse createJsonResponse(const nlohmann::json& data) const;
    HttpResponse createErrorResponse(int status_code, const std::string& message) const;
};

} // namespace ui
} // namespace arbitrage
