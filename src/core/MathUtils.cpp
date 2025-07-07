#include "MathUtils.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <numeric>
#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace arbitrage {
namespace core {

// Normal CDF approximation using Abramowitz and Stegun formula
double MathUtils::normalCDF(double x) {
    if (x < -8.0) return 0.0;
    if (x > 8.0) return 1.0;
    
    return normalCDFApproximation(x);
}

double MathUtils::normalCDFApproximation(double x) {
    const double a1 = 0.254829592;
    const double a2 = -0.284496736;
    const double a3 = 1.421413741;
    const double a4 = -1.453152027;
    const double a5 = 1.061405429;
    const double p = 0.3275911;
    
    double sign = (x >= 0) ? 1.0 : -1.0;
    x = std::abs(x) / std::sqrt(2.0);
    
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
    
    return 0.5 * (1.0 + sign * y);
}

double MathUtils::normalPDF(double x) {
    return std::exp(-0.5 * x * x) / constants::SQRT_2PI;
}

double MathUtils::blackScholesPrice(double spot_price, double strike_price, 
                                   double time_to_expiry, double risk_free_rate, 
                                   double volatility, bool is_call) {
    if (time_to_expiry <= 0.0) {
        // Option has expired
        if (is_call) {
            return std::max(spot_price - strike_price, 0.0);
        } else {
            return std::max(strike_price - spot_price, 0.0);
        }
    }
    
    auto [d1, d2] = calculateD1D2(spot_price, strike_price, time_to_expiry, 
                                  risk_free_rate, volatility);
    
    if (is_call) {
        return spot_price * normalCDF(d1) - strike_price * std::exp(-risk_free_rate * time_to_expiry) * normalCDF(d2);
    } else {
        return strike_price * std::exp(-risk_free_rate * time_to_expiry) * normalCDF(-d2) - spot_price * normalCDF(-d1);
    }
}

MathUtils::Greeks MathUtils::calculateGreeks(double spot_price, double strike_price,
                                           double time_to_expiry, double risk_free_rate,
                                           double volatility, bool is_call) {
    Greeks greeks{};
    
    if (time_to_expiry <= 0.0) {
        return greeks; // All Greeks are zero at expiry
    }
    
    auto [d1, d2] = calculateD1D2(spot_price, strike_price, time_to_expiry, 
                                  risk_free_rate, volatility);
    
    double sqrt_time = std::sqrt(time_to_expiry);
    double discount_factor = std::exp(-risk_free_rate * time_to_expiry);
    
    // Delta
    if (is_call) {
        greeks.delta = normalCDF(d1);
    } else {
        greeks.delta = normalCDF(d1) - 1.0;
    }
    
    // Gamma (same for calls and puts)
    greeks.gamma = normalPDF(d1) / (spot_price * volatility * sqrt_time);
    
    // Theta
    double theta_common = -(spot_price * normalPDF(d1) * volatility) / (2.0 * sqrt_time);
    if (is_call) {
        greeks.theta = (theta_common - risk_free_rate * strike_price * discount_factor * normalCDF(d2)) / constants::DAYS_PER_YEAR;
    } else {
        greeks.theta = (theta_common + risk_free_rate * strike_price * discount_factor * normalCDF(-d2)) / constants::DAYS_PER_YEAR;
    }
    
    // Vega (same for calls and puts)
    greeks.vega = spot_price * normalPDF(d1) * sqrt_time / 100.0; // Per 1% volatility change
    
    // Rho
    if (is_call) {
        greeks.rho = strike_price * time_to_expiry * discount_factor * normalCDF(d2) / 100.0;
    } else {
        greeks.rho = -strike_price * time_to_expiry * discount_factor * normalCDF(-d2) / 100.0;
    }
    
    return greeks;
}

double MathUtils::impliedVolatility(double market_price, double spot_price,
                                   double strike_price, double time_to_expiry,
                                   double risk_free_rate, bool is_call,
                                   int max_iterations, double tolerance) {
    // Initial guess using Brenner-Subrahmanyam approximation
    double initial_vol = std::sqrt(2.0 * constants::PI / time_to_expiry) * (market_price / spot_price);
    initial_vol = std::max(0.01, std::min(initial_vol, 5.0)); // Clamp between 1% and 500%
    
    double vol = initial_vol;
    
    for (int i = 0; i < max_iterations; ++i) {
        double price = blackScholesPrice(spot_price, strike_price, time_to_expiry, 
                                        risk_free_rate, vol, is_call);
        double price_diff = price - market_price;
        
        if (std::abs(price_diff) < tolerance) {
            return vol;
        }
        
        // Calculate vega for Newton-Raphson iteration
        auto [d1, d2] = calculateD1D2(spot_price, strike_price, time_to_expiry, 
                                      risk_free_rate, vol);
        double vega = spot_price * normalPDF(d1) * std::sqrt(time_to_expiry);
        
        if (vega < constants::EPSILON) {
            break; // Avoid division by zero
        }
        
        // Newton-Raphson update
        vol = vol - price_diff / vega;
        vol = std::max(0.001, std::min(vol, 10.0)); // Keep vol in reasonable range
    }
    
    return vol;
}

double MathUtils::perpetualSyntheticPrice(double spot_price, double funding_rate, 
                                         double funding_interval) {
    // For perpetual swaps, the synthetic price includes funding rate impact
    // Synthetic Price ≈ Spot Price * (1 + funding_rate * funding_interval / 24)
    double funding_adjustment = funding_rate * funding_interval / 24.0;
    return spot_price * (1.0 + funding_adjustment);
}

double MathUtils::futuresSyntheticPrice(double spot_price, double risk_free_rate,
                                       double time_to_expiry, double convenience_yield) {
    // Futures Price = Spot * exp((r - q) * T)
    // where r = risk-free rate, q = convenience yield, T = time to expiry
    double cost_of_carry = risk_free_rate - convenience_yield;
    return spot_price * std::exp(cost_of_carry * time_to_expiry);
}

double MathUtils::calculateBasis(double futures_price, double spot_price) {
    return futures_price - spot_price;
}

double MathUtils::calculateAnnualizedBasis(double basis, double spot_price, double time_to_expiry) {
    if (time_to_expiry <= 0.0 || spot_price <= 0.0) {
        return 0.0;
    }
    return (basis / spot_price) / time_to_expiry;
}

void MathUtils::vectorAdd(const std::vector<double>& a, const std::vector<double>& b, 
                         std::vector<double>& result) {
    size_t size = std::min(a.size(), b.size());
    result.resize(size);
    
    // Use SIMD for vectorized addition
    using batch_type = xsimd::batch<double>;
    constexpr size_t simd_size = batch_type::size;
    size_t simd_end = (size / simd_size) * simd_size;
    
    // Process SIMD-sized chunks
    for (size_t i = 0; i < simd_end; i += simd_size) {
        auto va = batch_type::load_unaligned(&a[i]);
        auto vb = batch_type::load_unaligned(&b[i]);
        auto vr = va + vb;
        vr.store_unaligned(&result[i]);
    }
    
    // Process remaining elements
    for (size_t i = simd_end; i < size; ++i) {
        result[i] = a[i] + b[i];
    }
}

void MathUtils::vectorMultiply(const std::vector<double>& a, const std::vector<double>& b,
                              std::vector<double>& result) {
    size_t size = std::min(a.size(), b.size());
    result.resize(size);
    
    // Use SIMD for vectorized multiplication
    using batch_type = xsimd::batch<double>;
    constexpr size_t simd_size = batch_type::size;
    size_t simd_end = (size / simd_size) * simd_size;
    
    // Process SIMD-sized chunks
    for (size_t i = 0; i < simd_end; i += simd_size) {
        auto va = batch_type::load_unaligned(&a[i]);
        auto vb = batch_type::load_unaligned(&b[i]);
        auto vr = va * vb;
        vr.store_unaligned(&result[i]);
    }
    
    // Process remaining elements
    for (size_t i = simd_end; i < size; ++i) {
        result[i] = a[i] * b[i];
    }
}

void MathUtils::vectorScale(const std::vector<double>& input, double scalar,
                           std::vector<double>& result) {
    result.resize(input.size());
    
    // Use SIMD for vectorized scaling
    using batch_type = xsimd::batch<double>;
    constexpr size_t simd_size = batch_type::size;
    size_t simd_end = (input.size() / simd_size) * simd_size;
    
    // Create a batch filled with the scalar value
    batch_type vscalar(scalar);
    
    // Process SIMD-sized chunks
    for (size_t i = 0; i < simd_end; i += simd_size) {
        auto vi = batch_type::load_unaligned(&input[i]);
        auto vr = vi * vscalar;
        vr.store_unaligned(&result[i]);
    }
    
    // Process remaining elements
    for (size_t i = simd_end; i < input.size(); ++i) {
        result[i] = input[i] * scalar;
    }
}

double MathUtils::movingAverage(const std::vector<double>& values, size_t window_size) {
    if (values.empty() || window_size == 0) {
        return 0.0;
    }
    
    size_t start = values.size() >= window_size ? values.size() - window_size : 0;
    double sum = 0.0;
    size_t count = 0;
    
    for (size_t i = start; i < values.size(); ++i) {
        sum += values[i];
        ++count;
    }
    
    return count > 0 ? sum / count : 0.0;
}

double MathUtils::exponentialMovingAverage(const std::vector<double>& values, double alpha) {
    if (values.empty()) {
        return 0.0;
    }
    
    double ema = values[0];
    for (size_t i = 1; i < values.size(); ++i) {
        ema = alpha * values[i] + (1.0 - alpha) * ema;
    }
    
    return ema;
}

double MathUtils::standardDeviation(const std::vector<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    double sum_sq_diff = 0.0;
    for (double value : values) {
        double diff = value - mean;
        sum_sq_diff += diff * diff;
    }
    
    return std::sqrt(sum_sq_diff / (values.size() - 1));
}

double MathUtils::correlation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }
    
    size_t n = x.size();
    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;
    
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    
    for (size_t i = 0; i < n; ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        
        sum_xy += dx * dy;
        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
    }
    
    double denominator = std::sqrt(sum_x2 * sum_y2);
    return (denominator > constants::EPSILON) ? sum_xy / denominator : 0.0;
}

double MathUtils::yearsBetween(const std::chrono::system_clock::time_point& start,
                              const std::chrono::system_clock::time_point& end) {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    return duration.count() / constants::SECONDS_PER_YEAR;
}

double MathUtils::daysBetween(const std::chrono::system_clock::time_point& start,
                             const std::chrono::system_clock::time_point& end) {
    auto duration = std::chrono::duration_cast<std::chrono::hours>(end - start);
    return duration.count() / 24.0;
}

std::pair<double, double> MathUtils::calculateD1D2(double spot_price, double strike_price,
                                                   double time_to_expiry, double risk_free_rate,
                                                   double volatility) {
    double sqrt_time = std::sqrt(time_to_expiry);
    double vol_sqrt_time = volatility * sqrt_time;
    
    double d1 = (std::log(spot_price / strike_price) + (risk_free_rate + 0.5 * volatility * volatility) * time_to_expiry) / vol_sqrt_time;
    double d2 = d1 - vol_sqrt_time;
    
    return {d1, d2};
}

// CalculationPipeline implementation
CalculationPipeline::CalculationPipeline(size_t num_threads) : stop_(false), total_tasks_(0), completed_tasks_(0) {
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] { workerLoop(); });
    }
}

CalculationPipeline::~CalculationPipeline() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

CalculationPipeline::Statistics CalculationPipeline::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        total_tasks_,
        completed_tasks_,
        std::chrono::milliseconds(0),
        tasks_.size()
    };
}

void CalculationPipeline::workerLoop() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            
            if (stop_ && tasks_.empty()) {
                break;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        task();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++completed_tasks_;
        }
    }
}

} // namespace core
} // namespace arbitrage
