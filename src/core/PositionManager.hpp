#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include "RiskManager.hpp"

namespace ArbitrageEngine {

// Use the correct namespace alias
using MarketDataPoint = arbitrage::data::MarketDataPoint;

/**
 * @brief P&L calculation structure
 */
struct PnLData {
    double realizedPnL{0.0};
    double unrealizedPnL{0.0};
    double totalPnL{0.0};
    double dailyPnL{0.0};
    double weeklyPnL{0.0};
    double monthlyPnL{0.0};
    
    // Attribution
    double tradingPnL{0.0};        // P&L from trading activities
    double fundingPnL{0.0};        // P&L from funding rate differences
    double carryPnL{0.0};          // P&L from carry/basis differences
    
    std::chrono::system_clock::time_point lastUpdate;
    bool isValid{true};
};

/**
 * @brief Position sizing algorithms
 */
enum class PositionSizingMethod {
    FIXED_SIZE,           // Fixed position size
    FIXED_PERCENTAGE,     // Fixed percentage of capital
    KELLY_CRITERION,      // Kelly optimal sizing
    RISK_PARITY,         // Risk parity based sizing
    VOLATILITY_TARGET,   // Target volatility sizing
    MAX_DRAWDOWN_LIMIT   // Max drawdown constraint sizing
};

/**
 * @brief Position sizing parameters
 */
struct PositionSizingParams {
    PositionSizingMethod method{PositionSizingMethod::FIXED_PERCENTAGE};
    double fixedSize{100000.0};           // Fixed size in USD
    double fixedPercentage{0.05};         // 5% of capital
    double kellyFraction{0.25};           // Fractional Kelly (25% of full Kelly)
    double targetVolatility{0.15};        // 15% target volatility
    double maxDrawdownLimit{0.10};        // 10% maximum drawdown
    double confidenceLevel{0.95};         // 95% confidence for risk calculations
    
    // Risk constraints
    double maxPositionSize{500000.0};     // Maximum position size
    double maxLeverage{5.0};              // Maximum leverage per position
    double maxCorrelation{0.7};           // Maximum correlation with existing positions
};

/**
 * @brief Capital allocation structure
 */
struct CapitalAllocation {
    double totalCapital{0.0};
    double allocatedCapital{0.0};
    double availableCapital{0.0};
    double reservedCapital{0.0};          // Reserved for margin requirements
    double usedMargin{0.0};
    
    // Per-strategy allocation
    std::unordered_map<std::string, double> strategyAllocations;
    
    // Per-exchange allocation
    std::unordered_map<std::string, double> exchangeAllocations;
    
    std::chrono::system_clock::time_point lastUpdate;
};

/**
 * @brief Advanced Position Management System
 * 
 * This class implements sophisticated position management including:
 * - Intelligent position sizing with multiple algorithms
 * - Real-time P&L tracking and attribution
 * - Capital allocation optimization
 * - Risk-adjusted position management
 * - Dynamic position adjustments based on market conditions
 */
class PositionManager {
public:
    /**
     * @brief Constructor
     * @param riskManager Shared risk manager instance
     * @param sizingParams Position sizing parameters
     */
    explicit PositionManager(std::shared_ptr<RiskManager> riskManager,
                           const PositionSizingParams& sizingParams = PositionSizingParams{});
    
    /**
     * @brief Destructor
     */
    ~PositionManager();
    
    // Core functionality
    bool initialize(double initialCapital);
    void shutdown();
    bool isRunning() const { return isRunning_.load(); }
    
    // Position management
    std::optional<Position> calculateOptimalPosition(
        const std::string& symbol,
        const std::string& exchange,
        double expectedReturn,
        double expectedVolatility,
        const std::string& strategy = "default"
    );
    
    bool openPosition(const Position& position, const std::string& strategy = "default");
    bool adjustPosition(const std::string& positionId, double newSize, const std::string& reason = "");
    bool closePosition(const std::string& positionId, const std::string& reason = "");
    
    // Position sizing methods
    double calculateKellySize(double expectedReturn, double variance, double capital) const;
    double calculateRiskParitySize(const std::string& symbol, const std::string& exchange, double capital) const;
    double calculateVolatilityTargetSize(double targetVol, double assetVol, double capital) const;
    
    // P&L tracking
    void updatePositionPrices(const std::vector<MarketDataPoint>& marketData);
    PnLData calculatePortfolioPnL() const;
    PnLData calculatePositionPnL(const std::string& positionId) const;
    std::unordered_map<std::string, PnLData> getStrategyPnL() const;
    
    // Capital management
    bool allocateCapital(const std::string& strategy, double amount);
    bool deallocateCapital(const std::string& strategy, double amount);
    CapitalAllocation getCapitalAllocation() const;
    double getAvailableCapital(const std::string& strategy = "") const;
    
    // Risk management integration
    bool checkPositionRisk(const Position& position) const;
    std::vector<std::string> getPositionsViolatingLimits() const;
    void rebalancePortfolio();
    
    // Portfolio analytics
    double getPortfolioVolatility() const;
    double getPortfolioBeta() const;
    double getSharpeRatio() const;
    double getMaxDrawdown() const;
    
    // Configuration and control
    void updateSizingParams(const PositionSizingParams& params);
    PositionSizingParams getSizingParams() const;
    void setRebalanceCallback(std::function<void(const std::vector<Position>&)> callback);
    
    // Reporting
    std::string getPositionReport() const;
    std::string getPerformanceReport() const;
    void exportPositionData(const std::string& filename) const;
    
    // Additional utility methods for dashboard
    std::vector<Position> getAllActivePositions() const;
    int getActivePositionCount() const;
    
private:
    // Internal state
    mutable std::mutex positionsMutex_;
    mutable std::mutex capitalMutex_;
    mutable std::mutex pnlMutex_;
    std::atomic<bool> isRunning_{false};
    
    // Core components
    std::shared_ptr<RiskManager> riskManager_;
    
    // Configuration
    PositionSizingParams sizingParams_;
    
    // Data storage
    std::unordered_map<std::string, Position> activePositions_;
    std::unordered_map<std::string, std::vector<Position>> positionHistory_;
    std::unordered_map<std::string, PnLData> positionPnL_;
    std::unordered_map<std::string, std::string> positionStrategies_; // positionId -> strategy
    
    // Capital and allocation
    CapitalAllocation capitalAllocation_;
    std::unordered_map<std::string, double> strategyCapital_;
    
    // Market data
    std::unordered_map<std::string, MarketDataPoint> latestPrices_;
    std::unordered_map<std::string, std::vector<double>> priceHistory_;
    
    // Performance tracking
    std::vector<double> portfolioValues_;
    std::vector<std::chrono::system_clock::time_point> valuationTimes_;
    
    // Callbacks
    std::function<void(const std::vector<Position>&)> rebalanceCallback_;
    
    // Internal methods
    double calculatePositionSize(const std::string& symbol, 
                               const std::string& exchange,
                               double expectedReturn,
                               double expectedVolatility,
                               const std::string& strategy) const;
    
    void updatePnL(const std::string& positionId);
    void updatePortfolioValue();
    bool validatePosition(const Position& position) const;
    double getCorrelationWithPortfolio(const std::string& symbol, const std::string& exchange) const;
    
    // Risk calculations
    double calculatePositionRisk(const Position& position) const;
    double calculatePortfolioRisk() const;
    bool wouldExceedConcentrationLimit(const Position& position) const;
    
    // Utility methods
    std::string generatePositionId(const std::string& symbol, const std::string& exchange) const;
    void logPositionEvent(const std::string& event, const std::string& positionId = "") const;
    double calculateReturns(const std::vector<double>& prices) const;
    double calculateVolatility(const std::vector<double>& returns) const;
};

} // namespace ArbitrageEngine
