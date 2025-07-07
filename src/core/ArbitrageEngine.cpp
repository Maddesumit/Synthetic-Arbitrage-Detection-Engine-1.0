#include "ArbitrageEngine.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <set>

namespace arbitrage {
namespace core {

/**================================================================================================
 * CONSTRUCTOR & CONFIGURATION MANAGEMENT
 * 
 * The constructor initializes the engine with default or custom configuration parameters.
 * These parameters control detection thresholds, risk limits, and execution constraints.
 *================================================================================================*/

ArbitrageEngine::ArbitrageEngine(const ArbitrageConfig& config)
    : config_(config)
    , last_detection_cycle_(std::chrono::system_clock::now()) {
    
    // Log engine initialization
    utils::Logger::info("ArbitrageEngine initialized with config:");
    utils::Logger::info("  Min profit threshold: $" + std::to_string(config_.min_profit_threshold_usd));
    utils::Logger::info("  Min profit percentage: " + std::to_string(config_.min_profit_threshold_percent * 100) + "%");
    utils::Logger::info("  Min confidence score: " + std::to_string(config_.min_confidence_score));
    utils::Logger::info("  Max position size: $" + std::to_string(config_.max_position_size_usd));
}

void ArbitrageEngine::updateConfig(const ArbitrageConfig& config) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    config_ = config;
    utils::Logger::info("ArbitrageEngine configuration updated");
}

void ArbitrageEngine::start() {
    is_running_.store(true);
    utils::Logger::info("ArbitrageEngine started");
}

void ArbitrageEngine::stop() {
    is_running_.store(false);
    utils::Logger::info("ArbitrageEngine stopped");
}

/**================================================================================================
 * DATA UPDATE METHODS
 * 
 * These methods handle incoming market data and pricing results. We store the latest data
 * in thread-safe containers for use in opportunity detection.
 *================================================================================================*/

void ArbitrageEngine::updateMarketData(const std::vector<data::MarketDataPoint>& data) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    for (const auto& point : data) {
        // Create unique key for this market data point
        std::string key = point.symbol + "_" + point.exchange;
        latest_market_data_[key] = point;
    }
    
    // Update performance metrics
    performance_metrics_.last_update = std::chrono::system_clock::now();
}

void ArbitrageEngine::updatePricingResults(const std::vector<PricingResult>& results) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    for (const auto& result : results) {
        latest_pricing_results_[result.instrument_id] = result;
    }
}

/**================================================================================================
 * MAIN DETECTION ALGORITHM
 * 
 * This is the core method that orchestrates the entire arbitrage detection process.
 * It runs multiple detection strategies in parallel and consolidates the results.
 * 
 * Detection Flow:
 * 1. Run all detection strategies
 * 2. Validate each opportunity
 * 3. Filter based on thresholds
 * 4. Rank by profitability and risk
 * 5. Update performance metrics
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectOpportunities(
    const std::vector<data::MarketDataPoint>& market_data,
    const std::vector<PricingResult>& pricing_results) {
    
    if (!is_running_.load()) {
        return {};
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Update our internal data stores
    updateMarketData(market_data);
    updatePricingResults(pricing_results);
    
    std::vector<ArbitrageOpportunityExtended> all_opportunities;
    
    try {
        // Run all detection strategies
        // Each strategy looks for specific types of arbitrage opportunities
        
        // 1. Spot vs Perpetual Arbitrage
        // Look for price differences between spot and perpetual swap contracts
        auto spot_perp_ops = detectSpotPerpArbitrage();
        all_opportunities.insert(all_opportunities.end(), spot_perp_ops.begin(), spot_perp_ops.end());
        
        // 2. Funding Rate Arbitrage
        // Exploit differences in funding rates across exchanges
        auto funding_ops = detectFundingRateArbitrage();
        all_opportunities.insert(all_opportunities.end(), funding_ops.begin(), funding_ops.end());
        
        // 3. Cross-Exchange Arbitrage
        // Find price differences for the same instrument across exchanges
        auto cross_exchange_ops = detectCrossExchangeArbitrage();
        all_opportunities.insert(all_opportunities.end(), cross_exchange_ops.begin(), cross_exchange_ops.end());
        
        // 4. Basis Arbitrage
        // Exploit futures-spot basis differences
        auto basis_ops = detectBasisArbitrage();
        all_opportunities.insert(all_opportunities.end(), basis_ops.begin(), basis_ops.end());
        
        // 5. Volatility Arbitrage
        // Trade on implied vs realized volatility differences
        auto vol_ops = detectVolatilityArbitrage();
        all_opportunities.insert(all_opportunities.end(), vol_ops.begin(), vol_ops.end());
        
        // 6. Statistical Arbitrage
        // Mean reversion and momentum strategies
        auto stat_ops = detectStatisticalArbitrage();
        all_opportunities.insert(all_opportunities.end(), stat_ops.begin(), stat_ops.end());
        
        // Validate all opportunities
        std::vector<ArbitrageOpportunityExtended> validated_opportunities;
        for (auto& opportunity : all_opportunities) {
            if (validateOpportunity(opportunity)) {
                validated_opportunities.push_back(opportunity);
            }
        }
        
        // Filter based on configuration thresholds
        auto filtered_opportunities = filterOpportunities(validated_opportunities);
        
        // Rank by profitability and risk
        auto ranked_opportunities = rankOpportunities(filtered_opportunities);
        
        // Update performance metrics
        updatePerformanceMetrics(ranked_opportunities);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        utils::Logger::debug("Detection cycle completed: " + 
                          std::to_string(ranked_opportunities.size()) + " opportunities found in " +
                          std::to_string(duration.count()) + "ms");
        
        return ranked_opportunities;
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error in detectOpportunities: " + std::string(e.what()));
        return {};
    }
}

/**================================================================================================
 * SYNTHETIC CONSTRUCTION METHODS
 * 
 * These methods construct synthetic instruments from component parts. A synthetic instrument
 * replicates the payoff of another instrument using combinations of available instruments.
 * 
 * Examples:
 * - Synthetic Perpetual = Spot + Funding Rate Adjustment
 * - Synthetic Future = Spot + Interest Rate - Convenience Yield
 * - Synthetic Option = Delta-hedged portfolio + volatility component
 *================================================================================================*/

double ArbitrageEngine::constructSyntheticPerpetual(const std::string& symbol, const std::string& exchange) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        // Get spot price for the underlying
        double spot_price = getSpotPrice(symbol, exchange);
        if (spot_price <= 0) {
            return 0.0;
        }
        
        // Get current funding rate
        double funding_rate = getFundingRate(symbol, exchange);
        
        // Synthetic perpetual price = Spot price + funding adjustment
        // The funding adjustment accounts for the expected funding payments
        // over time, which affects the fair value of the perpetual contract
        
        // Funding is typically paid every 8 hours, so we annualize it
        double annual_funding_rate = funding_rate * (365.25 * 24 / 8);  // 8-hour periods per year
        
        // Apply a small adjustment based on funding rate
        // Positive funding rate means longs pay shorts, so perpetual should trade at slight discount
        double funding_adjustment = spot_price * (-annual_funding_rate * 0.01); // 1% of annual rate
        
        double synthetic_price = spot_price + funding_adjustment;
        
        utils::Logger::debug("Synthetic perpetual " + symbol + " on " + exchange + 
                           ": spot=" + std::to_string(spot_price) + 
                           ", funding_rate=" + std::to_string(funding_rate) + 
                           ", synthetic=" + std::to_string(synthetic_price));
        
        return synthetic_price;
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error constructing synthetic perpetual: " + std::string(e.what()));
        return 0.0;
    }
}

double ArbitrageEngine::constructSyntheticFuture(const std::string& symbol, const std::string& exchange,
                                               const std::chrono::system_clock::time_point& expiry) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        // Get spot price
        double spot_price = getSpotPrice(symbol, exchange);
        if (spot_price <= 0) {
            return 0.0;
        }
        
        // Calculate time to expiry in years
        auto now = std::chrono::system_clock::now();
        auto time_diff = expiry - now;
        double time_to_expiry = std::chrono::duration<double>(time_diff).count() / (365.25 * 24 * 3600);
        
        if (time_to_expiry <= 0) {
            return spot_price; // Expired contract should equal spot
        }
        
        // Use risk-free rate (simplified - in practice this would be more complex)
        double risk_free_rate = 0.05; // 5% annual
        
        // Synthetic future price = Spot * e^(r * T)
        // This is the cost-of-carry model for futures pricing
        double synthetic_price = spot_price * std::exp(risk_free_rate * time_to_expiry);
        
        utils::Logger::debug("Synthetic future " + symbol + " expiring in " + 
                           std::to_string(time_to_expiry) + " years: " + std::to_string(synthetic_price));
        
        return synthetic_price;
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error constructing synthetic future: " + std::string(e.what()));
        return 0.0;
    }
}

double ArbitrageEngine::constructSyntheticOption(const std::string& symbol, const std::string& exchange,
                                               double strike, const std::chrono::system_clock::time_point& expiry) {
    // Option pricing requires more complex modeling (Black-Scholes, etc.)
    // For now, we'll return a placeholder. In a full implementation, this would
    // use the PricingEngine's option pricing capabilities.
    
    utils::Logger::debug("Synthetic option construction not fully implemented yet");
    return 0.0;
}

/**================================================================================================
 * SPOT VS PERPETUAL ARBITRAGE DETECTION
 * 
 * This strategy looks for price differences between spot and perpetual swap contracts.
 * The theory is that perpetual swaps should trade close to spot prices, with small
 * differences due to funding rates and market sentiment.
 * 
 * Opportunity exists when: |Spot Price - Perpetual Price| > threshold
 * 
 * Execution:
 * - If Perpetual > Spot: Sell perpetual, buy spot
 * - If Spot > Perpetual: Buy perpetual, sell spot
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectSpotPerpArbitrage() {
    std::vector<ArbitrageOpportunityExtended> opportunities;
    
    try {
        // Get all available symbols
        std::set<std::string> symbols;
        for (const auto& [key, data] : latest_market_data_) {
            symbols.insert(data.symbol);
        }
        
        for (const std::string& symbol : symbols) {
            // Look for both spot and perpetual data for this symbol
            std::vector<std::string> exchanges = {"binance", "okx", "bybit"};
            
            for (const std::string& exchange : exchanges) {
                try {
                    double spot_price = getSpotPrice(symbol, exchange);
                    double perp_price = getPerpetualPrice(symbol, exchange);
                    
                    if (spot_price <= 0 || perp_price <= 0) {
                        continue; // Skip if we don't have both prices
                    }
                    
                    // Calculate the price difference
                    double price_diff = std::abs(spot_price - perp_price);
                    double avg_price = (spot_price + perp_price) / 2.0;
                    double percentage_spread = (price_diff / avg_price) * 100.0;
                    
                    // Check if this meets our minimum threshold
                    if (percentage_spread < config_.min_profit_threshold_percent) {
                        continue;
                    }
                    
                    // Create arbitrage opportunity
                    ArbitrageOpportunityExtended opportunity{};
                    opportunity.id = generateOpportunityId("SPOT_PERP", symbol);
                    opportunity.instrument_symbol = symbol;
                    opportunity.exchange_a = exchange;
                    opportunity.exchange_b = "synthetic";
                    opportunity.strategy_type = ArbitrageOpportunityExtended::StrategyType::SPOT_PERP_ARBITRAGE;
                    
                    // Price information
                    opportunity.price_a = spot_price;
                    opportunity.price_b = perp_price;
                    opportunity.price_difference = price_diff;
                    opportunity.percentage_spread = percentage_spread;
                    
                    // Calculate profitability (simplified)
                    opportunity.required_capital = std::min(config_.max_position_size_usd, 10000.0);
                    opportunity.expected_profit_usd = (opportunity.required_capital / avg_price) * price_diff;
                    opportunity.expected_profit_percent = percentage_spread;
                    
                    // Estimate costs
                    opportunity.execution_cost = calculateTransactionCosts(opportunity);
                    opportunity.slippage_cost = calculateSlippage(opportunity);
                    
                    // Net profit after costs
                    opportunity.expected_profit_usd -= (opportunity.execution_cost + opportunity.slippage_cost);
                    
                    // Risk metrics
                    opportunity.confidence_score = calculateConfidenceScore(opportunity);
                    opportunity.liquidity_score = calculateLiquidityScore(opportunity);
                    opportunity.volatility_risk = calculateVolatilityRisk(symbol);
                    
                    // Timing
                    opportunity.detected_at = std::chrono::system_clock::now();
                    opportunity.estimated_duration = std::chrono::milliseconds(30000); // 30 seconds
                    opportunity.time_to_expiry = std::chrono::milliseconds(60000);     // 1 minute
                    
                    // Create execution legs
                    if (perp_price > spot_price) {
                        // Sell perpetual, buy spot
                        opportunity.legs.push_back({exchange, symbol + "-PERP", "sell", 1.0, perp_price, 0.5});
                        opportunity.legs.push_back({exchange, symbol, "buy", 1.0, spot_price, 0.5});
                    } else {
                        // Buy perpetual, sell spot
                        opportunity.legs.push_back({exchange, symbol + "-PERP", "buy", 1.0, perp_price, 0.5});
                        opportunity.legs.push_back({exchange, symbol, "sell", 1.0, spot_price, 0.5});
                    }
                    
                    // Initial validation flags
                    opportunity.is_valid = true;
                    opportunity.is_executable = true;
                    
                    opportunities.push_back(opportunity);
                    
                } catch (const std::exception& e) {
                    utils::Logger::debug("Error processing " + symbol + " on " + exchange + ": " + e.what());
                }
            }
        }
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error in detectSpotPerpArbitrage: " + std::string(e.what()));
    }
    
    return opportunities;
}

/**================================================================================================
 * FUNDING RATE ARBITRAGE DETECTION
 * 
 * Funding rates vary across exchanges for perpetual swaps. When one exchange has
 * significantly higher/lower funding rates, there's an opportunity to:
 * 
 * 1. Go long on the exchange with negative funding (get paid)
 * 2. Go short on the exchange with positive funding (avoid paying)
 * 3. Hedge the market risk between positions
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectFundingRateArbitrage() {
    std::vector<ArbitrageOpportunityExtended> opportunities;
    
    try {
        // Group funding rates by symbol
        std::map<std::string, std::vector<std::pair<std::string, double>>> funding_by_symbol;
        
        for (const auto& [key, data] : latest_market_data_) {
            if (data.funding_rate != 0.0) {
                funding_by_symbol[data.symbol].push_back({data.exchange, data.funding_rate});
            }
        }
        
        // Look for funding rate differences
        for (const auto& [symbol, funding_data] : funding_by_symbol) {
            if (funding_data.size() < 2) {
                continue; // Need at least 2 exchanges to compare
            }
            
            // Find the exchanges with highest and lowest funding rates
            auto max_funding = *std::max_element(funding_data.begin(), funding_data.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            auto min_funding = *std::min_element(funding_data.begin(), funding_data.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            
            double funding_diff = max_funding.second - min_funding.second;
            
            // Check if the difference is significant (e.g., > 0.01% = 0.0001)
            if (std::abs(funding_diff) < 0.0001) {
                continue;
            }
            
            // Create opportunity
            ArbitrageOpportunityExtended opportunity{};
            opportunity.id = generateOpportunityId("FUNDING", symbol);
            opportunity.instrument_symbol = symbol;
            opportunity.exchange_a = max_funding.first;
            opportunity.exchange_b = min_funding.first;
            opportunity.strategy_type = ArbitrageOpportunityExtended::StrategyType::FUNDING_RATE_ARBITRAGE;
            
            // For funding arbitrage, we care about the funding rate difference
            opportunity.price_a = max_funding.second * 10000; // Convert to basis points for display
            opportunity.price_b = min_funding.second * 10000;
            opportunity.price_difference = std::abs(funding_diff) * 10000;
            opportunity.percentage_spread = std::abs(funding_diff) * 100;
            
            // Estimate profit (funding is typically paid every 8 hours)
            opportunity.required_capital = std::min(config_.max_position_size_usd, 20000.0);
            
            // Annual profit from funding rate difference (assuming 3 payments per day)
            double annual_profit_rate = std::abs(funding_diff) * 3 * 365;
            opportunity.expected_profit_usd = opportunity.required_capital * annual_profit_rate / 365; // Daily profit
            opportunity.expected_profit_percent = annual_profit_rate * 100;
            
            // Risk and confidence
            opportunity.confidence_score = 0.8; // Funding rates are relatively predictable
            opportunity.liquidity_score = calculateLiquidityScore(opportunity);
            opportunity.volatility_risk = calculateVolatilityRisk(symbol);
            
            // Timing
            opportunity.detected_at = std::chrono::system_clock::now();
            opportunity.estimated_duration = std::chrono::hours(8); // Until next funding
            opportunity.time_to_expiry = std::chrono::hours(8);
            
            // Execution strategy
            double position_size = opportunity.required_capital / getPerpetualPrice(symbol, max_funding.first);
            
            // Short the high-funding exchange, long the low-funding exchange
            opportunity.legs.push_back({max_funding.first, symbol + "-PERP", "sell", position_size, 0.0, 0.5});
            opportunity.legs.push_back({min_funding.first, symbol + "-PERP", "buy", position_size, 0.0, 0.5});
            
            opportunity.is_valid = true;
            opportunity.is_executable = true;
            
            opportunities.push_back(opportunity);
        }
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error in detectFundingRateArbitrage: " + std::string(e.what()));
    }
    
    return opportunities;
}

/**================================================================================================
 * CROSS-EXCHANGE ARBITRAGE DETECTION
 * 
 * This is the classic arbitrage strategy: buy on the exchange where price is low,
 * sell on the exchange where price is high, for the same instrument.
 * 
 * Risk: Execution timing, liquidity differences, withdrawal/deposit delays
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectCrossExchangeArbitrage() {
    std::vector<ArbitrageOpportunityExtended> opportunities;
    
    try {
        // Group prices by symbol
        std::map<std::string, std::vector<std::pair<std::string, data::MarketDataPoint>>> prices_by_symbol;
        
        for (const auto& [key, data] : latest_market_data_) {
            if (data.last > 0) {  // Only consider valid price data
                prices_by_symbol[data.symbol].push_back({data.exchange, data});
            }
        }
        
        // Look for price differences across exchanges
        for (const auto& [symbol, exchange_data] : prices_by_symbol) {
            if (exchange_data.size() < 2) {
                continue; // Need at least 2 exchanges
            }
            
            // Find highest and lowest prices
            auto max_price_pair = *std::max_element(exchange_data.begin(), exchange_data.end(),
                [](const auto& a, const auto& b) { return a.second.last < b.second.last; });
            auto min_price_pair = *std::min_element(exchange_data.begin(), exchange_data.end(),
                [](const auto& a, const auto& b) { return a.second.last < b.second.last; });
            
            double price_diff = max_price_pair.second.last - min_price_pair.second.last;
            double avg_price = (max_price_pair.second.last + min_price_pair.second.last) / 2.0;
            double percentage_spread = (price_diff / avg_price) * 100.0;
            
            // Check if spread is significant
            if (percentage_spread < config_.min_profit_threshold_percent) {
                continue;
            }
            
            // Create opportunity
            ArbitrageOpportunityExtended opportunity{};
            opportunity.id = generateOpportunityId("CROSS_EXCHANGE", symbol);
            opportunity.instrument_symbol = symbol;
            opportunity.exchange_a = min_price_pair.first;  // Buy here (low price)
            opportunity.exchange_b = max_price_pair.first;  // Sell here (high price)
            opportunity.strategy_type = ArbitrageOpportunityExtended::StrategyType::CROSS_EXCHANGE_ARBITRAGE;
            
            // Price information
            opportunity.price_a = min_price_pair.second.last;
            opportunity.price_b = max_price_pair.second.last;
            opportunity.price_difference = price_diff;
            opportunity.percentage_spread = percentage_spread;
            
            // Calculate profitability
            opportunity.required_capital = std::min(config_.max_position_size_usd, 15000.0);
            double position_size = opportunity.required_capital / opportunity.price_a;
            opportunity.expected_profit_usd = position_size * price_diff;
            opportunity.expected_profit_percent = percentage_spread;
            
            // Estimate costs (cross-exchange arbitrage has higher costs)
            opportunity.execution_cost = calculateTransactionCosts(opportunity) * 2; // Two exchanges
            opportunity.slippage_cost = calculateSlippage(opportunity) * 1.5;       // Higher slippage risk
            
            // Subtract costs
            opportunity.expected_profit_usd -= (opportunity.execution_cost + opportunity.slippage_cost);
            
            // Risk assessment
            opportunity.confidence_score = calculateConfidenceScore(opportunity) * 0.8; // Lower confidence
            opportunity.liquidity_score = std::min(
                min_price_pair.second.volume / 1000.0,  // Normalize volume to 0-1 scale
                max_price_pair.second.volume / 1000.0
            );
            opportunity.volatility_risk = calculateVolatilityRisk(symbol) * 1.2; // Higher volatility risk
            
            // Timing (cross-exchange arbitrage needs to be executed quickly)
            opportunity.detected_at = std::chrono::system_clock::now();
            opportunity.estimated_duration = std::chrono::milliseconds(10000); // 10 seconds
            opportunity.time_to_expiry = std::chrono::milliseconds(30000);     // 30 seconds
            
            // Execution legs
            opportunity.legs.push_back({
                opportunity.exchange_a, symbol, "buy", position_size, opportunity.price_a, 0.5
            });
            opportunity.legs.push_back({
                opportunity.exchange_b, symbol, "sell", position_size, opportunity.price_b, 0.5
            });
            
            opportunity.is_valid = true;
            opportunity.is_executable = true;
            
            opportunities.push_back(opportunity);
        }
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error in detectCrossExchangeArbitrage: " + std::string(e.what()));
    }
    
    return opportunities;
}

/**================================================================================================
 * BASIS ARBITRAGE DETECTION (Placeholder)
 * 
 * Basis arbitrage exploits the difference between futures and spot prices.
 * The basis = Futures Price - Spot Price should equal the cost of carry.
 * When it deviates significantly, there's an arbitrage opportunity.
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectBasisArbitrage() {
    // Implementation placeholder - would detect futures vs spot price discrepancies
    std::vector<ArbitrageOpportunityExtended> opportunities;
    utils::Logger::debug("Basis arbitrage detection not fully implemented");
    return opportunities;
}

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectVolatilityArbitrage() {
    // Implementation placeholder - would detect implied vs realized volatility differences
    std::vector<ArbitrageOpportunityExtended> opportunities;
    utils::Logger::debug("Volatility arbitrage detection not fully implemented");
    return opportunities;
}

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::detectStatisticalArbitrage() {
    // Implementation placeholder - would detect mean reversion opportunities
    std::vector<ArbitrageOpportunityExtended> opportunities;
    utils::Logger::debug("Statistical arbitrage detection not fully implemented");
    return opportunities;
}

/**================================================================================================
 * HELPER METHODS FOR PRICE RETRIEVAL
 * 
 * These methods extract specific price information from our market data cache.
 *================================================================================================*/

double ArbitrageEngine::getSpotPrice(const std::string& symbol, const std::string& exchange) {
    std::string key = symbol + "_" + exchange;
    auto it = latest_market_data_.find(key);
    if (it != latest_market_data_.end() && it->second.last > 0) {
        return it->second.last;
    }
    return 0.0;
}

double ArbitrageEngine::getPerpetualPrice(const std::string& symbol, const std::string& exchange) {
    std::string perp_key = symbol + "-PERP_" + exchange;
    auto it = latest_market_data_.find(perp_key);
    if (it != latest_market_data_.end() && it->second.last > 0) {
        return it->second.last;
    }
    
    // Fallback: try to construct synthetic perpetual
    return constructSyntheticPerpetual(symbol, exchange);
}

double ArbitrageEngine::getFundingRate(const std::string& symbol, const std::string& exchange) {
    std::string key = symbol + "_" + exchange;
    auto it = latest_market_data_.find(key);
    if (it != latest_market_data_.end()) {
        return it->second.funding_rate;
    }
    return 0.0;
}

/**================================================================================================
 * OPPORTUNITY VALIDATION METHODS
 * 
 * These methods perform comprehensive validation of arbitrage opportunities to ensure
 * they are realistic, executable, and meet our risk management criteria.
 *================================================================================================*/

bool ArbitrageEngine::validateOpportunity(ArbitrageOpportunityExtended& opportunity) {
    try {
        // Reset validation status
        opportunity.is_valid = false;
        opportunity.is_executable = false;
        opportunity.validation_notes.clear();
        
        std::vector<std::string> validation_issues;
        
        // 1. Check minimum profit thresholds
        if (opportunity.expected_profit_usd < config_.min_profit_threshold_usd) {
            validation_issues.push_back("Profit below USD threshold");
        }
        
        if (opportunity.percentage_spread < config_.min_profit_threshold_percent) {
            validation_issues.push_back("Profit below percentage threshold");
        }
        
        // 2. Check confidence score
        if (opportunity.confidence_score < config_.min_confidence_score) {
            validation_issues.push_back("Confidence score too low");
        }
        
        // 3. Validate liquidity requirements
        if (!checkLiquidityRequirements(opportunity)) {
            validation_issues.push_back("Insufficient liquidity");
        }
        
        // 4. Check risk limits
        if (!checkRiskLimits(opportunity)) {
            validation_issues.push_back("Risk limits exceeded");
        }
        
        // 5. Check execution feasibility
        if (!checkExecutionFeasibility(opportunity)) {
            validation_issues.push_back("Execution not feasible");
        }
        
        // 6. Validate pricing data freshness
        auto now = std::chrono::system_clock::now();
        auto data_age = std::chrono::duration_cast<std::chrono::milliseconds>(now - opportunity.detected_at);
        if (data_age > std::chrono::milliseconds(5000)) { // 5 seconds
            validation_issues.push_back("Market data too stale");
        }
        
        // 7. Check for reasonable execution legs
        if (opportunity.legs.empty()) {
            validation_issues.push_back("No execution legs defined");
        }
        
        // Compile validation notes
        opportunity.validation_notes = "";
        for (size_t i = 0; i < validation_issues.size(); ++i) {
            if (i > 0) opportunity.validation_notes += "; ";
            opportunity.validation_notes += validation_issues[i];
        }
        
        // Opportunity is valid if there are no validation issues
        opportunity.is_valid = validation_issues.empty();
        opportunity.is_executable = opportunity.is_valid;
        
        if (opportunity.is_valid) {
            logOpportunity(opportunity);
        }
        
        return opportunity.is_valid;
        
    } catch (const std::exception& e) {
        utils::Logger::error("Error validating opportunity: " + std::string(e.what()));
        opportunity.is_valid = false;
        opportunity.is_executable = false;
        opportunity.validation_notes = "Validation error: " + std::string(e.what());
        return false;
    }
}

bool ArbitrageEngine::checkLiquidityRequirements(const ArbitrageOpportunityExtended& opportunity) {
    // Check if there's enough liquidity to execute the trade
    if (opportunity.liquidity_score < config_.min_liquidity_score) {
        return false;
    }
    
    // Check if position size is reasonable relative to market liquidity
    if (opportunity.required_capital > config_.max_position_size_usd) {
        return false;
    }
    
    return true;
}

bool ArbitrageEngine::checkRiskLimits(const ArbitrageOpportunityExtended& opportunity) {
    // Check maximum position size
    if (opportunity.required_capital > config_.max_position_size_usd) {
        return false;
    }
    
    // Check volatility risk
    if (opportunity.volatility_risk > 0.5) { // 50% volatility threshold
        return false;
    }
    
    // Check correlation risk for multi-leg strategies
    double correlation_risk = calculateCorrelationRisk(opportunity);
    if (correlation_risk > config_.max_correlation_risk) {
        return false;
    }
    
    return true;
}

bool ArbitrageEngine::checkExecutionFeasibility(const ArbitrageOpportunityExtended& opportunity) {
    // Check if opportunity duration is sufficient
    if (opportunity.estimated_duration < config_.min_opportunity_duration) {
        return false;
    }
    
    // Check if we have market data for all required exchanges/instruments
    for (const auto& leg : opportunity.legs) {
        std::string key = leg.instrument + "_" + leg.exchange;
        if (latest_market_data_.find(key) == latest_market_data_.end()) {
            return false;
        }
    }
    
    return true;
}

/**================================================================================================
 * RISK CALCULATION METHODS
 * 
 * These methods calculate various risk metrics for opportunities to help with
 * validation and ranking decisions.
 *================================================================================================*/

double ArbitrageEngine::calculateExpectedProfit(const ArbitrageOpportunityExtended& opportunity) {
    // Basic profit calculation - already done in detection, but we can refine here
    double gross_profit = opportunity.expected_profit_usd;
    double total_costs = opportunity.execution_cost + opportunity.slippage_cost;
    return std::max(0.0, gross_profit - total_costs);
}

double ArbitrageEngine::calculateRiskAdjustedReturn(const ArbitrageOpportunityExtended& opportunity) {
    double expected_profit = calculateExpectedProfit(opportunity);
    
    // Risk adjustment factors
    double volatility_adjustment = 1.0 - (opportunity.volatility_risk * 0.5);
    double liquidity_adjustment = opportunity.liquidity_score;
    double confidence_adjustment = opportunity.confidence_score;
    
    // Combined risk adjustment
    double risk_adjustment = volatility_adjustment * liquidity_adjustment * confidence_adjustment;
    
    return expected_profit * risk_adjustment;
}

double ArbitrageEngine::calculateConfidenceScore(const ArbitrageOpportunityExtended& opportunity) {
    double base_confidence = 0.8;
    
    // Adjust based on price spread magnitude
    if (opportunity.percentage_spread > 1.0) {
        base_confidence *= 0.9; // Very large spreads might be suspicious
    } else if (opportunity.percentage_spread > 0.5) {
        base_confidence *= 1.1; // Good sized spreads are more confident
    }
    
    // Adjust based on data freshness
    auto now = std::chrono::system_clock::now();
    auto data_age = std::chrono::duration_cast<std::chrono::milliseconds>(now - opportunity.detected_at);
    if (data_age > std::chrono::milliseconds(1000)) {
        base_confidence *= 0.9; // Reduce confidence for older data
    }
    
    // Adjust based on strategy type
    switch (opportunity.strategy_type) {
        case ArbitrageOpportunityExtended::StrategyType::CROSS_EXCHANGE_ARBITRAGE:
            base_confidence *= 0.8; // Higher execution risk
            break;
        case ArbitrageOpportunityExtended::StrategyType::FUNDING_RATE_ARBITRAGE:
            base_confidence *= 1.1; // More predictable
            break;
        default:
            break;
    }
    
    return std::min(1.0, std::max(0.0, base_confidence));
}

double ArbitrageEngine::calculateLiquidityScore(const ArbitrageOpportunityExtended& opportunity) {
    double total_liquidity_score = 0.0;
    int valid_legs = 0;
    
    for (const auto& leg : opportunity.legs) {
        std::string key = leg.instrument + "_" + leg.exchange;
        auto it = latest_market_data_.find(key);
        if (it != latest_market_data_.end()) {
            // Simple liquidity score based on volume
            double leg_liquidity = std::min(1.0, it->second.volume / 1000000.0); // Normalize to $1M
            total_liquidity_score += leg_liquidity;
            valid_legs++;
        }
    }
    
    return valid_legs > 0 ? total_liquidity_score / valid_legs : 0.0;
}

double ArbitrageEngine::calculateVolatilityRisk(const std::string& symbol) {
    // Simplified volatility calculation
    // In practice, this would use historical price data to calculate realized volatility
    
    // Default volatility assumptions based on asset type
    if (symbol.find("BTC") != std::string::npos) {
        return 0.4; // 40% annual volatility for Bitcoin
    } else if (symbol.find("ETH") != std::string::npos) {
        return 0.5; // 50% annual volatility for Ethereum
    } else {
        return 0.6; // 60% for other altcoins
    }
}

double ArbitrageEngine::calculateCorrelationRisk(const ArbitrageOpportunityExtended& opportunity) {
    // Simplified correlation risk calculation
    // In practice, this would calculate correlation between the instruments in different legs
    
    if (opportunity.legs.size() <= 1) {
        return 0.0; // No correlation risk for single-leg strategies
    }
    
    // For now, assume moderate correlation for similar instruments
    return 0.6; // 60% correlation
}

double ArbitrageEngine::calculateLiquidityRisk(const ArbitrageOpportunityExtended& opportunity) {
    return 1.0 - opportunity.liquidity_score;
}

/**================================================================================================
 * COST CALCULATION METHODS
 * 
 * These methods estimate the various costs associated with executing arbitrage trades.
 *================================================================================================*/

double ArbitrageEngine::calculateTransactionCosts(const ArbitrageOpportunityExtended& opportunity) {
    double total_cost = 0.0;
    
    // Estimate trading fees for each leg
    for (const auto& leg : opportunity.legs) {
        double notional = leg.quantity * leg.price;
        if (notional == 0.0) {
            notional = opportunity.required_capital * leg.weight;
        }
        
        // Typical trading fees: 0.1% for maker, 0.1% for taker
        double fee_rate = 0.001; // 0.1%
        total_cost += notional * fee_rate;
    }
    
    return total_cost;
}

double ArbitrageEngine::calculateSlippage(const ArbitrageOpportunityExtended& opportunity) {
    // Estimate slippage based on liquidity and position size
    double total_slippage = 0.0;
    
    for (const auto& leg : opportunity.legs) {
        double notional = leg.quantity * leg.price;
        if (notional == 0.0) {
            notional = opportunity.required_capital * leg.weight;
        }
        
        // Slippage increases with position size and decreases with liquidity
        double base_slippage = 0.0005; // 0.05% base slippage
        double liquidity_factor = 1.0 / std::max(0.1, opportunity.liquidity_score);
        double size_factor = std::sqrt(notional / 10000.0); // Scale with square root of size
        
        double leg_slippage = notional * base_slippage * liquidity_factor * size_factor;
        total_slippage += leg_slippage;
    }
    
    return total_slippage;
}

/**================================================================================================
 * OPPORTUNITY FILTERING AND RANKING
 * 
 * These methods filter opportunities based on configuration thresholds and rank them
 * by attractiveness (risk-adjusted returns).
 *================================================================================================*/

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::filterOpportunities(
    const std::vector<ArbitrageOpportunityExtended>& opportunities) {
    
    std::vector<ArbitrageOpportunityExtended> filtered;
    
    for (const auto& opportunity : opportunities) {
        // Only include valid opportunities
        if (!opportunity.is_valid) {
            continue;
        }
        
        // Apply configuration filters
        if (opportunity.expected_profit_usd < config_.min_profit_threshold_usd) {
            continue;
        }
        
        if (opportunity.percentage_spread < config_.min_profit_threshold_percent) {
            continue;
        }
        
        if (opportunity.confidence_score < config_.min_confidence_score) {
            continue;
        }
        
        if (opportunity.liquidity_score < config_.min_liquidity_score) {
            continue;
        }
        
        filtered.push_back(opportunity);
    }
    
    return filtered;
}

std::vector<ArbitrageOpportunityExtended> ArbitrageEngine::rankOpportunities(
    std::vector<ArbitrageOpportunityExtended> opportunities) {
    
    // Calculate risk-adjusted returns for all opportunities
    for (auto& opportunity : opportunities) {
        opportunity.risk_adjusted_return = calculateRiskAdjustedReturn(opportunity);
    }
    
    // Sort by risk-adjusted return (descending)
    std::sort(opportunities.begin(), opportunities.end(),
        [](const ArbitrageOpportunityExtended& a, const ArbitrageOpportunityExtended& b) {
            return a.risk_adjusted_return > b.risk_adjusted_return;
        });
    
    return opportunities;
}

/**================================================================================================
 * UTILITY AND HELPER METHODS
 *================================================================================================*/

std::string ArbitrageEngine::generateOpportunityId(const std::string& strategy, const std::string& symbol) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    // Create a simple random suffix
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::stringstream ss;
    ss << strategy << "_" << symbol << "_" << timestamp << "_" << dis(gen);
    return ss.str();
}

void ArbitrageEngine::updatePerformanceMetrics(const std::vector<ArbitrageOpportunityExtended>& opportunities) {
    performance_metrics_.detection_cycles.fetch_add(1);
    performance_metrics_.opportunities_detected.fetch_add(opportunities.size());
    
    size_t valid_count = std::count_if(opportunities.begin(), opportunities.end(),
        [](const ArbitrageOpportunityExtended& op) { return op.is_valid; });
    performance_metrics_.opportunities_validated.fetch_add(valid_count);
    
    // Calculate total expected profit
    double total_profit = 0.0;
    for (const auto& opportunity : opportunities) {
        if (opportunity.is_valid) {
            total_profit += opportunity.expected_profit_usd;
        }
    }
    performance_metrics_.total_expected_profit.store(
        performance_metrics_.total_expected_profit.load() + total_profit);
    
    // Update detection latency
    auto now = std::chrono::system_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_detection_cycle_);
    performance_metrics_.avg_detection_latency_ms.store(latency.count());
    last_detection_cycle_ = now;
    
    performance_metrics_.last_update = now;
}

void ArbitrageEngine::resetPerformanceMetrics() {
    performance_metrics_.opportunities_detected.store(0);
    performance_metrics_.opportunities_validated.store(0);
    performance_metrics_.detection_cycles.store(0);
    performance_metrics_.avg_detection_latency_ms.store(0.0);
    performance_metrics_.total_expected_profit.store(0.0);
    performance_metrics_.last_update = std::chrono::system_clock::now();
}

void ArbitrageEngine::logOpportunity(const ArbitrageOpportunityExtended& opportunity) {
    std::stringstream ss;
    ss << "Arbitrage Opportunity: " << opportunity.id << "\n"
       << "  Symbol: " << opportunity.instrument_symbol << "\n"
       << "  Strategy: ";
    
    switch (opportunity.strategy_type) {
        case ArbitrageOpportunityExtended::StrategyType::SPOT_PERP_ARBITRAGE:
            ss << "Spot-Perpetual";
            break;
        case ArbitrageOpportunityExtended::StrategyType::FUNDING_RATE_ARBITRAGE:
            ss << "Funding Rate";
            break;
        case ArbitrageOpportunityExtended::StrategyType::CROSS_EXCHANGE_ARBITRAGE:
            ss << "Cross-Exchange";
            break;
        default:
            ss << "Other";
    }
    
    ss << "\n  Expected Profit: $" << std::fixed << std::setprecision(2) << opportunity.expected_profit_usd
       << " (" << std::setprecision(3) << opportunity.percentage_spread << "%)\n"
       << "  Confidence: " << std::setprecision(2) << (opportunity.confidence_score * 100) << "%\n"
       << "  Required Capital: $" << std::setprecision(0) << opportunity.required_capital;
    
    utils::Logger::info(ss.str());
}

} // namespace core
} // namespace arbitrage
