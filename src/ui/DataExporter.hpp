#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>
#include "../core/PricingEngine.hpp"
#include "../data/MarketData.hpp"

namespace arbitrage {
namespace ui {

/**
 * @brief Data exporter for UI dashboard
 */
class DataExporter {
public:
    DataExporter();
    ~DataExporter();

    /**
     * @brief Update market data for export
     */
    void updateMarketData(const std::vector<data::MarketDataPoint>& data);

    /**
     * @brief Update pricing results for export
     */
    void updatePricingResults(const std::vector<core::PricingResult>& results);

    /**
     * @brief Update arbitrage opportunities for export
     */
    void updateArbitrageOpportunities(const std::vector<core::ArbitrageOpportunity>& opportunities);

    /**
     * @brief Export current system status as JSON
     */
    nlohmann::json exportSystemStatus() const;

    /**
     * @brief Export market data as JSON
     */
    nlohmann::json exportMarketData() const;

    /**
     * @brief Export pricing results as JSON
     */
    nlohmann::json exportPricingResults() const;

    /**
     * @brief Export arbitrage opportunities as JSON
     */
    nlohmann::json exportArbitrageOpportunities() const;

    /**
     * @brief Export performance metrics as JSON
     */
    nlohmann::json exportPerformanceMetrics() const;

    /**
     * @brief Export risk metrics as JSON
     */
    nlohmann::json exportRiskMetrics() const;

    /**
     * @brief Export configuration as JSON
     */
    nlohmann::json exportConfiguration() const;

    /**
     * @brief Export complete dashboard data as JSON
     */
    nlohmann::json exportDashboardData() const;

    /**
     * @brief Serialize individual data structures (public for dashboard)
     */
    nlohmann::json serializeMarketDataPoint(const data::MarketDataPoint& point) const;
    nlohmann::json serializePricingResult(const core::PricingResult& result) const;
    nlohmann::json serializeArbitrageOpportunity(const core::ArbitrageOpportunity& opportunity) const;

private:
    mutable std::mutex data_mutex_;
    
    // Cached data for export
    std::vector<data::MarketDataPoint> market_data_;
    std::vector<core::PricingResult> pricing_results_;
    std::vector<core::ArbitrageOpportunity> arbitrage_opportunities_;
    
    // Performance metrics
    std::chrono::steady_clock::time_point start_time_;
    size_t total_calculations_;
    size_t successful_calculations_;
    double total_calculation_time_ms_;
    
    // System status
    bool system_healthy_;
    std::string system_status_;
    
    void updatePerformanceMetrics();
};

} // namespace ui
} // namespace arbitrage
