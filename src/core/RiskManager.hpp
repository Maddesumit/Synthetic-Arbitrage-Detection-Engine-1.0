#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include "../data/MarketData.hpp"

namespace ArbitrageEngine {

// Forward declarations
class PositionTracker;
class VaRCalculator;

// Use the correct namespace for MarketDataPoint
using MarketDataPoint = arbitrage::data::MarketDataPoint;

/**
 * @brief Risk metrics structure containing all key risk measures
 */
struct RiskMetrics {
    // VaR and risk measures
    double portfolioVaR{0.0};           // Portfolio Value at Risk (1-day, 95%)
    double expectedShortfall{0.0};       // Expected Shortfall (CVaR)
    double portfolioBeta{0.0};          // Portfolio beta relative to market
    double maxDrawdown{0.0};            // Maximum historical drawdown
    
    // Position and exposure metrics
    double totalExposure{0.0};          // Total notional exposure
    double leveragedExposure{0.0};      // Leveraged exposure across all positions
    double concentrationRisk{0.0};      // Concentration risk measure (0-1)
    double correlationRisk{0.0};        // Average correlation between positions
    
    // Liquidity and execution risk
    double liquidityRisk{0.0};          // Liquidity risk score (0-1)
    double executionRisk{0.0};          // Estimated execution cost percentage
    double marketImpact{0.0};           // Estimated market impact of positions
    
    // Funding and basis risk
    double fundingRateRisk{0.0};        // Funding rate exposure risk
    double basisRisk{0.0};              // Basis risk from synthetic positions
    double rolloverRisk{0.0};           // Risk from contract rollovers
    
    // Real-time metrics
    std::chrono::system_clock::time_point timestamp;
    bool isValid{true};
    std::string errorMessage;
};

/**
 * @brief Risk limits structure for controlling maximum exposures
 */
struct RiskLimits {
    // Portfolio-level limits
    double maxPortfolioVaR{1000000.0};     // Maximum portfolio VaR in USD
    double maxLeverage{10.0};              // Maximum portfolio leverage
    double maxConcentration{0.25};         // Maximum concentration per asset (25%)
    double maxDrawdown{0.15};              // Maximum allowed drawdown (15%)
    
    // Position-level limits
    double maxPositionSize{500000.0};      // Maximum position size in USD
    double maxPositionLeverage{5.0};       // Maximum position leverage
    double maxCorrelation{0.8};            // Maximum correlation between positions
    
    // Risk thresholds
    double liquidityThreshold{0.1};        // Minimum liquidity score required
    double executionCostThreshold{0.005};  // Maximum execution cost (0.5%)
    double fundingRateThreshold{0.01};     // Maximum funding rate exposure (1%)
    
    // Alert thresholds (percentage of limits)
    double warningThreshold{0.8};          // Warning at 80% of limit
    double criticalThreshold{0.95};        // Critical alert at 95% of limit
};

/**
 * @brief Position structure representing a trading position
 */
struct Position {
    std::string positionId;
    std::string symbol;
    std::string exchange;
    
    // Position details
    double size{0.0};                      // Position size (positive = long, negative = short)
    double entryPrice{0.0};                // Average entry price
    double currentPrice{0.0};              // Current market price
    double unrealizedPnL{0.0};             // Unrealized profit/loss
    double realizedPnL{0.0};               // Realized profit/loss
    
    // Risk metrics
    double notionalValue{0.0};             // Notional value of position
    double leverage{1.0};                  // Position leverage
    double marginRequired{0.0};            // Margin required for position
    double positionVaR{0.0};               // Position-specific VaR
    
    // Timing and metadata
    std::chrono::system_clock::time_point openTime;
    std::chrono::system_clock::time_point lastUpdate;
    bool isActive{true};
    
    // Synthetic position details
    bool isSynthetic{false};               // Whether this is a synthetic position
    std::vector<std::string> underlyingAssets; // For synthetic positions
    double correlationWithMarket{0.0};     // Correlation with market index
};

/**
 * @brief Risk alert structure for notifications
 */
struct RiskAlert {
    enum class Severity { INFO, WARNING, CRITICAL };
    enum class Type { 
        VAR_BREACH, 
        LEVERAGE_BREACH, 
        CONCENTRATION_BREACH, 
        LIQUIDITY_RISK, 
        CORRELATION_RISK,
        FUNDING_RATE_RISK,
        EXECUTION_COST_HIGH
    };
    
    Severity severity;
    Type type;
    std::string message;
    std::string positionId;  // Empty if portfolio-level alert
    double currentValue;
    double limitValue;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Main Risk Management Engine
 * 
 * This class implements comprehensive risk management including:
 * - Real-time VaR calculations
 * - Position tracking and exposure management
 * - Risk limits enforcement
 * - Liquidity and correlation risk assessment
 * - Alert generation and notification
 */
class RiskManager {
public:
    /**
     * @brief Constructor
     * @param limits Risk limits configuration
     */
    explicit RiskManager(const RiskLimits& limits = RiskLimits{});
    
    /**
     * @brief Destructor
     */
    ~RiskManager();
    
    // Core functionality
    bool initialize();
    void shutdown();
    bool isRunning() const { return isRunning_.load(); }
    
    // Position management
    bool addPosition(const Position& position);
    bool updatePosition(const std::string& positionId, const Position& position);
    bool closePosition(const std::string& positionId);
    std::vector<Position> getAllPositions() const;
    std::optional<Position> getPosition(const std::string& positionId) const;
    
    // Risk calculation and monitoring
    RiskMetrics calculateRiskMetrics() const;
    bool updateMarketData(const std::vector<MarketDataPoint>& marketData);
    void performStressTest(const std::vector<double>& stressScenarios);
    
    // Risk limits and alerts
    void updateRiskLimits(const RiskLimits& limits);
    RiskLimits getRiskLimits() const;
    std::vector<RiskAlert> checkRiskLimits() const;
    std::vector<RiskAlert> getActiveAlerts() const;
    
    // Real-time monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    void setAlertCallback(std::function<void(const RiskAlert&)> callback);
    
    // Risk analysis
    double calculatePortfolioVaR(double confidenceLevel = 0.95, int horizonDays = 1) const;
    double calculatePositionCorrelation(const std::string& positionId1, 
                                       const std::string& positionId2) const;
    double estimateMarketImpact(const Position& position) const;
    double calculateLiquidityScore(const std::string& symbol, const std::string& exchange) const;
    
    // Configuration and status
    void setConfiguration(const std::string& configPath);
    std::string getStatusReport() const;
    void exportRiskReport(const std::string& filename) const;
    
private:
    // Internal state
    mutable std::mutex positionsMutex_;
    mutable std::mutex riskDataMutex_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> monitoringActive_{false};
    
    // Core components
    std::unique_ptr<PositionTracker> positionTracker_;
    std::unique_ptr<VaRCalculator> varCalculator_;
    
    // Data storage
    std::unordered_map<std::string, Position> positions_;
    std::unordered_map<std::string, std::vector<double>> priceHistory_;
    std::vector<RiskAlert> activeAlerts_;
    RiskLimits currentLimits_;
    
    // Market data
    std::unordered_map<std::string, MarketDataPoint> latestMarketData_;
    std::chrono::system_clock::time_point lastMarketDataUpdate_;
    
    // Callbacks and monitoring
    std::function<void(const RiskAlert&)> alertCallback_;
    std::thread monitoringThread_;
    
    // Internal methods
    void monitoringLoop();
    RiskMetrics calculateRiskMetricsInternal() const;
    void generateAlerts(const RiskMetrics& metrics) const;
    void updatePositionMetrics(Position& position) const;
    double calculatePositionVaR(const Position& position, double confidenceLevel) const;
    double calculateCorrelationMatrix() const;
    void performMonteCarlo(int numSimulations = 10000) const;
    
    // Risk calculations
    double calculateExpectedShortfall(double confidenceLevel = 0.95) const;
    double calculateMaxDrawdown() const;
    double calculateConcentrationRisk() const;
    double calculateFundingRateRisk() const;
    
    // Utilities
    void logRiskEvent(const std::string& event) const;
    bool isValidPosition(const Position& position) const;
    void cleanupExpiredAlerts();
};

} // namespace ArbitrageEngine
