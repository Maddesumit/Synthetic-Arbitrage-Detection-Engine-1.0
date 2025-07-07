#include "ExecutionSimulator.hpp"
#include "../utils/Logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <thread>

namespace arbitrage {
namespace core {

ExecutionSimulator::ExecutionSimulator(const SimulationParameters& params)
    : params_(params) {
    
    // Initialize random number generator with seed
    if (params_.use_deterministic_simulation) {
        rng_.seed(params_.random_seed);
    } else {
        std::random_device rd;
        rng_.seed(rd());
    }
    
    // Initialize P&L tracker for simulation
    PnLTracker::TrackingParameters tracking_params;
    tracking_params.initial_capital = 1000000.0; // $1M initial capital for simulation
    tracking_params.snapshot_interval = std::chrono::minutes(1); // 1-minute snapshots for simulation
    
    simulated_pnl_tracker_ = std::make_unique<PnLTracker>(tracking_params);
    
    std::string mode = params_.use_deterministic_simulation ? "deterministic" : "random";
    utils::Logger::info("Execution simulator initialized with " + mode + " mode");
}

void ExecutionSimulator::start() {
    if (running_.exchange(true)) {
        utils::Logger::warn("Execution simulator already running");
        return;
    }
    
    std::string mode_str;
    if (params_.mode == SimulationMode::PAPER_TRADING) {
        mode_str = "paper trading";
    } else if (params_.mode == SimulationMode::BACKTESTING) {
        mode_str = "backtesting";
    } else if (params_.mode == SimulationMode::STRESS_TESTING) {
        mode_str = "stress testing";
    } else {
        mode_str = "Monte Carlo";
    }
    
    utils::Logger::info("Starting execution simulator in " + mode_str + " mode");
    
    // Start simulation thread
    simulation_thread_ = std::thread(&ExecutionSimulator::simulationLoop, this);
}

void ExecutionSimulator::stop() {
    if (!running_.exchange(false)) {
        utils::Logger::warn("Execution simulator already stopped");
        return;
    }
    
    utils::Logger::info("Stopping execution simulator");
    
    // Wait for simulation thread to finish
    if (simulation_thread_.joinable()) {
        execution_cv_.notify_all();
        simulation_thread_.join();
    }
}

void ExecutionSimulator::submitExecutionPlan(std::unique_ptr<ExecutionPlan> plan) {
    if (!plan) {
        utils::Logger::error("Cannot submit null execution plan");
        return;
    }
    
    utils::Logger::info("Submitting execution plan " + plan->plan_id + 
                       " for " + plan->opportunity.opportunity.underlying_symbol);
    
    {
        std::lock_guard<std::mutex> lock(execution_mutex_);
        pending_plans_.push(std::move(plan));
    }
    
    execution_cv_.notify_one();
}

SimulatedExecution ExecutionSimulator::simulateOrderExecution(
    const ExecutionOrder& order,
    const std::vector<data::MarketDataPoint>& market_data) {
    
    SimulatedExecution result;
    result.original_order = order;
    
    // Find market data for the order's symbol and exchange
    auto market_point = findMarketData(order.symbol, order.exchange);
    
    if (market_point.symbol.empty()) {
        result.was_executed = false;
        result.failure_reasons.push_back("No market data available for " + 
                                        order.symbol + " on " + 
                                        std::to_string(static_cast<int>(order.exchange)));
        return result;
    }
    
    // Determine if execution should succeed based on market conditions
    bool execution_success = shouldExecutionSucceed(order, market_point);
    
    // Simulate exchange outage if applicable
    if (params_.market_conditions.exchange_outage && simulateExchangeOutage()) {
        execution_success = false;
        result.failure_reasons.push_back("Simulated exchange outage");
    }
    
    // Simulate network partition if applicable
    if (params_.market_conditions.network_partition && simulateNetworkPartition()) {
        execution_success = false;
        result.failure_reasons.push_back("Simulated network partition");
    }
    
    if (!execution_success) {
        result.was_executed = false;
        if (result.failure_reasons.empty()) {
            result.failure_reasons.push_back("Execution failed due to market conditions");
        }
        return result;
    }
    
    // Execution succeeded, simulate execution details
    result.was_executed = true;
    
    // Simulate latency
    result.execution_latency = simulateExecutionLatency(order);
    result.execution_time = std::chrono::system_clock::now() + 
                           std::chrono::duration_cast<std::chrono::system_clock::duration>(
                               result.execution_latency);
    
    // Record market state at execution
    result.bid_at_execution = market_point.bid;
    result.ask_at_execution = market_point.ask;
    result.spread_at_execution = market_point.ask - market_point.bid;
    result.volume_at_execution = market_point.volume;
    
    // Simulate slippage
    result.slippage = simulateSlippage(order, market_point);
    
    // Calculate executed price with slippage
    if (order.action == "BUY") {
        result.executed_price = order.target_price * (1.0 + result.slippage);
    } else {
        result.executed_price = order.target_price * (1.0 - result.slippage);
    }
    
    // Simulate market impact
    result.market_impact = simulateMarketImpact(order, market_point);
    
    // Simulate transaction cost
    result.transaction_cost = order.target_price * order.quantity * params_.transaction_fee_rate;
    
    // Calculate total cost
    result.total_cost = result.transaction_cost + 
                        (result.slippage * order.target_price * order.quantity);
    
    // Determine if this is a partial fill
    bool partial_fill = uniform_dist_(rng_) < params_.partial_fill_probability;
    
    if (partial_fill) {
        result.executed_quantity = calculatePartialFillQuantity(order);
    } else {
        result.executed_quantity = order.quantity;
    }
    
    return result;
}

SimulationMetrics ExecutionSimulator::runBacktest(
    const std::vector<RankedOpportunity>& opportunities,
    const std::vector<data::MarketDataPoint>& historical_data) {
    
    utils::Logger::info("Running backtest with " + 
                       std::to_string(opportunities.size()) + " opportunities and " +
                       std::to_string(historical_data.size()) + " historical data points");
    
    // Configure for backtesting mode
    SimulationParameters backtest_params = params_;
    backtest_params.mode = SimulationMode::BACKTESTING;
    backtest_params.use_deterministic_simulation = true;
    
    // Create temporary simulator for backtesting
    ExecutionSimulator backtest_simulator(backtest_params);
    
    // Set historical market data
    backtest_simulator.setMarketDataFeed(historical_data);
    
    // Create execution plans for each opportunity
    ExecutionPlanner planner;
    
    for (const auto& opportunity : opportunities) {
        auto plan = planner.createExecutionPlan(opportunity);
        if (plan) {
            backtest_simulator.submitExecutionPlan(std::move(plan));
        }
    }
    
    // Run the backtest
    backtest_simulator.start();
    
    // Wait for backtest to complete (simplified for demo)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Stop the backtest
    backtest_simulator.stop();
    
    // Get the metrics
    return backtest_simulator.getCurrentMetrics();
}

std::vector<SimulationMetrics> ExecutionSimulator::runMonteCarloSimulation(
    const std::vector<RankedOpportunity>& opportunities,
    size_t num_scenarios) {
    
    utils::Logger::info("Running Monte Carlo simulation with " + 
                       std::to_string(opportunities.size()) + " opportunities and " +
                       std::to_string(num_scenarios) + " scenarios");
    
    std::vector<SimulationMetrics> results;
    results.reserve(num_scenarios);
    
    // Run multiple simulations with different random seeds
    for (size_t i = 0; i < num_scenarios; ++i) {
        // Configure for Monte Carlo mode
        SimulationParameters mc_params = params_;
        mc_params.mode = SimulationMode::MONTE_CARLO;
        mc_params.random_seed = params_.random_seed + static_cast<uint32_t>(i);
        mc_params.use_deterministic_simulation = true;
        
        // Create temporary simulator for this scenario
        ExecutionSimulator scenario_simulator(mc_params);
        
        // Create execution plans for each opportunity
        ExecutionPlanner planner;
        
        for (const auto& opportunity : opportunities) {
            auto plan = planner.createExecutionPlan(opportunity);
            if (plan) {
                scenario_simulator.submitExecutionPlan(std::move(plan));
            }
        }
        
        // Run the scenario
        scenario_simulator.start();
        
        // Wait for simulation to complete (simplified for demo)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop the simulation
        scenario_simulator.stop();
        
        // Get the metrics for this scenario
        results.push_back(scenario_simulator.getCurrentMetrics());
    }
    
    return results;
}

SimulationMetrics ExecutionSimulator::runStressTest(
    const std::vector<RankedOpportunity>& opportunities,
    const MarketConditions& stress_conditions) {
    
    utils::Logger::info("Running stress test with " + 
                       std::to_string(opportunities.size()) + " opportunities");
    
    // Configure for stress testing mode
    SimulationParameters stress_params = params_;
    stress_params.mode = SimulationMode::STRESS_TESTING;
    stress_params.market_conditions = stress_conditions;
    
    // Make stress conditions more extreme
    applyStressConditions(stress_params.market_conditions);
    
    // Create temporary simulator for stress testing
    ExecutionSimulator stress_simulator(stress_params);
    
    // Create execution plans for each opportunity
    ExecutionPlanner planner;
    
    for (const auto& opportunity : opportunities) {
        auto plan = planner.createExecutionPlan(opportunity);
        if (plan) {
            stress_simulator.submitExecutionPlan(std::move(plan));
        }
    }
    
    // Run the stress test
    stress_simulator.start();
    
    // Wait for stress test to complete (simplified for demo)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Stop the stress test
    stress_simulator.stop();
    
    // Get the metrics
    return stress_simulator.getCurrentMetrics();
}

SimulationMetrics ExecutionSimulator::getCurrentMetrics() {
    return calculateMetrics();
}

std::vector<SimulatedExecution> ExecutionSimulator::getExecutionHistory() {
    std::lock_guard<std::mutex> lock(execution_mutex_);
    return execution_history_;
}

ExecutionSimulator::ValidationResult ExecutionSimulator::validateTradingLogic(
    const std::vector<RankedOpportunity>& predicted_opportunities,
    const std::vector<SimulatedExecution>& actual_executions) {
    
    ValidationResult result;
    
    if (predicted_opportunities.empty() || actual_executions.empty()) {
        result.logic_is_accurate = false;
        result.validation_issues.push_back("Insufficient data for validation");
        return result;
    }
    
    // Calculate prediction accuracy
    result.prediction_accuracy = calculatePredictionAccuracy(
        predicted_opportunities, actual_executions);
    
    // Validate profit predictions
    double total_predicted_profit = 0.0;
    double total_actual_profit = 0.0;
    
    for (const auto& opportunity : predicted_opportunities) {
        total_predicted_profit += opportunity.opportunity.expected_profit_pct * 
                                  opportunity.opportunity.required_capital;
    }
    
    // Calculate actual profit from executions (simplified)
    total_actual_profit = simulated_pnl_tracker_->getCurrentSnapshot().total_pnl;
    
    // Calculate error metrics
    if (total_predicted_profit > 0) {
        result.profit_prediction_error = 
            std::abs(total_actual_profit - total_predicted_profit) / total_predicted_profit;
    }
    
    // Determine overall accuracy
    if (result.prediction_accuracy < 0.5 || result.profit_prediction_error > 0.5) {
        result.logic_is_accurate = false;
        result.validation_issues.push_back("Low prediction accuracy: " + 
                                          std::to_string(result.prediction_accuracy));
        result.validation_issues.push_back("High profit prediction error: " + 
                                          std::to_string(result.profit_prediction_error));
    }
    
    return result;
}

void ExecutionSimulator::updateSimulationParameters(const SimulationParameters& params) {
    params_ = params;
}

void ExecutionSimulator::setMarketDataFeed(const std::vector<data::MarketDataPoint>& market_data) {
    std::lock_guard<std::mutex> lock(market_data_mutex_);
    current_market_data_ = market_data;
}

void ExecutionSimulator::setExecutionCallback(ExecutionCallback callback) {
    execution_callback_ = callback;
}

// Private implementation methods

void ExecutionSimulator::simulationLoop() {
    utils::Logger::info("Simulation loop started");
    
    while (running_) {
        std::unique_ptr<ExecutionPlan> plan;
        
        {
            std::unique_lock<std::mutex> lock(execution_mutex_);
            execution_cv_.wait_for(lock, std::chrono::milliseconds(100), 
                                  [this] { return !pending_plans_.empty() || !running_; });
            
            if (!running_) {
                break;
            }
            
            if (!pending_plans_.empty()) {
                plan = std::move(pending_plans_.front());
                pending_plans_.pop();
            }
        }
        
        if (plan) {
            processExecutionPlan(std::move(plan));
        }
    }
    
    utils::Logger::info("Simulation loop ended");
}

void ExecutionSimulator::processExecutionPlan(std::unique_ptr<ExecutionPlan> plan) {
    utils::Logger::info("Processing execution plan " + plan->plan_id);
    
    // Update plan status
    plan->status = ExecutionPlan::Status::EXECUTING;
    plan->actual_start_time = std::chrono::system_clock::now();
    
    // Process each order in the plan
    for (const auto& order : plan->orders) {
        // Get current market data
        std::vector<data::MarketDataPoint> market_data;
        {
            std::lock_guard<std::mutex> lock(market_data_mutex_);
            market_data = current_market_data_;
        }
        
        // Simulate order execution
        auto execution = simulateOrderExecution(order, market_data);
        
        // Store execution result
        {
            std::lock_guard<std::mutex> lock(execution_mutex_);
            execution_history_.push_back(execution);
        }
        
        // Update metrics
        updateSimulationMetrics(execution);
        
        // Record trade in P&L tracker if execution succeeded
        if (execution.was_executed) {
            simulated_pnl_tracker_->recordTrade(
                execution.original_order, 
                execution.executed_price, 
                execution.executed_quantity);
        }
        
        // Notify callback if registered
        if (execution_callback_) {
            execution_callback_(execution);
        }
        
        // Simulate passage of time between orders
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Update plan status
    plan->status = ExecutionPlan::Status::COMPLETED;
    plan->completion_time = std::chrono::system_clock::now();
    
    utils::Logger::info("Execution plan " + plan->plan_id + " completed");
}

double ExecutionSimulator::simulateSlippage(const ExecutionOrder& order, 
                                           const data::MarketDataPoint& market_data) {
    // Base slippage scaled by market conditions
    double base_slippage = params_.base_slippage * getVolatilityMultiplier();
    
    // Add spread component
    double spread_pct = (market_data.ask - market_data.bid) / market_data.last;
    double spread_component = spread_pct * getSpreadMultiplier();
    
    // Add size-based component
    double order_value = order.quantity * order.target_price;
    double size_component = 0.0;
    
    if (order_value > 0 && market_data.volume > 0) {
        // Scale slippage based on order size relative to volume
        size_component = std::min(1.0, order_value / (market_data.volume * market_data.last)) * 0.001;
    }
    
    // Random component using normal distribution
    double random_component = std::abs(normal_dist_(rng_)) * 0.0005;
    
    // Combine components
    return base_slippage + spread_component + size_component + random_component;
}

double ExecutionSimulator::simulateMarketImpact(const ExecutionOrder& order, 
                                              const data::MarketDataPoint& market_data) {
    // Calculate order value
    double order_value = order.quantity * order.target_price;
    
    // Base impact coefficient scaled by liquidity
    double impact_coefficient = params_.market_impact_coefficient / getLiquidityMultiplier();
    
    // Calculate market impact as a percentage of price
    double market_impact = impact_coefficient * (order_value / params_.liquidity_depth_factor);
    
    // Cap at a reasonable level
    return std::min(0.01, market_impact);
}

std::chrono::milliseconds ExecutionSimulator::simulateExecutionLatency(const ExecutionOrder& order) {
    // Base latency scaled by network conditions
    double base_latency = params_.base_latency_ms * getLatencyMultiplier();
    
    // Add random component
    double latency_ms = base_latency * (1.0 + normal_dist_(rng_) * 0.2);
    
    // Ensure minimum latency
    latency_ms = std::max(5.0, latency_ms);
    
    return std::chrono::milliseconds(static_cast<int>(latency_ms));
}

bool ExecutionSimulator::shouldExecutionSucceed(const ExecutionOrder& order, 
                                              const data::MarketDataPoint& market_data) {
    // Base success probability
    double success_probability = params_.execution_success_rate;
    
    // Reduce probability based on market conditions
    if (params_.market_conditions.high_volatility) {
        success_probability *= 0.9;
    }
    
    if (params_.market_conditions.low_liquidity) {
        success_probability *= 0.8;
    }
    
    // Reduce probability for large orders relative to volume
    double order_value = order.quantity * order.target_price;
    if (market_data.volume > 0) {
        double size_factor = order_value / (market_data.volume * market_data.last);
        if (size_factor > 0.1) {
            success_probability *= (1.0 - size_factor);
        }
    }
    
    // Check against random threshold
    return uniform_dist_(rng_) < success_probability;
}

double ExecutionSimulator::calculatePartialFillQuantity(const ExecutionOrder& order) {
    // Random partial fill between 10% and 90% of original quantity
    double fill_percentage = 0.1 + uniform_dist_(rng_) * 0.8;
    return order.quantity * fill_percentage;
}

data::MarketDataPoint ExecutionSimulator::applyMarketConditions(
    const data::MarketDataPoint& original_data) {
    
    data::MarketDataPoint modified = original_data;
    
    // Apply volatility multiplier
    double volatility = getVolatilityMultiplier();
    double spread_multiplier = getSpreadMultiplier();
    double liquidity_multiplier = getLiquidityMultiplier();
    
    // Modify the bid-ask spread
    double mid_price = (original_data.bid + original_data.ask) / 2.0;
    double half_spread = (original_data.ask - original_data.bid) / 2.0;
    double new_half_spread = half_spread * spread_multiplier;
    
    modified.bid = mid_price - new_half_spread;
    modified.ask = mid_price + new_half_spread;
    
    // Add random price variation based on volatility
    double price_variation = mid_price * volatility * normal_dist_(rng_) * 0.01;
    modified.bid += price_variation;
    modified.ask += price_variation;
    modified.last = (modified.bid + modified.ask) / 2.0;
    
    // Modify volume based on liquidity
    modified.volume = original_data.volume * liquidity_multiplier;
    
    return modified;
}

double ExecutionSimulator::getVolatilityMultiplier() {
    double multiplier = params_.market_conditions.volatility_multiplier;
    
    if (params_.market_conditions.high_volatility) {
        multiplier *= 2.0;
    }
    
    if (params_.market_conditions.flash_crash) {
        multiplier *= 5.0;
    }
    
    return multiplier;
}

double ExecutionSimulator::getLiquidityMultiplier() {
    double multiplier = params_.market_conditions.liquidity_multiplier;
    
    if (params_.market_conditions.low_liquidity) {
        multiplier *= 0.5;
    }
    
    return multiplier;
}

double ExecutionSimulator::getSpreadMultiplier() {
    double multiplier = params_.market_conditions.spread_multiplier;
    
    if (params_.market_conditions.wide_spreads) {
        multiplier *= 2.0;
    }
    
    if (params_.market_conditions.high_volatility) {
        multiplier *= 1.5;
    }
    
    return multiplier;
}

double ExecutionSimulator::getLatencyMultiplier() {
    double multiplier = params_.market_conditions.latency_multiplier;
    
    if (params_.market_conditions.high_latency) {
        multiplier *= 3.0;
    }
    
    if (params_.market_conditions.network_partition) {
        multiplier *= 10.0;
    }
    
    return multiplier;
}

void ExecutionSimulator::applyStressConditions(MarketConditions& conditions) {
    conditions.volatility_multiplier = params_.stress_volatility_multiplier;
    conditions.liquidity_multiplier = params_.stress_liquidity_reduction;
    conditions.spread_multiplier = params_.stress_spread_widening;
    conditions.latency_multiplier = 2.0;
    
    conditions.high_volatility = true;
    conditions.low_liquidity = true;
    conditions.wide_spreads = true;
    conditions.high_latency = true;
}

bool ExecutionSimulator::simulateExchangeOutage() {
    // 5% chance of exchange outage if enabled
    return uniform_dist_(rng_) < 0.05;
}

bool ExecutionSimulator::simulateNetworkPartition() {
    // 3% chance of network partition if enabled
    return uniform_dist_(rng_) < 0.03;
}

void ExecutionSimulator::simulateFlashCrash(std::vector<data::MarketDataPoint>& market_data) {
    // Simulate flash crash by dropping prices by 5-15%
    double crash_magnitude = 0.05 + uniform_dist_(rng_) * 0.1;
    
    for (auto& data_point : market_data) {
        data_point.bid *= (1.0 - crash_magnitude);
        data_point.ask *= (1.0 - crash_magnitude);
        data_point.last *= (1.0 - crash_magnitude);
        data_point.volume *= 3.0; // Increase volume during crash
    }
}

void ExecutionSimulator::updateSimulationMetrics(const SimulatedExecution& execution) {
    // This would update internal metrics tracking
    // Simplified for demo purposes
}

SimulationMetrics ExecutionSimulator::calculateMetrics() {
    SimulationMetrics metrics;
    
    // In a real implementation, this would calculate comprehensive metrics
    // based on execution_history_ and simulated_pnl_tracker_
    
    std::lock_guard<std::mutex> lock(execution_mutex_);
    
    // Basic metrics from execution history
    metrics.total_orders = execution_history_.size();
    
    metrics.successful_executions = std::count_if(
        execution_history_.begin(), execution_history_.end(),
        [](const SimulatedExecution& exec) { return exec.was_executed; });
    
    metrics.failed_executions = metrics.total_orders - metrics.successful_executions;
    
    metrics.partial_fills = std::count_if(
        execution_history_.begin(), execution_history_.end(),
        [](const SimulatedExecution& exec) { 
            return exec.was_executed && exec.executed_quantity < exec.original_order.quantity; 
        });
    
    // Calculate success rate
    if (metrics.total_orders > 0) {
        metrics.success_rate = static_cast<double>(metrics.successful_executions) / 
                              metrics.total_orders;
    }
    
    // Calculate average slippage
    if (metrics.successful_executions > 0) {
        double total_slippage = 0.0;
        for (const auto& exec : execution_history_) {
            if (exec.was_executed) {
                total_slippage += exec.slippage;
            }
        }
        metrics.average_slippage = total_slippage / metrics.successful_executions;
    }
    
    // Get P&L metrics from tracker
    if (simulated_pnl_tracker_) {
        auto snapshot = simulated_pnl_tracker_->getCurrentSnapshot();
        metrics.total_simulated_pnl = snapshot.total_pnl;
        metrics.sharpe_ratio = snapshot.sharpe_ratio;
        metrics.max_drawdown = snapshot.max_drawdown_pct;
        metrics.win_rate = snapshot.win_rate_pct / 100.0;
        metrics.var_95 = snapshot.var_95;
    }
    
    return metrics;
}

double ExecutionSimulator::calculatePredictionAccuracy(
    const std::vector<RankedOpportunity>& predicted,
    const std::vector<SimulatedExecution>& actual) {
    
    // Simplified prediction accuracy calculation
    // In a real implementation, this would be more sophisticated
    
    // Count profitable trades
    int profitable_executions = 0;
    for (const auto& exec : actual) {
        if (exec.was_executed) {
            // Simplified profit calculation
            bool is_buy = exec.original_order.action == "BUY";
            double profit_pct = is_buy ? 
                (simulated_pnl_tracker_->getMarketPrice(
                    exec.original_order.symbol, exec.original_order.exchange) - 
                 exec.executed_price) / exec.executed_price :
                (exec.executed_price - 
                 simulated_pnl_tracker_->getMarketPrice(
                     exec.original_order.symbol, exec.original_order.exchange)) / 
                exec.executed_price;
            
            if (profit_pct > 0) {
                profitable_executions++;
            }
        }
    }
    
    // Calculate percentage of profitable trades
    double actual_success_rate = 0.0;
    if (!actual.empty()) {
        actual_success_rate = static_cast<double>(profitable_executions) / actual.size();
    }
    
    // Calculate average predicted success rate
    double predicted_success_rate = 0.0;
    if (!predicted.empty()) {
        double total_confidence = 0.0;
        for (const auto& opp : predicted) {
            total_confidence += opp.opportunity.confidence;
        }
        predicted_success_rate = total_confidence / predicted.size();
    }
    
    // Calculate accuracy as 1 - absolute difference between predictions and actuals
    return 1.0 - std::abs(predicted_success_rate - actual_success_rate);
}

data::MarketDataPoint ExecutionSimulator::findMarketData(
    const std::string& symbol, data::Exchange exchange) {
    
    std::lock_guard<std::mutex> lock(market_data_mutex_);
    
    // Convert exchange enum to string for comparison
    std::string exchange_str;
    switch (exchange) {
        case data::Exchange::BINANCE:
            exchange_str = "binance";
            break;
        case data::Exchange::OKX:
            exchange_str = "okx";
            break;
        case data::Exchange::BYBIT:
            exchange_str = "bybit";
            break;
        default:
            exchange_str = "unknown";
            break;
    }
    
    // Find matching market data
    for (const auto& data : current_market_data_) {
        if (data.symbol == symbol && data.exchange == exchange_str) {
            return data;
        }
    }
    
    // Return empty data if not found
    return data::MarketDataPoint{};
}

double ExecutionSimulator::generateRandomPrice(double base_price, double volatility) {
    return base_price * (1.0 + normal_dist_(rng_) * volatility);
}

std::chrono::milliseconds ExecutionSimulator::generateRandomLatency(double base_latency_ms) {
    double latency = base_latency_ms * (1.0 + std::abs(normal_dist_(rng_) * 0.5));
    return std::chrono::milliseconds(static_cast<int>(latency));
}

} // namespace core
} // namespace arbitrage
