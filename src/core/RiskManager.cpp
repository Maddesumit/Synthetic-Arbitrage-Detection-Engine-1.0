#include "RiskManager.hpp"
#include "../utils/Logger.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>

namespace ArbitrageEngine {

// Use the correct namespace alias
using MarketDataPoint = arbitrage::data::MarketDataPoint;
using Logger = arbitrage::utils::Logger;

// Helper class for Value at Risk calculations
class VaRCalculator {
public:
    /**
     * @brief Calculate portfolio VaR using Monte Carlo simulation
     */
    double calculateVaR(const std::vector<Position>& positions, 
                       const std::unordered_map<std::string, std::vector<double>>& priceHistory,
                       double confidenceLevel = 0.95, 
                       int numSimulations = 10000) {
        if (positions.empty() || priceHistory.empty()) {
            return 0.0;
        }
        
        std::vector<double> portfolioReturns = simulateReturns(positions, priceHistory, numSimulations);
        if (portfolioReturns.empty()) {
            return 0.0;
        }
        
        std::sort(portfolioReturns.begin(), portfolioReturns.end());
        size_t varIndex = static_cast<size_t>((1.0 - confidenceLevel) * portfolioReturns.size());
        
        return std::abs(portfolioReturns[varIndex]);
    }
    
    /**
     * @brief Calculate Expected Shortfall (CVaR)
     */
    double calculateExpectedShortfall(const std::vector<Position>& positions,
                                    const std::unordered_map<std::string, std::vector<double>>& priceHistory,
                                    double confidenceLevel = 0.95,
                                    int numSimulations = 10000) {
        std::vector<double> portfolioReturns = simulateReturns(positions, priceHistory, numSimulations);
        if (portfolioReturns.empty()) {
            return 0.0;
        }
        
        std::sort(portfolioReturns.begin(), portfolioReturns.end());
        size_t varIndex = static_cast<size_t>((1.0 - confidenceLevel) * portfolioReturns.size());
        
        double sum = 0.0;
        for (size_t i = 0; i < varIndex; ++i) {
            sum += portfolioReturns[i];
        }
        
        return std::abs(sum / varIndex);
    }

private:
    std::vector<double> simulateReturns(const std::vector<Position>& positions,
                                       const std::unordered_map<std::string, std::vector<double>>& priceHistory,
                                       int numSimulations) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dis(0.0, 1.0);
        
        std::vector<double> portfolioReturns;
        portfolioReturns.reserve(numSimulations);
        
        for (int sim = 0; sim < numSimulations; ++sim) {
            double portfolioReturn = 0.0;
            
            for (const auto& position : positions) {
                std::string key = position.symbol + "_" + position.exchange;
                auto histIt = priceHistory.find(key);
                
                if (histIt != priceHistory.end() && !histIt->second.empty()) {
                    double volatility = calculateVolatility(histIt->second);
                    double randomReturn = dis(gen) * volatility;
                    double positionReturn = randomReturn * position.notionalValue;
                    portfolioReturn += positionReturn;
                }
            }
            
            portfolioReturns.push_back(portfolioReturn);
        }
        
        return portfolioReturns;
    }
    
    double calculateVolatility(const std::vector<double>& prices) {
        if (prices.size() < 2) return 0.01; // Default 1% volatility
        
        std::vector<double> returns;
        for (size_t i = 1; i < prices.size(); ++i) {
            if (prices[i-1] != 0.0) {
                returns.push_back(std::log(prices[i] / prices[i-1]));
            }
        }
        
        if (returns.empty()) return 0.01;
        
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        
        for (double ret : returns) {
            variance += (ret - mean) * (ret - mean);
        }
        
        return std::sqrt(variance / returns.size());
    }
};

// Helper class for position tracking
class PositionTracker {
public:
    void updatePosition(const std::string& positionId, const Position& position) {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        positions_[positionId] = position;
        lastUpdate_ = std::chrono::system_clock::now();
    }
    
    std::optional<Position> getPosition(const std::string& positionId) {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        auto it = positions_.find(positionId);
        if (it != positions_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<Position> getAllPositions() {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        std::vector<Position> result;
        result.reserve(positions_.size());
        
        for (const auto& [id, position] : positions_) {
            if (position.isActive) {
                result.push_back(position);
            }
        }
        
        return result;
    }
    
    bool removePosition(const std::string& positionId) {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        return positions_.erase(positionId) > 0;
    }
    
    double getTotalExposure() {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        double total = 0.0;
        for (const auto& [id, position] : positions_) {
            if (position.isActive) {
                total += std::abs(position.notionalValue);
            }
        }
        return total;
    }

private:
    std::mutex positionsMutex_;
    std::unordered_map<std::string, Position> positions_;
    std::chrono::system_clock::time_point lastUpdate_;
};

// RiskManager Implementation
RiskManager::RiskManager(const RiskLimits& limits) 
    : currentLimits_(limits),
      positionTracker_(std::make_unique<PositionTracker>()),
      varCalculator_(std::make_unique<VaRCalculator>()) {
    
    LOG_INFO("RiskManager initialized with limits: MaxVaR={}, MaxLeverage={}", 
             limits.maxPortfolioVaR, limits.maxLeverage);
}

RiskManager::~RiskManager() {
    shutdown();
}

bool RiskManager::initialize() {
    if (isRunning_.load()) {
        LOG_WARN("RiskManager already running");
        return true;
    }
    
    try {
        // Initialize internal components
        lastMarketDataUpdate_ = std::chrono::system_clock::now();
        
        // Clear any existing data
        {
            std::lock_guard<std::mutex> lock(positionsMutex_);
            positions_.clear();
            activeAlerts_.clear();
        }
        
        isRunning_.store(true);
        LOG_INFO("RiskManager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize RiskManager: {}", e.what());
        return false;
    }
}

void RiskManager::shutdown() {
    if (!isRunning_.load()) {
        return;
    }
    
    // Stop monitoring first
    stopRealTimeMonitoring();
    
    // Clean up
    isRunning_.store(false);
    
    LOG_INFO("RiskManager shutdown completed");
}

bool RiskManager::addPosition(const Position& position) {
    if (!isValidPosition(position)) {
        LOG_ERROR("Invalid position: {}", position.positionId);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(positionsMutex_);
    positions_[position.positionId] = position;
    
    // Update position tracker
    positionTracker_->updatePosition(position.positionId, position);
    
    LOG_INFO("Added position: {} {} {} size={}", 
             position.positionId, position.symbol, position.exchange, position.size);
    
    return true;
}

bool RiskManager::updatePosition(const std::string& positionId, const Position& position) {
    if (!isValidPosition(position)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(positionsMutex_);
    auto it = positions_.find(positionId);
    if (it == positions_.end()) {
        LOG_ERROR("Position not found for update: {}", positionId);
        return false;
    }
    
    positions_[positionId] = position;
    positionTracker_->updatePosition(positionId, position);
    
    return true;
}

bool RiskManager::closePosition(const std::string& positionId) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    auto it = positions_.find(positionId);
    if (it == positions_.end()) {
        return false;
    }
    
    // Mark as inactive instead of removing
    it->second.isActive = false;
    it->second.size = 0.0;
    
    LOG_INFO("Closed position: {}", positionId);
    return true;
}

std::vector<Position> RiskManager::getAllPositions() const {
    return positionTracker_->getAllPositions();
}

std::optional<Position> RiskManager::getPosition(const std::string& positionId) const {
    return positionTracker_->getPosition(positionId);
}

RiskMetrics RiskManager::calculateRiskMetrics() const {
    std::lock_guard<std::mutex> lock(riskDataMutex_);
    return calculateRiskMetricsInternal();
}

RiskMetrics RiskManager::calculateRiskMetricsInternal() const {
    RiskMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    auto positions = positionTracker_->getAllPositions();
    if (positions.empty()) {
        return metrics;
    }
    
    try {
        // Calculate portfolio VaR
        metrics.portfolioVaR = varCalculator_->calculateVaR(positions, priceHistory_);
        metrics.expectedShortfall = varCalculator_->calculateExpectedShortfall(positions, priceHistory_);
        
        // Calculate exposures
        double totalNotional = 0.0;
        double leveragedNotional = 0.0;
        
        for (const auto& position : positions) {
            totalNotional += std::abs(position.notionalValue);
            leveragedNotional += std::abs(position.notionalValue * position.leverage);
        }
        
        metrics.totalExposure = totalNotional;
        metrics.leveragedExposure = leveragedNotional;
        
        // Calculate concentration risk
        metrics.concentrationRisk = calculateConcentrationRisk();
        
        // Calculate correlation risk
        metrics.correlationRisk = calculateCorrelationMatrix();
        
        // Calculate funding rate risk
        metrics.fundingRateRisk = calculateFundingRateRisk();
        
        // Calculate liquidity risk
        double totalLiquidityScore = 0.0;
        for (const auto& position : positions) {
            totalLiquidityScore += calculateLiquidityScore(position.symbol, position.exchange);
        }
        metrics.liquidityRisk = 1.0 - (totalLiquidityScore / positions.size());
        
        // Calculate max drawdown
        metrics.maxDrawdown = calculateMaxDrawdown();
        
        metrics.isValid = true;
        
    } catch (const std::exception& e) {
        metrics.isValid = false;
        metrics.errorMessage = e.what();
        LOG_ERROR("Error calculating risk metrics: {}", e.what());
    }
    
    return metrics;
}

bool RiskManager::updateMarketData(const std::vector<MarketDataPoint>& marketData) {
    std::lock_guard<std::mutex> lock(riskDataMutex_);
    
    for (const auto& data : marketData) {
        std::string key = data.symbol + "_" + data.exchange;
        latestMarketData_[key] = data;
        
        // Update price history - use last price (if available) or mid price
        double price = (data.last > 0) ? data.last : (data.bid + data.ask) / 2.0;
        priceHistory_[key].push_back(price);
        
        // Keep only last 1000 prices for memory efficiency
        if (priceHistory_[key].size() > 1000) {
            priceHistory_[key].erase(priceHistory_[key].begin());
        }
    }
    
    lastMarketDataUpdate_ = std::chrono::system_clock::now();
    return true;
}

std::vector<RiskAlert> RiskManager::checkRiskLimits() const {
    std::vector<RiskAlert> alerts;
    RiskMetrics metrics = calculateRiskMetrics();
    
    if (!metrics.isValid) {
        return alerts;
    }
    
    // Check VaR limit
    if (metrics.portfolioVaR > currentLimits_.maxPortfolioVaR * currentLimits_.criticalThreshold) {
        RiskAlert alert;
        alert.severity = RiskAlert::Severity::CRITICAL;
        alert.type = RiskAlert::Type::VAR_BREACH;
        alert.message = "Portfolio VaR exceeded critical threshold";
        alert.currentValue = metrics.portfolioVaR;
        alert.limitValue = currentLimits_.maxPortfolioVaR;
        alert.timestamp = std::chrono::system_clock::now();
        alerts.push_back(alert);
    } else if (metrics.portfolioVaR > currentLimits_.maxPortfolioVaR * currentLimits_.warningThreshold) {
        RiskAlert alert;
        alert.severity = RiskAlert::Severity::WARNING;
        alert.type = RiskAlert::Type::VAR_BREACH;
        alert.message = "Portfolio VaR approaching limit";
        alert.currentValue = metrics.portfolioVaR;
        alert.limitValue = currentLimits_.maxPortfolioVaR;
        alert.timestamp = std::chrono::system_clock::now();
        alerts.push_back(alert);
    }
    
    // Check concentration risk
    if (metrics.concentrationRisk > currentLimits_.maxConcentration) {
        RiskAlert alert;
        alert.severity = RiskAlert::Severity::WARNING;
        alert.type = RiskAlert::Type::CONCENTRATION_BREACH;
        alert.message = "Portfolio concentration too high";
        alert.currentValue = metrics.concentrationRisk;
        alert.limitValue = currentLimits_.maxConcentration;
        alert.timestamp = std::chrono::system_clock::now();
        alerts.push_back(alert);
    }
    
    // Check liquidity risk
    if (metrics.liquidityRisk > currentLimits_.liquidityThreshold) {
        RiskAlert alert;
        alert.severity = RiskAlert::Severity::WARNING;
        alert.type = RiskAlert::Type::LIQUIDITY_RISK;
        alert.message = "Low liquidity detected in portfolio";
        alert.currentValue = metrics.liquidityRisk;
        alert.limitValue = currentLimits_.liquidityThreshold;
        alert.timestamp = std::chrono::system_clock::now();
        alerts.push_back(alert);
    }
    
    return alerts;
}

void RiskManager::startRealTimeMonitoring() {
    if (monitoringActive_.load()) {
        LOG_WARN("Real-time monitoring already active");
        return;
    }
    
    monitoringActive_.store(true);
    monitoringThread_ = std::thread(&RiskManager::monitoringLoop, this);
    
    LOG_INFO("Started real-time risk monitoring");
}

void RiskManager::stopRealTimeMonitoring() {
    if (!monitoringActive_.load()) {
        return;
    }
    
    monitoringActive_.store(false);
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    LOG_INFO("Stopped real-time risk monitoring");
}

void RiskManager::monitoringLoop() {
    const auto monitoringInterval = std::chrono::seconds(5); // Check every 5 seconds
    
    while (monitoringActive_.load()) {
        try {
            // Calculate current risk metrics
            RiskMetrics metrics = calculateRiskMetrics();
            
            // Check for limit breaches
            std::vector<RiskAlert> newAlerts = checkRiskLimits();
            
            // Process new alerts
            for (const auto& alert : newAlerts) {
                {
                    std::lock_guard<std::mutex> lock(positionsMutex_);
                    activeAlerts_.push_back(alert);
                }
                
                // Trigger callback if set
                if (alertCallback_) {
                    alertCallback_(alert);
                }
                
                logRiskEvent("Risk alert generated: " + alert.message);
            }
            
            // Clean up old alerts
            cleanupExpiredAlerts();
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error in risk monitoring loop: {}", e.what());
        }
        
        std::this_thread::sleep_for(monitoringInterval);
    }
}

double RiskManager::calculatePortfolioVaR(double confidenceLevel, int horizonDays) const {
    auto positions = positionTracker_->getAllPositions();
    return varCalculator_->calculateVaR(positions, priceHistory_, confidenceLevel);
}

double RiskManager::calculateConcentrationRisk() const {
    auto positions = positionTracker_->getAllPositions();
    if (positions.empty()) {
        return 0.0;
    }
    
    // Group positions by symbol
    std::unordered_map<std::string, double> symbolExposure;
    double totalExposure = 0.0;
    
    for (const auto& position : positions) {
        double exposure = std::abs(position.notionalValue);
        symbolExposure[position.symbol] += exposure;
        totalExposure += exposure;
    }
    
    if (totalExposure == 0.0) {
        return 0.0;
    }
    
    // Find maximum concentration
    double maxConcentration = 0.0;
    for (const auto& [symbol, exposure] : symbolExposure) {
        double concentration = exposure / totalExposure;
        maxConcentration = std::max(maxConcentration, concentration);
    }
    
    return maxConcentration;
}

double RiskManager::calculateFundingRateRisk() const {
    // Simplified funding rate risk calculation
    // In practice, this would analyze funding rate exposure across positions
    auto positions = positionTracker_->getAllPositions();
    
    double totalFundingExposure = 0.0;
    double totalExposure = 0.0;
    
    for (const auto& position : positions) {
        if (position.isSynthetic) {
            // Synthetic positions have funding rate exposure
            totalFundingExposure += std::abs(position.notionalValue);
        }
        totalExposure += std::abs(position.notionalValue);
    }
    
    return totalExposure > 0.0 ? (totalFundingExposure / totalExposure) : 0.0;
}

double RiskManager::calculateCorrelationMatrix() const {
    // Simplified correlation calculation
    // In practice, this would compute full correlation matrix
    return 0.5; // Placeholder - moderate correlation
}

double RiskManager::calculateMaxDrawdown() const {
    // Placeholder implementation
    // In practice, this would track historical P&L and compute maximum drawdown
    return 0.05; // 5% placeholder drawdown
}

double RiskManager::calculateLiquidityScore(const std::string& symbol, const std::string& exchange) const {
    // Simplified liquidity scoring
    // In practice, this would analyze orderbook depth and trading volume
    
    std::string key = symbol + "_" + exchange;
    auto it = latestMarketData_.find(key);
    
    if (it != latestMarketData_.end()) {
        // Higher volume = higher liquidity score
        double volumeScore = std::min(1.0, it->second.volume / 1000000.0); // Normalize to 1M volume
        return volumeScore;
    }
    
    return 0.5; // Default moderate liquidity
}

void RiskManager::setAlertCallback(std::function<void(const RiskAlert&)> callback) {
    alertCallback_ = callback;
}

void RiskManager::updateRiskLimits(const RiskLimits& limits) {
    std::lock_guard<std::mutex> lock(riskDataMutex_);
    currentLimits_ = limits;
    
    LOG_INFO("Updated risk limits: MaxVaR={}, MaxLeverage={}", 
             limits.maxPortfolioVaR, limits.maxLeverage);
}

RiskLimits RiskManager::getRiskLimits() const {
    std::lock_guard<std::mutex> lock(riskDataMutex_);
    return currentLimits_;
}

std::vector<RiskAlert> RiskManager::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    return activeAlerts_;
}

bool RiskManager::isValidPosition(const Position& position) const {
    return !position.positionId.empty() && 
           !position.symbol.empty() && 
           !position.exchange.empty() &&
           position.notionalValue >= 0.0;
}

void RiskManager::logRiskEvent(const std::string& event) const {
    LOG_INFO("Risk Event: {}", event);
}

void RiskManager::cleanupExpiredAlerts() {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto expiry = std::chrono::minutes(30); // Alerts expire after 30 minutes
    
    activeAlerts_.erase(
        std::remove_if(activeAlerts_.begin(), activeAlerts_.end(),
            [now, expiry](const RiskAlert& alert) {
                return (now - alert.timestamp) > expiry;
            }),
        activeAlerts_.end()
    );
}

std::string RiskManager::getStatusReport() const {
    std::ostringstream report;
    
    RiskMetrics metrics = calculateRiskMetrics();
    auto positions = getAllPositions();
    auto alerts = getActiveAlerts();
    
    report << "=== Risk Manager Status Report ===\n";
    report << "Running: " << (isRunning_.load() ? "Yes" : "No") << "\n";
    report << "Monitoring: " << (monitoringActive_.load() ? "Active" : "Inactive") << "\n";
    report << "Active Positions: " << positions.size() << "\n";
    report << "Active Alerts: " << alerts.size() << "\n";
    report << "\n=== Risk Metrics ===\n";
    report << "Portfolio VaR: $" << metrics.portfolioVaR << "\n";
    report << "Expected Shortfall: $" << metrics.expectedShortfall << "\n";
    report << "Total Exposure: $" << metrics.totalExposure << "\n";
    report << "Leveraged Exposure: $" << metrics.leveragedExposure << "\n";
    report << "Concentration Risk: " << (metrics.concentrationRisk * 100) << "%\n";
    report << "Liquidity Risk: " << (metrics.liquidityRisk * 100) << "%\n";
    
    return report.str();
}

} // namespace ArbitrageEngine
