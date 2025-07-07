#pragma once

#include <vector>
#include <queue>
#include <random>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "ExecutionPlanner.hpp"
#include "PnLTracker.hpp"
#include "../data/MarketData.hpp"
#include "../utils/Logger.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief Simulation mode types
 */
enum class SimulationMode {
    PAPER_TRADING,       // Full simulation with realistic execution
    BACKTESTING,         // Historical data simulation
    STRESS_TESTING,      // Extreme scenario testing
    MONTE_CARLO          // Multiple random scenario simulation
};

/**
 * @brief Market condition simulation parameters
 */
struct MarketConditions {
    double volatility_multiplier = 1.0;
    double liquidity_multiplier = 1.0;
    double spread_multiplier = 1.0;
    double latency_multiplier = 1.0;
    
    // Market stress factors
    bool high_volatility = false;
    bool low_liquidity = false;
    bool wide_spreads = false;
    bool high_latency = false;
    
    // Extreme scenarios
    bool flash_crash = false;
    bool exchange_outage = false;
    bool network_partition = false;
};

/**
 * @brief Simulated execution result
 */
struct SimulatedExecution {
    ExecutionOrder original_order;
    
    // Execution details
    bool was_executed = false;
    double executed_price = 0.0;
    double executed_quantity = 0.0;
    std::chrono::system_clock::time_point execution_time;
    std::chrono::milliseconds execution_latency{0};
    
    // Execution quality metrics
    double slippage = 0.0;
    double market_impact = 0.0;
    double transaction_cost = 0.0;
    double total_cost = 0.0;
    
    // Failure reasons (if not executed)
    std::vector<std::string> failure_reasons;
    
    // Market state at execution
    double bid_at_execution = 0.0;
    double ask_at_execution = 0.0;
    double spread_at_execution = 0.0;
    double volume_at_execution = 0.0;
};

/**
 * @brief Simulation statistics and metrics
 */
struct SimulationMetrics {
    // Execution statistics
    size_t total_orders = 0;
    size_t successful_executions = 0;
    size_t failed_executions = 0;
    size_t partial_fills = 0;
    double success_rate = 0.0;
    
    // Timing metrics
    std::chrono::milliseconds average_execution_time{0};
    std::chrono::milliseconds median_execution_time{0};
    std::chrono::milliseconds max_execution_time{0};
    
    // Cost metrics
    double average_slippage = 0.0;
    double average_market_impact = 0.0;
    double total_transaction_costs = 0.0;
    double total_execution_costs = 0.0;
    
    // Performance metrics
    double total_simulated_pnl = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate = 0.0;
    
    // Risk metrics
    double var_95 = 0.0;
    double expected_shortfall = 0.0;
    double max_single_loss = 0.0;
    
    // Market impact analysis
    double market_impact_per_dollar = 0.0;
    double liquidity_consumption_rate = 0.0;
};

/**
 * @brief Advanced trade execution simulation engine
 */
class ExecutionSimulator {
public:
    struct SimulationParameters {
        SimulationMode mode;
        MarketConditions market_conditions;
        
        // Execution modeling parameters
        double base_slippage; // 0.05% base slippage
        double base_latency_ms; // 50ms base latency
        double transaction_fee_rate; // 0.1% transaction fee
        
        // Market impact modeling
        double market_impact_coefficient; // Impact per $1000 traded
        double liquidity_depth_factor; // $10k base liquidity depth
        
        // Execution probability factors
        double execution_success_rate; // 95% base success rate
        double partial_fill_probability; // 10% chance of partial fill
        
        // Stress testing parameters
        double stress_volatility_multiplier;
        double stress_liquidity_reduction; // 50% liquidity reduction
        double stress_spread_widening; // 2x spread widening
        
        // Random number generation
        uint32_t random_seed;
        bool use_deterministic_simulation;

        // Constructor with default values
        SimulationParameters() 
            : mode(SimulationMode::PAPER_TRADING),
              base_slippage(0.0005),
              base_latency_ms(50.0),
              transaction_fee_rate(0.001),
              market_impact_coefficient(0.0001),
              liquidity_depth_factor(10000.0),
              execution_success_rate(0.95),
              partial_fill_probability(0.1),
              stress_volatility_multiplier(3.0),
              stress_liquidity_reduction(0.5),
              stress_spread_widening(2.0),
              random_seed(42),
              use_deterministic_simulation(false) {
        }
    };
    
    explicit ExecutionSimulator(const SimulationParameters& params = SimulationParameters{});
    
    /**
     * @brief Start the simulation engine
     */
    void start();
    
    /**
     * @brief Stop the simulation engine
     */
    void stop();
    
    /**
     * @brief Submit execution plan for simulation
     */
    void submitExecutionPlan(std::unique_ptr<ExecutionPlan> plan);
    
    /**
     * @brief Simulate execution of a single order
     */
    SimulatedExecution simulateOrderExecution(
        const ExecutionOrder& order,
        const std::vector<data::MarketDataPoint>& market_data);
    
    /**
     * @brief Run backtesting simulation with historical data
     */
    SimulationMetrics runBacktest(
        const std::vector<RankedOpportunity>& opportunities,
        const std::vector<data::MarketDataPoint>& historical_data);
    
    /**
     * @brief Run Monte Carlo simulation with multiple scenarios
     */
    std::vector<SimulationMetrics> runMonteCarloSimulation(
        const std::vector<RankedOpportunity>& opportunities,
        size_t num_scenarios = 1000);
    
    /**
     * @brief Run stress testing under extreme conditions
     */
    SimulationMetrics runStressTest(
        const std::vector<RankedOpportunity>& opportunities,
        const MarketConditions& stress_conditions);
    
    /**
     * @brief Get current simulation metrics
     */
    SimulationMetrics getCurrentMetrics();
    
    /**
     * @brief Get all simulated executions
     */
    std::vector<SimulatedExecution> getExecutionHistory();
    
    /**
     * @brief Validate trading logic accuracy
     */
    struct ValidationResult {
        bool logic_is_accurate = true;
        double prediction_accuracy = 0.0;
        double profit_prediction_error = 0.0;
        double risk_prediction_error = 0.0;
        std::vector<std::string> validation_issues;
    };
    
    ValidationResult validateTradingLogic(
        const std::vector<RankedOpportunity>& predicted_opportunities,
        const std::vector<SimulatedExecution>& actual_executions);
    
    /**
     * @brief Update simulation parameters
     */
    void updateSimulationParameters(const SimulationParameters& params);
    
    /**
     * @brief Get current simulation parameters
     */
    const SimulationParameters& getSimulationParameters() const { return params_; }
    
    /**
     * @brief Set market data feed for real-time simulation
     */
    void setMarketDataFeed(const std::vector<data::MarketDataPoint>& market_data);
    
    /**
     * @brief Register callback for execution results
     */
    using ExecutionCallback = std::function<void(const SimulatedExecution&)>;
    void setExecutionCallback(ExecutionCallback callback);

private:
    SimulationParameters params_;
    std::atomic<bool> running_{false};
    std::thread simulation_thread_;
    
    // Market data and state
    std::vector<data::MarketDataPoint> current_market_data_;
    std::mutex market_data_mutex_;
    
    // Execution tracking
    std::vector<SimulatedExecution> execution_history_;
    std::queue<std::unique_ptr<ExecutionPlan>> pending_plans_;
    std::mutex execution_mutex_;
    std::condition_variable execution_cv_;
    
    // P&L tracking for simulation
    std::unique_ptr<PnLTracker> simulated_pnl_tracker_;
    
    // Random number generation
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};
    std::normal_distribution<double> normal_dist_{0.0, 1.0};
    
    // Execution callback
    ExecutionCallback execution_callback_;
    
    // Core simulation methods
    void simulationLoop();
    void processExecutionPlan(std::unique_ptr<ExecutionPlan> plan);
    
    // Market simulation helpers
    double simulateSlippage(const ExecutionOrder& order, const data::MarketDataPoint& market_data);
    double simulateMarketImpact(const ExecutionOrder& order, const data::MarketDataPoint& market_data);
    std::chrono::milliseconds simulateExecutionLatency(const ExecutionOrder& order);
    bool shouldExecutionSucceed(const ExecutionOrder& order, const data::MarketDataPoint& market_data);
    double calculatePartialFillQuantity(const ExecutionOrder& order);
    
    // Market condition modeling
    data::MarketDataPoint applyMarketConditions(const data::MarketDataPoint& original_data);
    double getVolatilityMultiplier();
    double getLiquidityMultiplier();
    double getSpreadMultiplier();
    double getLatencyMultiplier();
    
    // Stress testing helpers
    void applyStressConditions(MarketConditions& conditions);
    bool simulateExchangeOutage();
    bool simulateNetworkPartition();
    void simulateFlashCrash(std::vector<data::MarketDataPoint>& market_data);
    
    // Metrics calculation
    void updateSimulationMetrics(const SimulatedExecution& execution);
    SimulationMetrics calculateMetrics();
    
    // Validation helpers
    double calculatePredictionAccuracy(
        const std::vector<RankedOpportunity>& predicted,
        const std::vector<SimulatedExecution>& actual);
    
    // Utility functions
    data::MarketDataPoint findMarketData(const std::string& symbol, data::Exchange exchange);
    double generateRandomPrice(double base_price, double volatility);
    std::chrono::milliseconds generateRandomLatency(double base_latency_ms);
};

} // namespace core
} // namespace arbitrage
