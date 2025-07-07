#include "PositionManager.hpp"
#include "../utils/Logger.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace ArbitrageEngine {

// Use the correct namespace alias
using MarketDataPoint = arbitrage::data::MarketDataPoint;
using Logger = arbitrage::utils::Logger;

PositionManager::PositionManager(std::shared_ptr<RiskManager> riskManager,
                               const PositionSizingParams& sizingParams)
    : riskManager_(riskManager), sizingParams_(sizingParams) {
    
    LOG_INFO("PositionManager initialized with sizing method: {}", 
                              static_cast<int>(sizingParams.method));
}

PositionManager::~PositionManager() {
    shutdown();
}

bool PositionManager::initialize(double initialCapital) {
    if (isRunning_.load()) {
        LOG_WARN("PositionManager already running");
        return true;
    }
    
    try {
        // Initialize capital allocation
        {
            std::lock_guard<std::mutex> lock(capitalMutex_);
            capitalAllocation_.totalCapital = initialCapital;
            capitalAllocation_.availableCapital = initialCapital;
            capitalAllocation_.allocatedCapital = 0.0;
            capitalAllocation_.reservedCapital = 0.0;
            capitalAllocation_.usedMargin = 0.0;
            capitalAllocation_.lastUpdate = std::chrono::system_clock::now();
        }
        
        // Initialize portfolio tracking
        portfolioValues_.clear();
        portfolioValues_.push_back(initialCapital);
        valuationTimes_.clear();
        valuationTimes_.push_back(std::chrono::system_clock::now());
        
        isRunning_.store(true);
        LOG_INFO("PositionManager initialized with capital: ${:.2f}", initialCapital);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize PositionManager: {}", e.what());
        return false;
    }
}

void PositionManager::shutdown() {
    if (!isRunning_.load()) {
        return;
    }
    
    isRunning_.store(false);
    LOG_INFO("PositionManager shutdown completed");
}

std::optional<Position> PositionManager::calculateOptimalPosition(
    const std::string& symbol,
    const std::string& exchange,
    double expectedReturn,
    double expectedVolatility,
    const std::string& strategy) {
    
    if (!isRunning_.load()) {
        return std::nullopt;
    }
    
    try {
        // Calculate optimal position size
        double optimalSize = calculatePositionSize(symbol, exchange, expectedReturn, 
                                                 expectedVolatility, strategy);
        
        if (optimalSize <= 0.0) {
            LOG_DEBUG("Optimal position size is zero or negative for {}/{}", symbol, exchange);
            return std::nullopt;
        }
        
        // Create position structure
        Position position;
        position.positionId = generatePositionId(symbol, exchange);
        position.symbol = symbol;
        position.exchange = exchange;
        position.size = optimalSize;
        position.notionalValue = optimalSize; // Assuming 1:1 for now
        position.leverage = 1.0; // Default leverage
        position.openTime = std::chrono::system_clock::now();
        position.lastUpdate = position.openTime;
        position.isActive = true;
        position.isSynthetic = false;
        
        // Get current market price if available
        std::string key = symbol + "_" + exchange;
        auto priceIt = latestPrices_.find(key);
        if (priceIt != latestPrices_.end()) {
            double price = (priceIt->second.last > 0) ? priceIt->second.last : (priceIt->second.bid + priceIt->second.ask) / 2.0;
            position.currentPrice = price;
            position.entryPrice = price;
            position.notionalValue = optimalSize * price;
        }
        
        // Validate position against risk limits
        if (!checkPositionRisk(position)) {
            LOG_WARN("Position failed risk checks: {}/{}", symbol, exchange);
            return std::nullopt;
        }
        
        LOG_INFO("Calculated optimal position: {} {} size={:.2f}", 
                                 symbol, exchange, optimalSize);
        
        return position;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error calculating optimal position: {}", e.what());
        return std::nullopt;
    }
}

bool PositionManager::openPosition(const Position& position, const std::string& strategy) {
    if (!validatePosition(position)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(positionsMutex_);
    
    // Check if position already exists
    if (activePositions_.find(position.positionId) != activePositions_.end()) {
        LOG_ERROR("Position already exists: {}", position.positionId);
        return false;
    }
    
    // Check capital availability
    double requiredCapital = position.notionalValue / position.leverage;
    if (getAvailableCapital(strategy) < requiredCapital) {
        LOG_ERROR("Insufficient capital for position: {} (required: {:.2f})", 
                                  position.positionId, requiredCapital);
        return false;
    }
    
    // Add position to active positions
    activePositions_[position.positionId] = position;
    positionStrategies_[position.positionId] = strategy;
    
    // Initialize P&L tracking
    PnLData pnl;
    pnl.lastUpdate = std::chrono::system_clock::now();
    positionPnL_[position.positionId] = pnl;
    
    // Update capital allocation
    {
        std::lock_guard<std::mutex> capLock(capitalMutex_);
        capitalAllocation_.allocatedCapital += requiredCapital;
        capitalAllocation_.availableCapital -= requiredCapital;
        strategyCapital_[strategy] -= requiredCapital;
    }
    
    // Add to risk manager
    if (riskManager_) {
        riskManager_->addPosition(position);
    }
    
    logPositionEvent("Position opened", position.positionId);
    return true;
}

bool PositionManager::adjustPosition(const std::string& positionId, double newSize, const std::string& reason) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    
    auto it = activePositions_.find(positionId);
    if (it == activePositions_.end()) {
        LOG_ERROR("Position not found for adjustment: {}", positionId);
        return false;
    }
    
    Position& position = it->second;
    double oldSize = position.size;
    double oldNotional = position.notionalValue;
    
    // Update position
    position.size = newSize;
    position.notionalValue = newSize * position.currentPrice;
    position.lastUpdate = std::chrono::system_clock::now();
    
    // Update capital allocation
    double capitalChange = (position.notionalValue - oldNotional) / position.leverage;
    {
        std::lock_guard<std::mutex> capLock(capitalMutex_);
        capitalAllocation_.allocatedCapital += capitalChange;
        capitalAllocation_.availableCapital -= capitalChange;
        
        auto strategyIt = positionStrategies_.find(positionId);
        if (strategyIt != positionStrategies_.end()) {
            strategyCapital_[strategyIt->second] -= capitalChange;
        }
    }
    
    // Update risk manager
    if (riskManager_) {
        riskManager_->updatePosition(positionId, position);
    }
    
    LOG_INFO("Position adjusted: {} from {:.2f} to {:.2f} ({})", 
                              positionId, oldSize, newSize, reason);
    
    logPositionEvent("Position adjusted: " + reason, positionId);
    return true;
}

bool PositionManager::closePosition(const std::string& positionId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    
    auto it = activePositions_.find(positionId);
    if (it == activePositions_.end()) {
        LOG_ERROR("Position not found for closing: {}", positionId);
        return false;
    }
    
    Position& position = it->second;
    
    // Calculate final P&L
    updatePnL(positionId);
    
    // Update capital allocation
    double releasedCapital = position.notionalValue / position.leverage;
    {
        std::lock_guard<std::mutex> capLock(capitalMutex_);
        capitalAllocation_.allocatedCapital -= releasedCapital;
        capitalAllocation_.availableCapital += releasedCapital;
        
        auto strategyIt = positionStrategies_.find(positionId);
        if (strategyIt != positionStrategies_.end()) {
            strategyCapital_[strategyIt->second] += releasedCapital;
        }
    }
    
    // Move to history
    auto strategyIt = positionStrategies_.find(positionId);
    std::string strategy = (strategyIt != positionStrategies_.end()) ? strategyIt->second : "default";
    
    position.isActive = false;
    positionHistory_[strategy].push_back(position);
    
    // Remove from active positions
    activePositions_.erase(it);
    positionStrategies_.erase(positionId);
    
    // Update risk manager
    if (riskManager_) {
        riskManager_->closePosition(positionId);
    }
    
    LOG_INFO("Position closed: {} ({})", positionId, reason);
    logPositionEvent("Position closed: " + reason, positionId);
    
    return true;
}

double PositionManager::calculatePositionSize(const std::string& symbol, 
                                            const std::string& exchange,
                                            double expectedReturn,
                                            double expectedVolatility,
                                            const std::string& strategy) const {
    
    double availableCapital = getAvailableCapital(strategy);
    if (availableCapital <= 0.0) {
        return 0.0;
    }
    
    double calculatedSize = 0.0;
    
    switch (sizingParams_.method) {
        case PositionSizingMethod::FIXED_SIZE:
            calculatedSize = sizingParams_.fixedSize;
            break;
            
        case PositionSizingMethod::FIXED_PERCENTAGE:
            calculatedSize = availableCapital * sizingParams_.fixedPercentage;
            break;
            
        case PositionSizingMethod::KELLY_CRITERION:
            calculatedSize = calculateKellySize(expectedReturn, expectedVolatility * expectedVolatility, availableCapital);
            break;
            
        case PositionSizingMethod::RISK_PARITY:
            calculatedSize = calculateRiskParitySize(symbol, exchange, availableCapital);
            break;
            
        case PositionSizingMethod::VOLATILITY_TARGET:
            calculatedSize = calculateVolatilityTargetSize(sizingParams_.targetVolatility, expectedVolatility, availableCapital);
            break;
            
        case PositionSizingMethod::MAX_DRAWDOWN_LIMIT:
            // Conservative sizing based on max drawdown limit
            calculatedSize = availableCapital * sizingParams_.maxDrawdownLimit / (2.0 * expectedVolatility);
            break;
            
        default:
            calculatedSize = availableCapital * sizingParams_.fixedPercentage;
            break;
    }
    
    // Apply constraints
    calculatedSize = std::min(calculatedSize, sizingParams_.maxPositionSize);
    calculatedSize = std::min(calculatedSize, availableCapital);
    calculatedSize = std::max(calculatedSize, 0.0);
    
    return calculatedSize;
}

double PositionManager::calculateKellySize(double expectedReturn, double variance, double capital) const {
    if (variance <= 0.0 || expectedReturn <= 0.0) {
        return 0.0;
    }
    
    // Kelly fraction = (expected return - risk-free rate) / variance
    // Assuming risk-free rate is 0 for simplicity
    double kellyFraction = expectedReturn / variance;
    
    // Apply fractional Kelly to reduce risk
    kellyFraction *= sizingParams_.kellyFraction;
    
    // Kelly size = capital * kelly fraction
    return capital * kellyFraction;
}

double PositionManager::calculateRiskParitySize(const std::string& symbol, const std::string& exchange, double capital) const {
    // Get portfolio volatility
    double portfolioVol = getPortfolioVolatility();
    if (portfolioVol <= 0.0) {
        portfolioVol = 0.15; // Default 15% portfolio volatility
    }
    
    // Get asset volatility
    std::string key = symbol + "_" + exchange;
    auto histIt = priceHistory_.find(key);
    double assetVol = 0.20; // Default 20% asset volatility
    
    if (histIt != priceHistory_.end() && histIt->second.size() > 20) {
        std::vector<double> returns;
        const auto& prices = histIt->second;
        
        for (size_t i = 1; i < prices.size(); ++i) {
            if (prices[i-1] != 0.0) {
                returns.push_back(std::log(prices[i] / prices[i-1]));
            }
        }
        
        if (!returns.empty()) {
            assetVol = calculateVolatility(returns);
        }
    }
    
    // Risk parity sizing: inversely proportional to volatility
    double targetRisk = portfolioVol / std::sqrt(static_cast<double>(activePositions_.size() + 1));
    return capital * targetRisk / assetVol;
}

double PositionManager::calculateVolatilityTargetSize(double targetVol, double assetVol, double capital) const {
    if (assetVol <= 0.0) {
        return 0.0;
    }
    
    // Size = capital * (target volatility / asset volatility)
    return capital * targetVol / assetVol;
}

void PositionManager::updatePositionPrices(const std::vector<MarketDataPoint>& marketData) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    
    for (const auto& data : marketData) {
        std::string key = data.symbol + "_" + data.exchange;
        latestPrices_[key] = data;
        
        // Update price history - use last price (if available) or mid price
        double price = (data.last > 0) ? data.last : (data.bid + data.ask) / 2.0;
        priceHistory_[key].push_back(price);
        if (priceHistory_[key].size() > 1000) {
            priceHistory_[key].erase(priceHistory_[key].begin());
        }
        
        // Update positions with this symbol/exchange
        for (auto& [positionId, position] : activePositions_) {
            if (position.symbol == data.symbol && position.exchange == data.exchange) {
                double price = (data.last > 0) ? data.last : (data.bid + data.ask) / 2.0;
                position.currentPrice = price;
                position.notionalValue = position.size * price;
                position.lastUpdate = std::chrono::system_clock::now();
                
                // Update P&L
                updatePnL(positionId);
                
                // Update risk manager
                if (riskManager_) {
                    riskManager_->updatePosition(positionId, position);
                }
            }
        }
    }
    
    // Update portfolio value
    updatePortfolioValue();
}

void PositionManager::updatePnL(const std::string& positionId) {
    auto posIt = activePositions_.find(positionId);
    if (posIt == activePositions_.end()) {
        return;
    }
    
    const Position& position = posIt->second;
    
    std::lock_guard<std::mutex> pnlLock(pnlMutex_);
    auto pnlIt = positionPnL_.find(positionId);
    if (pnlIt == positionPnL_.end()) {
        return;
    }
    
    PnLData& pnl = pnlIt->second;
    
    // Calculate unrealized P&L
    if (position.entryPrice != 0.0) {
        double priceDiff = position.currentPrice - position.entryPrice;
        pnl.unrealizedPnL = priceDiff * position.size;
        pnl.totalPnL = pnl.realizedPnL + pnl.unrealizedPnL;
        
        // For synthetic positions, add funding/carry P&L
        if (position.isSynthetic) {
            // Simplified funding P&L calculation
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::hours>(now - position.openTime);
            double fundingRate = 0.0001; // 0.01% per hour (simplified)
            pnl.fundingPnL = position.notionalValue * fundingRate * duration.count();
            pnl.totalPnL += pnl.fundingPnL;
        }
    }
    
    pnl.lastUpdate = std::chrono::system_clock::now();
    pnl.isValid = true;
}

PnLData PositionManager::calculatePortfolioPnL() const {
    std::lock_guard<std::mutex> pnlLock(pnlMutex_);
    
    PnLData portfolioPnL;
    portfolioPnL.lastUpdate = std::chrono::system_clock::now();
    
    for (const auto& [positionId, pnl] : positionPnL_) {
        portfolioPnL.realizedPnL += pnl.realizedPnL;
        portfolioPnL.unrealizedPnL += pnl.unrealizedPnL;
        portfolioPnL.totalPnL += pnl.totalPnL;
        portfolioPnL.tradingPnL += pnl.tradingPnL;
        portfolioPnL.fundingPnL += pnl.fundingPnL;
        portfolioPnL.carryPnL += pnl.carryPnL;
    }
    
    portfolioPnL.isValid = true;
    return portfolioPnL;
}

bool PositionManager::checkPositionRisk(const Position& position) const {
    if (!riskManager_) {
        return true; // No risk manager, assume OK
    }
    
    // Check position size limits
    if (position.notionalValue > sizingParams_.maxPositionSize) {
        LOG_WARN("Position size exceeds limit: {:.2f} > {:.2f}", 
                                 position.notionalValue, sizingParams_.maxPositionSize);
        return false;
    }
    
    // Check leverage limits
    if (position.leverage > sizingParams_.maxLeverage) {
        LOG_WARN("Position leverage exceeds limit: {:.2f} > {:.2f}", 
                                 position.leverage, sizingParams_.maxLeverage);
        return false;
    }
    
    // Check concentration limits
    if (wouldExceedConcentrationLimit(position)) {
        LOG_WARN("Position would exceed concentration limit");
        return false;
    }
    
    // Check correlation limits
    double correlation = getCorrelationWithPortfolio(position.symbol, position.exchange);
    if (correlation > sizingParams_.maxCorrelation) {
        LOG_WARN("Position correlation too high: {:.2f} > {:.2f}", 
                                 correlation, sizingParams_.maxCorrelation);
        return false;
    }
    
    return true;
}

double PositionManager::getAvailableCapital(const std::string& strategy) const {
    std::lock_guard<std::mutex> lock(capitalMutex_);
    
    if (strategy.empty()) {
        return capitalAllocation_.availableCapital;
    }
    
    auto it = strategyCapital_.find(strategy);
    if (it != strategyCapital_.end()) {
        return std::max(0.0, it->second);
    }
    
    return 0.0;
}

bool PositionManager::allocateCapital(const std::string& strategy, double amount) {
    std::lock_guard<std::mutex> lock(capitalMutex_);
    
    if (amount <= 0.0 || amount > capitalAllocation_.availableCapital) {
        return false;
    }
    
    strategyCapital_[strategy] += amount;
    capitalAllocation_.availableCapital -= amount;
    capitalAllocation_.strategyAllocations[strategy] += amount;
    capitalAllocation_.lastUpdate = std::chrono::system_clock::now();
    
    LOG_INFO("Allocated ${:.2f} to strategy: {}", amount, strategy);
    return true;
}

double PositionManager::getPortfolioVolatility() const {
    if (portfolioValues_.size() < 2) {
        return 0.15; // Default volatility
    }
    
    std::vector<double> returns;
    for (size_t i = 1; i < portfolioValues_.size(); ++i) {
        if (portfolioValues_[i-1] != 0.0) {
            returns.push_back(std::log(portfolioValues_[i] / portfolioValues_[i-1]));
        }
    }
    
    return calculateVolatility(returns);
}

double PositionManager::calculateVolatility(const std::vector<double>& returns) const {
    if (returns.size() < 2) {
        return 0.0;
    }
    
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    
    for (double ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(variance / (returns.size() - 1));
}

void PositionManager::updatePortfolioValue() {
    PnLData portfolioPnL = calculatePortfolioPnL();
    
    std::lock_guard<std::mutex> lock(capitalMutex_);
    double currentValue = capitalAllocation_.totalCapital + portfolioPnL.totalPnL;
    
    portfolioValues_.push_back(currentValue);
    valuationTimes_.push_back(std::chrono::system_clock::now());
    
    // Keep only last 10000 values
    if (portfolioValues_.size() > 10000) {
        portfolioValues_.erase(portfolioValues_.begin());
        valuationTimes_.erase(valuationTimes_.begin());
    }
}

bool PositionManager::validatePosition(const Position& position) const {
    return !position.positionId.empty() && 
           !position.symbol.empty() && 
           !position.exchange.empty() &&
           position.size != 0.0 &&
           position.notionalValue >= 0.0;
}

bool PositionManager::wouldExceedConcentrationLimit(const Position& position) const {
    // Calculate current exposure by symbol
    std::unordered_map<std::string, double> symbolExposure;
    double totalExposure = 0.0;
    
    for (const auto& [id, pos] : activePositions_) {
        symbolExposure[pos.symbol] += std::abs(pos.notionalValue);
        totalExposure += std::abs(pos.notionalValue);
    }
    
    // Add new position
    symbolExposure[position.symbol] += std::abs(position.notionalValue);
    totalExposure += std::abs(position.notionalValue);
    
    if (totalExposure == 0.0) {
        return false;
    }
    
    // Check if any symbol exceeds concentration limit
    double maxConcentration = 0.25; // 25% max concentration
    for (const auto& [symbol, exposure] : symbolExposure) {
        if (exposure / totalExposure > maxConcentration) {
            return true;
        }
    }
    
    return false;
}

double PositionManager::getCorrelationWithPortfolio(const std::string& symbol, const std::string& exchange) const {
    // Simplified correlation calculation
    // In practice, this would calculate correlation with existing portfolio
    return 0.3; // Default moderate correlation
}

std::string PositionManager::generatePositionId(const std::string& symbol, const std::string& exchange) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return symbol + "_" + exchange + "_" + std::to_string(timestamp);
}

void PositionManager::logPositionEvent(const std::string& event, const std::string& positionId) const {
    if (positionId.empty()) {
        LOG_INFO("Position Event: {}", event);
    } else {
        LOG_INFO("Position Event: {} [{}]", event, positionId);
    }
}

std::string PositionManager::getPositionReport() const {
    std::ostringstream report;
    
    auto positions = activePositions_;
    auto portfolioPnL = calculatePortfolioPnL();
    auto capitalAlloc = getCapitalAllocation();
    
    report << "=== Position Manager Report ===\n";
    report << "Active Positions: " << positions.size() << "\n";
    report << "Total Capital: $" << std::fixed << std::setprecision(2) << capitalAlloc.totalCapital << "\n";
    report << "Allocated Capital: $" << capitalAlloc.allocatedCapital << "\n";
    report << "Available Capital: $" << capitalAlloc.availableCapital << "\n";
    report << "\n=== Portfolio P&L ===\n";
    report << "Realized P&L: $" << portfolioPnL.realizedPnL << "\n";
    report << "Unrealized P&L: $" << portfolioPnL.unrealizedPnL << "\n";
    report << "Total P&L: $" << portfolioPnL.totalPnL << "\n";
    report << "Funding P&L: $" << portfolioPnL.fundingPnL << "\n";
    
    report << "\n=== Active Positions ===\n";
    for (const auto& [id, position] : positions) {
        report << "ID: " << id << "\n";
        report << "  Symbol: " << position.symbol << "/" << position.exchange << "\n";
        report << "  Size: " << position.size << "\n";
        report << "  Notional: $" << position.notionalValue << "\n";
        report << "  Entry Price: $" << position.entryPrice << "\n";
        report << "  Current Price: $" << position.currentPrice << "\n";
        
        auto pnlIt = positionPnL_.find(id);
        if (pnlIt != positionPnL_.end()) {
            report << "  P&L: $" << pnlIt->second.totalPnL << "\n";
        }
        report << "\n";
    }
    
    return report.str();
}

CapitalAllocation PositionManager::getCapitalAllocation() const {
    std::lock_guard<std::mutex> lock(capitalMutex_);
    return capitalAllocation_;
}

void PositionManager::updateSizingParams(const PositionSizingParams& params) {
    sizingParams_ = params;
    LOG_INFO("Updated position sizing parameters");
}

PositionSizingParams PositionManager::getSizingParams() const {
    return sizingParams_;
}

int PositionManager::getActivePositionCount() const {
    return static_cast<int>(activePositions_.size());
}

std::vector<ArbitrageEngine::Position> PositionManager::getAllActivePositions() const {
    std::vector<Position> positions;
    for (const auto& [id, position] : activePositions_) {
        positions.push_back(position);
    }
    return positions;
}

} // namespace ArbitrageEngine
