#include "PricingEngine.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <chrono>

namespace arbitrage {
namespace core {

PricingEngine::PricingEngine(const MarketEnvironment& market_env) 
    : market_env_(market_env) {
    calculation_pipeline_ = std::make_unique<CalculationPipeline>();
    pricing_result_pool_ = std::make_unique<MemoryPool<PricingResult>>();
    
    LOG_INFO("PricingEngine initialized with {} interest rates", 
             market_env_.interest_rates.size());
}

PricingEngine::~PricingEngine() = default;

bool PricingEngine::registerInstrument(const InstrumentSpec& spec) {
    std::unique_lock<std::shared_mutex> lock(instruments_mutex_);
    
    instruments_[spec.symbol] = spec;
    
    LOG_INFO("Registered instrument: {} (type: {}, exchange: {})", 
             spec.symbol, static_cast<int>(spec.type), 
             data::exchangeToString(spec.exchange));
    
    return true;
}

bool PricingEngine::unregisterInstrument(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(instruments_mutex_);
    
    auto it = instruments_.find(symbol);
    if (it != instruments_.end()) {
        instruments_.erase(it);
        LOG_INFO("Unregistered instrument: {}", symbol);
        return true;
    }
    
    return false;
}

PricingResult PricingEngine::calculateSyntheticPrice(const std::string& symbol, 
                                                    const data::MarketData& market_data) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::shared_lock<std::shared_mutex> lock(instruments_mutex_);
    
    auto it = instruments_.find(symbol);
    if (it == instruments_.end()) {
        LOG_WARN("Instrument not registered: {}", symbol);
        PricingResult result;
        result.confidence = 0.0;
        updateStatistics(false, 0.0);
        return result;
    }
    
    const InstrumentSpec& spec = it->second;
    PricingResult result;
    
    try {
        switch (spec.type) {
            case InstrumentType::SPOT:
                result = priceSpot(spec, market_data);
                break;
            case InstrumentType::PERPETUAL_SWAP:
                result = pricePerpetualSwap(spec, market_data);
                break;
            case InstrumentType::FUTURES:
                result = priceFutures(spec, market_data);
                break;
            case InstrumentType::CALL_OPTION:
                result = priceOption(spec, market_data, true);
                break;
            case InstrumentType::PUT_OPTION:
                result = priceOption(spec, market_data, false);
                break;
            default:
                LOG_ERROR("Unknown instrument type for {}", symbol);
                result.confidence = 0.0;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double calculation_time_ms = duration.count() / 1000.0;
        
        updateStatistics(result.confidence > 0.0, calculation_time_ms);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Pricing error for {}: {}", symbol, e.what());
        result.confidence = 0.0;
        updateStatistics(false, 0.0);
    }
    
    return result;
}

std::unordered_map<std::string, PricingResult> 
PricingEngine::calculateAllSyntheticPrices(const data::MarketData& market_data) {
    std::unordered_map<std::string, PricingResult> results;
    
    std::shared_lock<std::shared_mutex> lock(instruments_mutex_);
    
    for (const auto& [symbol, spec] : instruments_) {
        results[symbol] = calculateSyntheticPrice(symbol, market_data);
    }
    
    return results;
}

void PricingEngine::updateMarketEnvironment(const MarketEnvironment& market_env) {
    market_env_ = market_env;
    LOG_INFO("Market environment updated");
}

const MarketEnvironment& PricingEngine::getMarketEnvironment() const {
    return market_env_;
}

void PricingEngine::setVolatility(const std::string& symbol, double volatility) {
    std::unique_lock<std::shared_mutex> lock(volatilities_mutex_);
    volatilities_[symbol] = volatility;
}

double PricingEngine::getVolatility(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(volatilities_mutex_);
    
    auto it = volatilities_.find(symbol);
    if (it != volatilities_.end()) {
        return it->second;
    }
    
    return market_env_.default_volatility;
}

PricingEngine::Statistics PricingEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return statistics_;
}

void PricingEngine::resetStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    statistics_ = Statistics{};
}

PricingResult PricingEngine::priceSpot(const InstrumentSpec& spec, const data::MarketData& market_data) {
    PricingResult result;
    result.pricing_model = "Spot";
    
    // For spot instruments, synthetic price equals the market price
    double spot_price = getUnderlyingPrice(spec.symbol, market_data);
    
    if (spot_price > 0.0) {
        result.synthetic_price = spot_price;
        result.components.base_price = spot_price;
        result.confidence = 1.0;
    } else {
        result.confidence = 0.0;
    }
    
    return result;
}

PricingResult PricingEngine::pricePerpetualSwap(const InstrumentSpec& spec, const data::MarketData& market_data) {
    PricingResult result;
    result.pricing_model = "Perpetual Swap";
    
    // Get underlying spot price
    double spot_price = getUnderlyingPrice(spec.underlying_symbol, market_data);
    if (spot_price <= 0.0) {
        result.confidence = 0.0;
        return result;
    }
    
    // Get funding rate
    double funding_rate = getFundingRate(spec.symbol, spec.exchange, market_data);
    
    // Calculate synthetic price using perpetual swap model
    result.synthetic_price = MathUtils::perpetualSyntheticPrice(spot_price, funding_rate);
    
    result.components.base_price = spot_price;
    result.components.funding_adjustment = result.synthetic_price - spot_price;
    result.confidence = 0.9; // High confidence for perpetual swaps
    
    return result;
}

PricingResult PricingEngine::priceFutures(const InstrumentSpec& spec, const data::MarketData& market_data) {
    PricingResult result;
    result.pricing_model = "Futures";
    
    // Get underlying spot price
    double spot_price = getUnderlyingPrice(spec.underlying_symbol, market_data);
    if (spot_price <= 0.0) {
        result.confidence = 0.0;
        return result;
    }
    
    // Calculate time to expiry
    double time_to_expiry = getTimeToExpiry(spec.expiry_time);
    if (time_to_expiry <= 0.0) {
        // Expired contract
        result.synthetic_price = spot_price;
        result.confidence = 1.0;
        return result;
    }
    
    // Get risk-free rate
    double risk_free_rate = getInterestRate(spec.quote_currency);
    
    // Calculate synthetic futures price
    result.synthetic_price = MathUtils::futuresSyntheticPrice(spot_price, risk_free_rate, time_to_expiry);
    
    result.components.base_price = spot_price;
    result.components.carry_cost = result.synthetic_price - spot_price;
    result.confidence = 0.85; // Good confidence for futures
    
    return result;
}

PricingResult PricingEngine::priceOption(const InstrumentSpec& spec, const data::MarketData& market_data, bool is_call) {
    PricingResult result;
    result.pricing_model = is_call ? "Black-Scholes Call" : "Black-Scholes Put";
    
    // Get underlying spot price
    double spot_price = getUnderlyingPrice(spec.underlying_symbol, market_data);
    if (spot_price <= 0.0) {
        result.confidence = 0.0;
        return result;
    }
    
    // Calculate time to expiry
    double time_to_expiry = getTimeToExpiry(spec.expiry_time);
    if (time_to_expiry <= 0.0) {
        // Expired option - intrinsic value only
        if (is_call) {
            result.synthetic_price = std::max(spot_price - spec.strike_price, 0.0);
        } else {
            result.synthetic_price = std::max(spec.strike_price - spot_price, 0.0);
        }
        result.confidence = 1.0;
        return result;
    }
    
    // Get parameters
    double risk_free_rate = getInterestRate(spec.quote_currency);
    double volatility = getVolatility(spec.underlying_symbol);
    
    // Calculate Black-Scholes price
    result.synthetic_price = MathUtils::blackScholesPrice(
        spot_price, spec.strike_price, time_to_expiry, 
        risk_free_rate, volatility, is_call
    );
    
    // Calculate Greeks
    result.greeks = MathUtils::calculateGreeks(
        spot_price, spec.strike_price, time_to_expiry,
        risk_free_rate, volatility, is_call
    );
    
    // Set components
    result.components.base_price = spot_price;
    double intrinsic_value = is_call ? 
        std::max(spot_price - spec.strike_price, 0.0) :
        std::max(spec.strike_price - spot_price, 0.0);
    result.components.time_value = result.synthetic_price - intrinsic_value;
    result.components.volatility_component = result.synthetic_price * 0.1; // Rough estimate
    
    result.confidence = 0.75; // Moderate confidence for options (depends on vol estimation)
    
    return result;
}

double PricingEngine::getUnderlyingPrice(const std::string& symbol, const data::MarketData& market_data) {
    // Try to get price from different sources in order of preference
    
    // 1. Try order book mid price
    for (const auto& [exchange, orderbook] : market_data.orderbooks) {
        if (orderbook.symbol == symbol && orderbook.isValid()) {
            return orderbook.getMidPrice();
        }
    }
    
    // 2. Try latest trade price
    for (const auto& [exchange, trade] : market_data.latest_trades) {
        if (trade.symbol == symbol) {
            return trade.price;
        }
    }
    
    // 3. Try ticker last price
    for (const auto& [exchange, ticker] : market_data.tickers) {
        if (ticker.symbol == symbol) {
            return ticker.last;
        }
    }
    
    return 0.0; // No price available
}

double PricingEngine::getFundingRate(const std::string& symbol, data::Exchange exchange, 
                                    const data::MarketData& market_data) {
    auto it = market_data.funding_rates.find(exchange);
    if (it != market_data.funding_rates.end() && it->second.symbol == symbol) {
        return it->second.funding_rate;
    }
    
    return 0.0; // Default funding rate
}

double PricingEngine::getTimeToExpiry(const std::chrono::system_clock::time_point& expiry_time) {
    auto now = std::chrono::system_clock::now();
    return MathUtils::yearsBetween(now, expiry_time);
}

double PricingEngine::getInterestRate(const std::string& currency) const {
    auto it = market_env_.interest_rates.find(currency);
    if (it != market_env_.interest_rates.end()) {
        return it->second;
    }
    
    return market_env_.risk_free_rate; // Default rate
}

void PricingEngine::updateStatistics(bool success, double calculation_time_ms) {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    ++statistics_.total_calculations;
    if (success) {
        ++statistics_.successful_calculations;
    }
    
    // Update average calculation time using exponential moving average
    const double alpha = 0.1;
    statistics_.average_calculation_time_ms = 
        alpha * calculation_time_ms + (1.0 - alpha) * statistics_.average_calculation_time_ms;
    
    statistics_.registered_instruments = instruments_.size();
}

// ArbitrageDetector implementation
ArbitrageDetector::ArbitrageDetector(std::shared_ptr<PricingEngine> pricing_engine,
                                   const Parameters& params)
    : pricing_engine_(pricing_engine), params_(params) {
    LOG_INFO("ArbitrageDetector initialized with min profit threshold: {:.4f}%", 
             params_.min_profit_threshold * 100);
}

std::vector<ArbitrageOpportunity> 
ArbitrageDetector::detectOpportunities(const data::MarketData& market_data) {
    std::vector<ArbitrageOpportunity> opportunities;
    
    if (params_.enable_synthetic_arbitrage) {
        auto spot_opps = detectSpotArbitrage(market_data);
        opportunities.insert(opportunities.end(), spot_opps.begin(), spot_opps.end());
        
        auto derivative_opps = detectDerivativeArbitrage(market_data);
        opportunities.insert(opportunities.end(), derivative_opps.begin(), derivative_opps.end());
    }
    
    // Filter opportunities based on criteria
    opportunities.erase(
        std::remove_if(opportunities.begin(), opportunities.end(),
            [this](const ArbitrageOpportunity& opp) {
                return opp.expected_profit_pct < params_.min_profit_threshold ||
                       opp.risk_score > params_.max_risk_score ||
                       opp.confidence < params_.min_confidence;
            }),
        opportunities.end()
    );
    
    // Sort by expected profit (descending)
    std::sort(opportunities.begin(), opportunities.end(),
        [](const ArbitrageOpportunity& a, const ArbitrageOpportunity& b) {
            return a.expected_profit_pct > b.expected_profit_pct;
        });
    
    return opportunities;
}

void ArbitrageDetector::updateParameters(const Parameters& params) {
    std::unique_lock<std::shared_mutex> lock(params_mutex_);
    params_ = params;
}

std::vector<ArbitrageOpportunity> 
ArbitrageDetector::detectSpotArbitrage(const data::MarketData& market_data) {
    std::vector<ArbitrageOpportunity> opportunities;
    
    // This is a simplified implementation
    // In practice, you would compare prices across exchanges and with synthetic prices
    
    return opportunities;
}

std::vector<ArbitrageOpportunity> 
ArbitrageDetector::detectDerivativeArbitrage(const data::MarketData& market_data) {
    std::vector<ArbitrageOpportunity> opportunities;
    
    // This is a simplified implementation
    // In practice, you would compare derivative prices with their synthetic equivalents
    
    return opportunities;
}

double ArbitrageDetector::calculateRiskScore(const ArbitrageOpportunity& opportunity) {
    // Simplified risk scoring
    // In practice, this would consider volatility, liquidity, correlation, etc.
    
    double risk_score = 0.0;
    
    // Higher deviation means higher risk
    for (const auto& leg : opportunity.legs) {
        risk_score += std::abs(leg.deviation) * 0.1;
    }
    
    // Cross-exchange arbitrage has higher risk
    if (opportunity.legs.size() > 1) {
        bool cross_exchange = false;
        data::Exchange first_exchange = opportunity.legs[0].exchange;
        for (size_t i = 1; i < opportunity.legs.size(); ++i) {
            if (opportunity.legs[i].exchange != first_exchange) {
                cross_exchange = true;
                break;
            }
        }
        if (cross_exchange) {
            risk_score += 0.2;
        }
    }
    
    return std::min(risk_score, 1.0);
}

} // namespace core
} // namespace arbitrage
