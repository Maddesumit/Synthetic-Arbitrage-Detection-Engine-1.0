#include "OpportunityRanker.hpp"
#include <numeric>
#include <cmath>
#include <algorithm>

namespace arbitrage {
namespace core {

OpportunityRanker::OpportunityRanker(const ScoringParameters& params) 
    : params_(params) {
    utils::Logger::info("OpportunityRanker initialized with custom parameters");
}

std::vector<RankedOpportunity> OpportunityRanker::rankOpportunities(
    const std::vector<ArbitrageOpportunity>& opportunities) {
    
    if (opportunities.empty()) {
        utils::Logger::warn("No opportunities to rank");
        return {};
    }
    
    // Filter opportunities by minimum criteria first
    auto filtered_opportunities = filterOpportunities(opportunities);
    utils::Logger::info("Filtered " + std::to_string(filtered_opportunities.size()) + 
                       " opportunities from " + std::to_string(opportunities.size()));
    
    std::vector<RankedOpportunity> ranked_opportunities;
    ranked_opportunities.reserve(filtered_opportunities.size());
    
    // Calculate all scores for each opportunity
    for (const auto& opportunity : filtered_opportunities) {
        RankedOpportunity ranked;
        ranked.opportunity = opportunity;
        
        // Calculate individual scores
        ranked.profit_score = calculateProfitScore(opportunity);
        ranked.risk_adjusted_score = calculateRiskAdjustedScore(opportunity);
        ranked.sharpe_score = calculateSharpeScore(opportunity);
        ranked.capital_efficiency_score = calculateCapitalEfficiencyScore(opportunity);
        ranked.liquidity_score = calculateLiquidityScore(opportunity);
        ranked.execution_probability_score = calculateExecutionProbabilityScore(opportunity);
        
        // Calculate additional metrics
        ranked.expected_sharpe_ratio = (opportunity.expected_profit_pct - params_.risk_free_rate) / 
                                      std::max(opportunity.risk_score, 0.001);
        ranked.capital_efficiency_ratio = opportunity.expected_profit_pct / 
                                         std::max(opportunity.required_capital, 1.0);
        
        // Calculate composite score
        ranked.composite_score = calculateCompositeScore(ranked);
        
        ranked_opportunities.push_back(ranked);
    }
    
    // Sort by composite score (descending)
    std::sort(ranked_opportunities.begin(), ranked_opportunities.end());
    
    // Assign ranks
    for (size_t i = 0; i < ranked_opportunities.size(); ++i) {
        ranked_opportunities[i].rank = i + 1;
    }
    
    utils::Logger::info("Ranked " + std::to_string(ranked_opportunities.size()) + " opportunities");
    
    return ranked_opportunities;
}

double OpportunityRanker::calculateProfitScore(const ArbitrageOpportunity& opportunity) {
    // Normalize profit percentage (0-10% range mapped to 0-1)
    double profit_pct = opportunity.expected_profit_pct * 100.0; // Convert to percentage
    return std::min(1.0, std::max(0.0, profit_pct / 10.0));
}

double OpportunityRanker::calculateRiskAdjustedScore(const ArbitrageOpportunity& opportunity) {
    // Risk-adjusted return = profit / risk
    double risk_adjusted_return = opportunity.expected_profit_pct / 
                                 std::max(opportunity.risk_score, 0.001);
    
    // Normalize to 0-1 scale (assuming max reasonable ratio of 10)
    return std::min(1.0, std::max(0.0, risk_adjusted_return / 10.0));
}

double OpportunityRanker::calculateSharpeScore(const ArbitrageOpportunity& opportunity) {
    // Calculate Sharpe ratio
    double excess_return = opportunity.expected_profit_pct - params_.risk_free_rate;
    double sharpe_ratio = excess_return / std::max(opportunity.risk_score, 0.001);
    
    // Normalize Sharpe ratio (assuming max reasonable Sharpe of 3.0)
    return std::min(1.0, std::max(0.0, sharpe_ratio / 3.0));
}

double OpportunityRanker::calculateCapitalEfficiencyScore(const ArbitrageOpportunity& opportunity) {
    // Capital efficiency = profit per dollar invested
    double efficiency = opportunity.expected_profit_pct / 
                       std::max(opportunity.required_capital, 1.0);
    
    // Normalize (assuming max efficiency of 0.001 = 0.1% per dollar)
    return std::min(1.0, std::max(0.0, efficiency / 0.001));
}

double OpportunityRanker::calculateLiquidityScore(const ArbitrageOpportunity& opportunity) {
    // Estimate liquidity based on required capital and market conditions
    // This is a simplified approach - in practice, you'd use order book depth
    
    double estimated_liquidity = 0.0;
    for (const auto& leg : opportunity.legs) {
        // Assume some relationship between price and available liquidity
        estimated_liquidity += leg.price * 100.0; // Simplified liquidity estimate
    }
    
    // Score based on liquidity relative to required capital
    double liquidity_ratio = estimated_liquidity / std::max(opportunity.required_capital, 1.0);
    
    // Normalize (assuming 10x liquidity is excellent)
    return std::min(1.0, std::max(0.0, liquidity_ratio / 10.0));
}

double OpportunityRanker::calculateExecutionProbabilityScore(const ArbitrageOpportunity& opportunity) {
    // Execution probability based on confidence and risk factors
    double base_probability = opportunity.confidence;
    
    // Adjust for risk factors
    double risk_adjustment = 1.0 - opportunity.risk_score;
    
    // Adjust for market conditions (simplified)
    double market_adjustment = 1.0; // Could incorporate volatility, spreads, etc.
    
    return base_probability * risk_adjustment * market_adjustment;
}

double OpportunityRanker::calculateCompositeScore(const RankedOpportunity& opportunity) {
    return params_.profit_weight * opportunity.profit_score +
           params_.risk_weight * opportunity.risk_adjusted_score +
           params_.liquidity_weight * opportunity.liquidity_score +
           params_.execution_weight * opportunity.execution_probability_score +
           params_.capital_efficiency_weight * opportunity.capital_efficiency_score;
}

std::vector<ArbitrageOpportunity> OpportunityRanker::filterOpportunities(
    const std::vector<ArbitrageOpportunity>& opportunities) {
    
    std::vector<ArbitrageOpportunity> filtered;
    
    for (const auto& opportunity : opportunities) {
        // Apply minimum thresholds
        if (opportunity.expected_profit_pct >= params_.min_profit_threshold &&
            opportunity.confidence >= params_.min_confidence_threshold &&
            opportunity.required_capital >= params_.min_liquidity_threshold) {
            
            filtered.push_back(opportunity);
        }
    }
    
    return filtered;
}

void OpportunityRanker::updateScoringParameters(const ScoringParameters& params) {
    params_ = params;
    utils::Logger::info("Updated scoring parameters");
}

OpportunityRanker::OpportunityStatistics OpportunityRanker::calculateStatistics(
    const std::vector<ArbitrageOpportunity>& opportunities) {
    
    OpportunityStatistics stats;
    stats.total_opportunities = opportunities.size();
    
    if (opportunities.empty()) {
        return stats;
    }
    
    // Calculate profit statistics
    std::vector<double> profits;
    std::vector<double> risks;
    std::vector<double> capitals;
    
    for (const auto& opp : opportunities) {
        profits.push_back(opp.expected_profit_pct);
        risks.push_back(opp.risk_score);
        capitals.push_back(opp.required_capital);
    }
    
    auto [mean_profit, std_profit] = calculateMeanStd(profits);
    auto [mean_risk, std_risk] = calculateMeanStd(risks);
    auto [mean_capital, std_capital] = calculateMeanStd(capitals);
    
    stats.mean_profit = mean_profit;
    stats.std_profit = std_profit;
    stats.mean_risk = mean_risk;
    stats.std_risk = std_risk;
    stats.mean_capital_required = mean_capital;
    
    // Count filtered opportunities
    auto filtered = filterOpportunities(opportunities);
    stats.filtered_opportunities = filtered.size();
    
    return stats;
}

double OpportunityRanker::normalizeScore(double value, double min_val, double max_val) {
    if (max_val <= min_val) return 0.0;
    return std::min(1.0, std::max(0.0, (value - min_val) / (max_val - min_val)));
}

double OpportunityRanker::calculateZScore(double value, double mean, double std_dev) {
    if (std_dev == 0.0) return 0.0;
    return (value - mean) / std_dev;
}

std::pair<double, double> OpportunityRanker::calculateMeanStd(const std::vector<double>& values) {
    if (values.empty()) return {0.0, 0.0};
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    double variance = 0.0;
    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= values.size();
    
    return {mean, std::sqrt(variance)};
}

} // namespace core
} // namespace arbitrage
