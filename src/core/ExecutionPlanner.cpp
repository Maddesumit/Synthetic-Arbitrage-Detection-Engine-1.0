#include "ExecutionPlanner.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace arbitrage {
namespace core {

ExecutionPlanner::ExecutionPlanner(const PlanningParameters& params) 
    : params_(params) {
    utils::Logger::info("ExecutionPlanner initialized");
}

std::unique_ptr<ExecutionPlan> ExecutionPlanner::createExecutionPlan(const RankedOpportunity& opportunity) {
    auto plan = std::make_unique<ExecutionPlan>();
    plan->plan_id = generatePlanId();
    plan->opportunity = opportunity;
    plan->created_at = std::chrono::system_clock::now();
    
    // Calculate optimal sizing
    auto sizing = calculateOptimalSizing(opportunity.opportunity, plan->sizing_strategy);
    
    // Create execution orders for each leg
    for (size_t i = 0; i < opportunity.opportunity.legs.size(); ++i) {
        const auto& leg = opportunity.opportunity.legs[i];
        
        ExecutionOrder order;
        order.order_id = generateOrderId();
        order.symbol = leg.symbol;
        order.exchange = leg.exchange;
        order.action = leg.action;
        order.quantity = (i < sizing.size()) ? sizing[i] : 1.0;
        order.target_price = leg.price;
        
        // Set limit price with slippage tolerance
        double slippage_factor = (order.action == "BUY") ? 1.0 + params_.default_slippage_tolerance 
                                                         : 1.0 - params_.default_slippage_tolerance;
        order.limit_price = order.target_price * slippage_factor;
        
        // Set stop price for risk management
        double stop_factor = (order.action == "BUY") ? 1.0 + plan->stop_loss_threshold 
                                                     : 1.0 - plan->stop_loss_threshold;
        order.stop_price = order.target_price * stop_factor;
        
        // Set execution timing
        order.planned_execution_time = plan->created_at + std::chrono::milliseconds(i * 100);
        
        plan->orders.push_back(order);
    }
    
    // Estimate execution costs
    auto cost_estimate = estimateExecutionCosts(*plan);
    plan->estimated_total_cost = cost_estimate.total_cost;
    plan->estimated_slippage = cost_estimate.slippage;
    plan->estimated_market_impact = cost_estimate.market_impact;
    
    // Set capital requirements
    plan->max_total_capital = opportunity.opportunity.required_capital;
    
    // Validate the plan
    auto validation = validateExecutionPlan(*plan);
    if (validation.is_valid) {
        plan->status = ExecutionPlan::Status::READY_TO_EXECUTE;
        utils::Logger::info("Created execution plan: " + plan->plan_id);
    } else {
        plan->status = ExecutionPlan::Status::FAILED;
        utils::Logger::warn("Execution plan validation failed: " + plan->plan_id);
        for (const auto& error : validation.errors) {
            utils::Logger::error("Validation error: " + error);
        }
    }
    
    return plan;
}

std::vector<std::unique_ptr<ExecutionPlan>> ExecutionPlanner::optimizeExecutionSequence(
    const std::vector<RankedOpportunity>& opportunities) {
    
    std::vector<std::unique_ptr<ExecutionPlan>> plans;
    double total_capital_used = 0.0;
    
    for (const auto& opportunity : opportunities) {
        // Check if we have enough capital remaining
        if (total_capital_used + opportunity.opportunity.required_capital > 
            params_.max_single_trade_capital * params_.max_total_capital_utilization) {
            utils::Logger::warn("Capital limit reached, skipping opportunity");
            continue;
        }
        
        auto plan = createExecutionPlan(opportunity);
        if (plan && plan->status == ExecutionPlan::Status::READY_TO_EXECUTE) {
            // Adjust timing to avoid conflicts
            auto delay = std::chrono::milliseconds(plans.size() * 1000); // 1 second between plans
            plan->planned_start_time = std::chrono::system_clock::now() + delay;
            
            total_capital_used += opportunity.opportunity.required_capital;
            plans.push_back(std::move(plan));
        }
    }
    
    utils::Logger::info("Optimized execution sequence for " + std::to_string(plans.size()) + " opportunities");
    return plans;
}

std::vector<double> ExecutionPlanner::calculateOptimalSizing(
    const ArbitrageOpportunity& opportunity, SizingStrategy strategy) {
    
    std::vector<double> sizing;
    double base_size = 0.0;
    
    switch (strategy) {
        case SizingStrategy::KELLY_CRITERION:
            base_size = calculateKellySizing(opportunity);
            break;
        case SizingStrategy::VOLATILITY_ADJUSTED:
            base_size = calculateVolatilityAdjustedSizing(opportunity);
            break;
        case SizingStrategy::LIQUIDITY_CONSTRAINED:
            base_size = calculateLiquidityConstrainedSizing(opportunity);
            break;
        case SizingStrategy::RISK_PARITY:
            base_size = calculateRiskParitySizing(opportunity);
            break;
        case SizingStrategy::FIXED_SIZE:
        default:
            base_size = params_.min_position_size;
            break;
    }
    
    // Apply constraints
    base_size = std::max(params_.min_position_size, 
                        std::min(params_.max_position_size, base_size));
    
    // Create sizing for each leg
    for (const auto& leg : opportunity.legs) {
        double leg_size = base_size / leg.price; // Convert to quantity
        sizing.push_back(leg_size);
    }
    
    return sizing;
}

std::chrono::system_clock::time_point ExecutionPlanner::calculateOptimalTiming(
    const ExecutionPlan& plan,
    const std::vector<data::MarketDataPoint>& market_data) {
    
    auto now = std::chrono::system_clock::now();
    
    switch (plan.timing_strategy) {
        case ExecutionTiming::IMMEDIATE:
            return now;
            
        case ExecutionTiming::OPTIMAL_DELAY: {
            // Simple delay based on market volatility
            auto delay = calculateExecutionDelay(plan);
            return now + delay;
        }
        
        case ExecutionTiming::MARKET_CONDITION_BASED: {
            // Wait for favorable market conditions
            if (isOptimalExecutionTime(market_data)) {
                return now;
            } else {
                return now + std::chrono::seconds(5); // Wait 5 seconds
            }
        }
        
        case ExecutionTiming::LIQUIDITY_BASED:
        default:
            // Simple immediate execution for now
            return now;
    }
}

void ExecutionPlanner::handlePartialFill(ExecutionPlan& plan, const ExecutionOrder& partially_filled_order) {
    utils::Logger::info("Handling partial fill for order: " + partially_filled_order.order_id);
    
    // Find the order in the plan
    for (auto& order : plan.orders) {
        if (order.order_id == partially_filled_order.order_id) {
            // Update the order with partial fill information
            order.executed_quantity = partially_filled_order.executed_quantity;
            order.executed_price = partially_filled_order.executed_price;
            order.execution_time = partially_filled_order.execution_time;
            
            // Calculate remaining quantity
            double remaining = order.quantity - order.executed_quantity;
            
            if (remaining > params_.min_position_size) {
                // Create a new order for the remaining quantity
                ExecutionOrder remaining_order = order;
                remaining_order.order_id = generateOrderId();
                remaining_order.quantity = remaining;
                remaining_order.is_executed = false;
                remaining_order.executed_quantity = 0.0;
                remaining_order.planned_execution_time = std::chrono::system_clock::now() + 
                                                        std::chrono::milliseconds(100);
                
                plan.orders.push_back(remaining_order);
                plan.status = ExecutionPlan::Status::PARTIALLY_FILLED;
            } else {
                // Consider the order complete if remaining is too small
                order.quantity = order.executed_quantity;
                order.is_executed = true;
            }
            
            break;
        }
    }
    
    // Check if all orders are complete
    bool all_complete = true;
    for (const auto& order : plan.orders) {
        if (!order.is_executed) {
            all_complete = false;
            break;
        }
    }
    
    if (all_complete) {
        plan.status = ExecutionPlan::Status::COMPLETED;
        plan.completion_time = std::chrono::system_clock::now();
    }
}

ExecutionPlanner::ExecutionCostEstimate ExecutionPlanner::estimateExecutionCosts(const ExecutionPlan& plan) {
    ExecutionCostEstimate estimate;
    
    for (const auto& order : plan.orders) {
        // Transaction costs (simplified - typically 0.1% for crypto exchanges)
        double transaction_cost = order.quantity * order.target_price * 0.001;
        estimate.transaction_costs += transaction_cost;
        
        // Market impact
        double market_impact = estimateMarketImpact(order);
        estimate.market_impact += market_impact;
        
        // Slippage
        double slippage = estimateSlippage(order);
        estimate.slippage += slippage;
    }
    
    // Opportunity cost (time value)
    estimate.opportunity_cost = plan.opportunity.opportunity.expected_profit_pct * 
                               plan.max_total_capital * 0.01; // 1% of potential profit
    
    estimate.total_cost = estimate.transaction_costs + estimate.market_impact + 
                         estimate.slippage + estimate.opportunity_cost;
    
    return estimate;
}

ExecutionPlanner::ValidationResult ExecutionPlanner::validateExecutionPlan(const ExecutionPlan& plan) {
    ValidationResult result;
    result.is_valid = true;
    result.confidence_score = 1.0;
    
    // Check capital constraints
    if (!checkCapitalConstraints(plan)) {
        result.errors.push_back("Capital constraints violated");
        result.is_valid = false;
    }
    
    // Check risk limits
    if (!checkRiskLimits(plan)) {
        result.errors.push_back("Risk limits exceeded");
        result.is_valid = false;
    }
    
    // Validate orders
    for (const auto& order : plan.orders) {
        if (order.quantity <= 0) {
            result.errors.push_back("Invalid order quantity: " + order.order_id);
            result.is_valid = false;
        }
        
        if (order.target_price <= 0) {
            result.errors.push_back("Invalid target price: " + order.order_id);
            result.is_valid = false;
        }
    }
    
    // Check minimum profit threshold
    if (plan.opportunity.opportunity.expected_profit_pct < 0.001) { // 0.1%
        result.warnings.push_back("Low expected profit margin");
        result.confidence_score *= 0.8;
    }
    
    // Check execution timing
    auto estimated_duration = std::chrono::milliseconds(plan.orders.size() * 200);
    if (estimated_duration > params_.max_execution_window) {
        result.warnings.push_back("Execution window may be too long");
        result.confidence_score *= 0.9;
    }
    
    return result;
}

// Private helper methods implementation

double ExecutionPlanner::calculateKellySizing(const ArbitrageOpportunity& opportunity) {
    // Kelly criterion: f = (bp - q) / b
    // where f = fraction to bet, b = odds, p = probability of win, q = probability of loss
    
    double win_probability = opportunity.confidence;
    double loss_probability = 1.0 - win_probability;
    double win_amount = opportunity.expected_profit_pct;
    double loss_amount = opportunity.risk_score; // Simplified
    
    if (loss_amount == 0.0) loss_amount = 0.01; // Avoid division by zero
    
    double kelly_fraction = (win_probability * win_amount - loss_probability) / loss_amount;
    
    // Apply conservative scaling
    kelly_fraction *= params_.kelly_fraction;
    
    // Convert to position size
    return std::max(params_.min_position_size, 
                   kelly_fraction * params_.max_single_trade_capital);
}

double ExecutionPlanner::calculateVolatilityAdjustedSizing(const ArbitrageOpportunity& opportunity) {
    // Size inversely proportional to risk/volatility
    double base_size = params_.max_position_size;
    double volatility_adjustment = 1.0 / (1.0 + opportunity.risk_score);
    
    return base_size * volatility_adjustment;
}

double ExecutionPlanner::calculateLiquidityConstrainedSizing(const ArbitrageOpportunity& opportunity) {
    // Size based on estimated available liquidity
    double estimated_liquidity = 0.0;
    
    for (const auto& leg : opportunity.legs) {
        // Simplified liquidity estimation
        estimated_liquidity += leg.price * 1000.0; // Assume $1000 per price level
    }
    
    // Use a fraction of estimated liquidity
    return std::min(params_.max_position_size, estimated_liquidity * 0.1);
}

double ExecutionPlanner::calculateRiskParitySizing(const ArbitrageOpportunity& opportunity) {
    // Equal risk contribution from each position
    double target_risk = params_.max_portfolio_var / opportunity.legs.size();
    double position_risk = opportunity.risk_score;
    
    if (position_risk == 0.0) position_risk = 0.01;
    
    return target_risk / position_risk * params_.max_single_trade_capital;
}

double ExecutionPlanner::estimateMarketImpact(const ExecutionOrder& order) {
    // Simplified market impact model
    double order_value = order.quantity * order.target_price;
    
    // Market impact increases with order size
    double impact_factor = std::min(0.001, order_value / 1000000.0); // 0.1% max impact
    
    return order_value * impact_factor;
}

double ExecutionPlanner::estimateSlippage(const ExecutionOrder& order) {
    // Simplified slippage estimation
    return order.quantity * order.target_price * params_.default_slippage_tolerance;
}

bool ExecutionPlanner::isOptimalExecutionTime(const std::vector<data::MarketDataPoint>& market_data) {
    // Simplified check - could include volatility, spread analysis, etc.
    if (market_data.empty()) return true;
    
    // Check if spreads are reasonable
    for (const auto& data : market_data) {
        double spread = (data.ask - data.bid) / data.last;
        if (spread > 0.005) { // 0.5% spread threshold
            return false;
        }
    }
    
    return true;
}

std::chrono::milliseconds ExecutionPlanner::calculateExecutionDelay(const ExecutionPlan& plan) {
    // Simple delay based on number of orders
    return std::chrono::milliseconds(plan.orders.size() * 100);
}

bool ExecutionPlanner::checkCapitalConstraints(const ExecutionPlan& plan) {
    return plan.max_total_capital <= params_.max_single_trade_capital;
}

bool ExecutionPlanner::checkRiskLimits(const ExecutionPlan& plan) {
    return plan.opportunity.opportunity.risk_score <= params_.max_portfolio_var;
}

bool ExecutionPlanner::checkCorrelationLimits(const std::vector<std::unique_ptr<ExecutionPlan>>& plans) {
    // Simplified correlation check
    return plans.size() <= 10; // Max 10 concurrent plans
}

std::string ExecutionPlanner::generatePlanId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "PLAN_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << "_" << dis(gen);
    return ss.str();
}

std::string ExecutionPlanner::generateOrderId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "ORD_" << std::put_time(std::gmtime(&time_t), "%H%M%S") << "_" << dis(gen);
    return ss.str();
}

void ExecutionPlanner::updatePlanningParameters(const PlanningParameters& params) {
    params_ = params;
    utils::Logger::info("Updated execution planning parameters");
}

} // namespace core
} // namespace arbitrage
