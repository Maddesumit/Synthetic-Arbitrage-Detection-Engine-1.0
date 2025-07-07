#include "DataExporter.hpp"
#include "../utils/Logger.hpp"
#include <iomanip>
#include <sstream>

namespace arbitrage {
namespace ui {

DataExporter::DataExporter() 
    : start_time_(std::chrono::steady_clock::now())
    , total_calculations_(0)
    , successful_calculations_(0)
    , total_calculation_time_ms_(0.0)
    , system_healthy_(true)
    , system_status_("Running") {
}

DataExporter::~DataExporter() = default;

void DataExporter::updateMarketData(const std::vector<data::MarketDataPoint>& data) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    market_data_ = data;
    
    // Keep only last 1000 points for performance
    if (market_data_.size() > 1000) {
        market_data_.erase(market_data_.begin(), market_data_.end() - 1000);
    }
}

void DataExporter::updatePricingResults(const std::vector<core::PricingResult>& results) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    pricing_results_ = results;
    
    // Update performance metrics
    total_calculations_ += results.size();
    for (const auto& result : results) {
        if (result.success) {
            ++successful_calculations_;
        }
        total_calculation_time_ms_ += result.calculation_time_ms;
    }
}

void DataExporter::updateArbitrageOpportunities(const std::vector<core::ArbitrageOpportunity>& opportunities) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    arbitrage_opportunities_ = opportunities;
    
    // Keep only last 100 opportunities
    if (arbitrage_opportunities_.size() > 100) {
        arbitrage_opportunities_.erase(arbitrage_opportunities_.begin(), 
                                     arbitrage_opportunities_.end() - 100);
    }
}

nlohmann::json DataExporter::exportSystemStatus() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time_).count();
    
    return nlohmann::json{
        {"healthy", system_healthy_},
        {"status", system_status_},
        {"uptime_seconds", uptime},
        {"total_calculations", total_calculations_},
        {"successful_calculations", successful_calculations_},
        {"success_rate", total_calculations_ > 0 ? 
            static_cast<double>(successful_calculations_) / total_calculations_ * 100.0 : 0.0},
        {"avg_calculation_time_ms", total_calculations_ > 0 ? 
            total_calculation_time_ms_ / total_calculations_ : 0.0},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportMarketData() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    nlohmann::json result = nlohmann::json::array();
    for (const auto& point : market_data_) {
        result.push_back(serializeMarketDataPoint(point));
    }
    
    return nlohmann::json{
        {"data", result},
        {"count", market_data_.size()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportPricingResults() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    nlohmann::json result = nlohmann::json::array();
    for (const auto& pricing_result : pricing_results_) {
        result.push_back(serializePricingResult(pricing_result));
    }
    
    return nlohmann::json{
        {"data", result},
        {"count", pricing_results_.size()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportArbitrageOpportunities() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    nlohmann::json result = nlohmann::json::array();
    for (const auto& opportunity : arbitrage_opportunities_) {
        result.push_back(serializeArbitrageOpportunity(opportunity));
    }
    
    return nlohmann::json{
        {"data", result},
        {"count", arbitrage_opportunities_.size()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    return nlohmann::json{
        {"total_calculations", total_calculations_},
        {"successful_calculations", successful_calculations_},
        {"failed_calculations", total_calculations_ - successful_calculations_},
        {"success_rate_percent", total_calculations_ > 0 ? 
            static_cast<double>(successful_calculations_) / total_calculations_ * 100.0 : 0.0},
        {"total_calculation_time_ms", total_calculation_time_ms_},
        {"average_calculation_time_ms", total_calculations_ > 0 ? 
            total_calculation_time_ms_ / total_calculations_ : 0.0},
        {"calculations_per_second", total_calculation_time_ms_ > 0 ? 
            total_calculations_ / (total_calculation_time_ms_ / 1000.0) : 0.0},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportRiskMetrics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Calculate risk metrics from current opportunities
    double max_risk_score = 0.0;
    double avg_risk_score = 0.0;
    double total_exposure = 0.0;
    
    if (!arbitrage_opportunities_.empty()) {
        for (const auto& opp : arbitrage_opportunities_) {
            max_risk_score = std::max(max_risk_score, opp.risk_score);
            avg_risk_score += opp.risk_score;
            total_exposure += opp.required_capital;
        }
        avg_risk_score /= arbitrage_opportunities_.size();
    }
    
    return nlohmann::json{
        {"max_risk_score", max_risk_score},
        {"average_risk_score", avg_risk_score},
        {"total_exposure", total_exposure},
        {"active_opportunities", arbitrage_opportunities_.size()},
        {"risk_threshold", 0.5}, // From configuration
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportConfiguration() const {
    return nlohmann::json{
        {"arbitrage", {
            {"min_profit_threshold", 0.001},
            {"max_risk_score", 0.5},
            {"min_confidence", 0.7}
        }},
        {"pricing", {
            {"risk_free_rate", 0.05},
            {"default_volatility", 0.3},
            {"funding_rate_weight", 1.0}
        }},
        {"system", {
            {"max_threads", std::thread::hardware_concurrency()},
            {"update_interval_ms", 100},
            {"data_retention_hours", 24}
        }},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::exportDashboardData() const {
    return nlohmann::json{
        {"system_status", exportSystemStatus()},
        {"market_data", exportMarketData()},
        {"pricing_results", exportPricingResults()},
        {"arbitrage_opportunities", exportArbitrageOpportunities()},
        {"performance_metrics", exportPerformanceMetrics()},
        {"risk_metrics", exportRiskMetrics()},
        {"configuration", exportConfiguration()}
    };
}

nlohmann::json DataExporter::serializeMarketDataPoint(const data::MarketDataPoint& point) const {
    return nlohmann::json{
        {"symbol", point.symbol},
        {"exchange", point.exchange},
        {"timestamp", point.timestamp.count()},
        {"bid", point.bid},
        {"ask", point.ask},
        {"last", point.last},
        {"volume", point.volume},
        {"funding_rate", point.funding_rate}
    };
}

nlohmann::json DataExporter::serializePricingResult(const core::PricingResult& result) const {
    nlohmann::json components = {
        {"base_price", result.components.base_price},
        {"carry_cost", result.components.carry_cost},
        {"funding_adjustment", result.components.funding_adjustment},
        {"volatility_component", result.components.volatility_component},
        {"time_value", result.components.time_value}
    };
    
    nlohmann::json greeks = nlohmann::json::object();
    if (result.greeks.has_value()) {
        const auto& g = result.greeks.value();
        greeks = {
            {"delta", g.delta},
            {"gamma", g.gamma},
            {"theta", g.theta},
            {"vega", g.vega},
            {"rho", g.rho}
        };
    }
    
    return nlohmann::json{
        {"instrument_id", result.instrument_id},
        {"synthetic_price", result.synthetic_price},
        {"confidence", result.confidence},
        {"model_name", result.model_name},
        {"components", components},
        {"greeks", greeks},
        {"success", result.success},
        {"calculation_time_ms", result.calculation_time_ms},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            result.timestamp.time_since_epoch()).count()}
    };
}

nlohmann::json DataExporter::serializeArbitrageOpportunity(const core::ArbitrageOpportunity& opportunity) const {
    nlohmann::json legs_json = nlohmann::json::array();
    for (const auto& leg : opportunity.legs) {
        legs_json.push_back({
            {"symbol", leg.symbol},
            {"type", static_cast<int>(leg.type)},
            {"exchange", static_cast<int>(leg.exchange)},
            {"price", leg.price},
            {"synthetic_price", leg.synthetic_price},
            {"deviation", leg.deviation},
            {"action", leg.action}
        });
    }
    
    return nlohmann::json{
        {"underlying_symbol", opportunity.underlying_symbol},
        {"legs", legs_json},
        {"expected_profit_pct", opportunity.expected_profit_pct},
        {"required_capital", opportunity.required_capital},
        {"risk_score", opportunity.risk_score},
        {"confidence", opportunity.confidence},
        {"detected_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            opportunity.detected_at.time_since_epoch()).count()}
    };
}

} // namespace ui
} // namespace arbitrage
