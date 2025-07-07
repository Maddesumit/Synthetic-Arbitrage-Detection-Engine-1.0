#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include "ArbitrageEngine.hpp"
#include "../utils/Logger.hpp"

namespace arbitrage {
namespace core {

/**
 * @brief Ranking criteria for arbitrage opportunities
 */
enum class RankingCriteria {
    PROFIT_PERCENTAGE,
    RISK_ADJUSTED_RETURN,
    SHARPE_RATIO,
    CAPITAL_EFFICIENCY,
    LIQUIDITY_SCORE,
    EXECUTION_PROBABILITY,
    COMPOSITE_SCORE
};

/**
 * @brief Statistical scoring parameters
 */
struct ScoringParameters {
    double profit_weight = 0.3;
    double risk_weight = 0.25;
    double liquidity_weight = 0.2;
    double execution_weight = 0.15;
    double capital_efficiency_weight = 0.1;
    
    // Risk-free rate for Sharpe ratio calculation
    double risk_free_rate = 0.02; // 2% annual
    
    // Minimum thresholds
    double min_profit_threshold = 0.001; // 0.1%
    double min_liquidity_threshold = 1000.0; // $1000 minimum liquidity
    double min_confidence_threshold = 0.7; // 70% minimum confidence
};

/**
 * @brief Enhanced opportunity with ranking metrics
 */
struct RankedOpportunity {
    ArbitrageOpportunity opportunity;
    
    // Ranking scores (0.0 - 1.0)
    double profit_score = 0.0;
    double risk_adjusted_score = 0.0;
    double sharpe_score = 0.0;
    double capital_efficiency_score = 0.0;
    double liquidity_score = 0.0;
    double execution_probability_score = 0.0;
    double composite_score = 0.0;
    
    // Additional metrics
    double expected_sharpe_ratio = 0.0;
    double capital_efficiency_ratio = 0.0;
    double liquidity_depth = 0.0;
    double execution_cost_estimate = 0.0;
    
    // Ranking position
    size_t rank = 0;
    
    bool operator<(const RankedOpportunity& other) const {
        return composite_score > other.composite_score; // Higher score = better rank
    }
};

/**
 * @brief Advanced opportunity ranking and scoring system
 */
class OpportunityRanker {
public:
    explicit OpportunityRanker(const ScoringParameters& params = ScoringParameters{});
    
    /**
     * @brief Rank a list of arbitrage opportunities
     */
    std::vector<RankedOpportunity> rankOpportunities(
        const std::vector<ArbitrageOpportunity>& opportunities);
    
    /**
     * @brief Calculate individual scoring metrics
     */
    double calculateProfitScore(const ArbitrageOpportunity& opportunity);
    double calculateRiskAdjustedScore(const ArbitrageOpportunity& opportunity);
    double calculateSharpeScore(const ArbitrageOpportunity& opportunity);
    double calculateCapitalEfficiencyScore(const ArbitrageOpportunity& opportunity);
    double calculateLiquidityScore(const ArbitrageOpportunity& opportunity);
    double calculateExecutionProbabilityScore(const ArbitrageOpportunity& opportunity);
    
    /**
     * @brief Calculate composite score using weighted combination
     */
    double calculateCompositeScore(const RankedOpportunity& opportunity);
    
    /**
     * @brief Filter opportunities by minimum criteria
     */
    std::vector<ArbitrageOpportunity> filterOpportunities(
        const std::vector<ArbitrageOpportunity>& opportunities);
    
    /**
     * @brief Update scoring parameters dynamically
     */
    void updateScoringParameters(const ScoringParameters& params);
    
    /**
     * @brief Get current scoring parameters
     */
    const ScoringParameters& getScoringParameters() const { return params_; }
    
    /**
     * @brief Statistical analysis of opportunity distribution
     */
    struct OpportunityStatistics {
        double mean_profit = 0.0;
        double std_profit = 0.0;
        double mean_risk = 0.0;
        double std_risk = 0.0;
        double mean_capital_required = 0.0;
        size_t total_opportunities = 0;
        size_t filtered_opportunities = 0;
    };
    
    OpportunityStatistics calculateStatistics(
        const std::vector<ArbitrageOpportunity>& opportunities);

private:
    ScoringParameters params_;
    
    // Statistical normalization helpers
    double normalizeScore(double value, double min_val, double max_val);
    double calculateZScore(double value, double mean, double std_dev);
    std::pair<double, double> calculateMeanStd(const std::vector<double>& values);
};

} // namespace core
} // namespace arbitrage
