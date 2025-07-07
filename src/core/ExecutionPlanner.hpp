#pragma once

#include <vector>
#include <queue>
#include <chrono>
#include <memory>
#include "ArbitrageEngine.hpp"
#include "OpportunityRanker.hpp"
#include "../data/MarketData.hpp"
#include "../utils/Logger.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief Execution timing strategy
 */
enum class ExecutionTiming {
    IMMEDIATE,
    OPTIMAL_DELAY,
    MARKET_CONDITION_BASED,
    LIQUIDITY_BASED
};

/**
 * @brief Order sizing strategy
 */
enum class SizingStrategy {
    FIXED_SIZE,
    KELLY_CRITERION,
    VOLATILITY_ADJUSTED,
    LIQUIDITY_CONSTRAINED,
    RISK_PARITY
};

/**
 * @brief Individual execution order
 */
struct ExecutionOrder {
    std::string symbol;
    data::Exchange exchange;
    std::string action; // "BUY" or "SELL"
    double quantity;
    double target_price;
    double limit_price;
    double stop_price;
    
    // Timing information
    std::chrono::system_clock::time_point planned_execution_time;
    std::chrono::milliseconds max_delay{1000}; // Maximum acceptable delay
    
    // Risk parameters
    double max_slippage = 0.001; // 0.1%
    double max_market_impact = 0.0005; // 0.05%
    
    // Status tracking
    bool is_executed = false;
    double executed_price = 0.0;
    double executed_quantity = 0.0;
    std::chrono::system_clock::time_point execution_time;
    
    // Unique identifier
    std::string order_id;
};

/**
 * @brief Execution plan for a complete arbitrage opportunity
 */
struct ExecutionPlan {
    std::string plan_id;
    RankedOpportunity opportunity;
    std::vector<ExecutionOrder> orders;
    
    // Execution parameters
    ExecutionTiming timing_strategy = ExecutionTiming::OPTIMAL_DELAY;
    SizingStrategy sizing_strategy = SizingStrategy::KELLY_CRITERION;
    
    // Risk management
    double max_total_capital = 0.0;
    double max_position_size = 0.0;
    double stop_loss_threshold = -0.05; // -5%
    double take_profit_threshold = 0.02; // 2%
    
    // Execution metrics
    double estimated_total_cost = 0.0;
    double estimated_slippage = 0.0;
    double estimated_market_impact = 0.0;
    std::chrono::milliseconds estimated_execution_time{500};
    
    // Status
    enum class Status {
        PLANNED,
        READY_TO_EXECUTE,
        EXECUTING,
        PARTIALLY_FILLED,
        COMPLETED,
        CANCELLED,
        FAILED
    } status = Status::PLANNED;
    
    // Timing
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point planned_start_time;
    std::chrono::system_clock::time_point actual_start_time;
    std::chrono::system_clock::time_point completion_time;
    
    // Performance tracking
    double actual_profit = 0.0;
    double actual_cost = 0.0;
    double actual_slippage = 0.0;
    double execution_quality_score = 0.0;
};

/**
 * @brief Execution planning and optimization engine
 */
class ExecutionPlanner {
public:
    struct PlanningParameters {
        // Capital constraints
        double max_single_trade_capital; // $50k max per trade
        double max_total_capital_utilization; // 80% of available capital
        
        // Risk parameters
        double max_portfolio_var; // 2% VaR limit
        double max_correlation_exposure; // 50% max correlated exposure
        
        // Execution parameters
        double default_slippage_tolerance; // 0.1%
        std::chrono::milliseconds max_execution_window; // 5 seconds
        
        // Sizing parameters
        double kelly_fraction; // Conservative Kelly fraction
        double min_position_size; // $100 minimum
        double max_position_size; // $10k maximum
        
        // Market impact thresholds
        double low_impact_threshold; // 0.01%
        double high_impact_threshold; // 0.1%

        // Constructor with default values
        PlanningParameters() 
            : max_single_trade_capital(50000.0),
              max_total_capital_utilization(0.8),
              max_portfolio_var(0.02),
              max_correlation_exposure(0.5),
              default_slippage_tolerance(0.001),
              max_execution_window(std::chrono::milliseconds(5000)),
              kelly_fraction(0.25),
              min_position_size(100.0),
              max_position_size(10000.0),
              low_impact_threshold(0.0001),
              high_impact_threshold(0.001) {
        }
    };
    
    explicit ExecutionPlanner(const PlanningParameters& params = PlanningParameters{});
    
    /**
     * @brief Create execution plan for a ranked opportunity
     */
    std::unique_ptr<ExecutionPlan> createExecutionPlan(const RankedOpportunity& opportunity);
    
    /**
     * @brief Optimize execution sequence for multiple opportunities
     */
    std::vector<std::unique_ptr<ExecutionPlan>> optimizeExecutionSequence(
        const std::vector<RankedOpportunity>& opportunities);
    
    /**
     * @brief Calculate optimal order sizing
     */
    std::vector<double> calculateOptimalSizing(
        const ArbitrageOpportunity& opportunity, 
        SizingStrategy strategy = SizingStrategy::KELLY_CRITERION);
    
    /**
     * @brief Optimize execution timing
     */
    std::chrono::system_clock::time_point calculateOptimalTiming(
        const ExecutionPlan& plan,
        const std::vector<data::MarketDataPoint>& market_data);
    
    /**
     * @brief Handle partial fill scenarios
     */
    void handlePartialFill(ExecutionPlan& plan, const ExecutionOrder& partially_filled_order);
    
    /**
     * @brief Estimate execution costs
     */
    struct ExecutionCostEstimate {
        double transaction_costs = 0.0;
        double market_impact = 0.0;
        double slippage = 0.0;
        double opportunity_cost = 0.0;
        double total_cost = 0.0;
    };
    
    ExecutionCostEstimate estimateExecutionCosts(const ExecutionPlan& plan);
    
    /**
     * @brief Validate execution plan feasibility
     */
    struct ValidationResult {
        bool is_valid = false;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        double confidence_score = 0.0;
    };
    
    ValidationResult validateExecutionPlan(const ExecutionPlan& plan);
    
    /**
     * @brief Update planning parameters
     */
    void updatePlanningParameters(const PlanningParameters& params);
    
    /**
     * @brief Get current planning parameters
     */
    const PlanningParameters& getPlanningParameters() const { return params_; }

private:
    PlanningParameters params_;
    
    // Helper methods for sizing strategies
    double calculateKellySizing(const ArbitrageOpportunity& opportunity);
    double calculateVolatilityAdjustedSizing(const ArbitrageOpportunity& opportunity);
    double calculateLiquidityConstrainedSizing(const ArbitrageOpportunity& opportunity);
    double calculateRiskParitySizing(const ArbitrageOpportunity& opportunity);
    
    // Market impact estimation
    double estimateMarketImpact(const ExecutionOrder& order);
    double estimateSlippage(const ExecutionOrder& order);
    
    // Timing optimization
    bool isOptimalExecutionTime(const std::vector<data::MarketDataPoint>& market_data);
    std::chrono::milliseconds calculateExecutionDelay(const ExecutionPlan& plan);
    
    // Risk management helpers
    bool checkCapitalConstraints(const ExecutionPlan& plan);
    bool checkRiskLimits(const ExecutionPlan& plan);
    bool checkCorrelationLimits(const std::vector<std::unique_ptr<ExecutionPlan>>& plans);
    
    // Utility functions
    std::string generatePlanId();
    std::string generateOrderId();
};

} // namespace core
} // namespace arbitrage
