#pragma once

#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <optional>
#include <memory>
#include <xsimd/xsimd.hpp>

namespace arbitrage {
namespace core {

/**
 * @brief Mathematical constants
 */
namespace constants {
    constexpr double PI = 3.14159265358979323846;
    constexpr double E = 2.71828182845904523536;
    constexpr double SQRT_2PI = 2.50662827463100050241;
    constexpr double EPSILON = 1e-15;
    constexpr double SECONDS_PER_YEAR = 365.25 * 24 * 3600;
    constexpr double DAYS_PER_YEAR = 365.25;
}

/**
 * @brief SIMD-optimized mathematical utilities
 */
class MathUtils {
public:
    /**
     * @brief Calculate cumulative normal distribution (CDF)
     * Uses SIMD optimization for vectorized calculation
     */
    static double normalCDF(double x);
    
    /**
     * @brief Calculate probability density function (PDF) of normal distribution
     */
    static double normalPDF(double x);
    
    /**
     * @brief Calculate Black-Scholes option price
     * @param spot_price Current spot price
     * @param strike_price Strike price
     * @param time_to_expiry Time to expiry in years
     * @param risk_free_rate Risk-free interest rate
     * @param volatility Volatility (annualized)
     * @param is_call true for call option, false for put option
     */
    static double blackScholesPrice(double spot_price, double strike_price, 
                                   double time_to_expiry, double risk_free_rate, 
                                   double volatility, bool is_call = true);
    
    /**
     * @brief Calculate Black-Scholes Greeks
     */
    struct Greeks {
        double delta;    // Price sensitivity to underlying
        double gamma;    // Delta sensitivity to underlying
        double theta;    // Time decay
        double vega;     // Volatility sensitivity
        double rho;      // Interest rate sensitivity
    };
    
    static Greeks calculateGreeks(double spot_price, double strike_price,
                                 double time_to_expiry, double risk_free_rate,
                                 double volatility, bool is_call = true);
    
    /**
     * @brief Calculate implied volatility using Newton-Raphson method
     * @param market_price Market price of the option
     * @param spot_price Current spot price
     * @param strike_price Strike price
     * @param time_to_expiry Time to expiry in years
     * @param risk_free_rate Risk-free interest rate
     * @param is_call true for call option, false for put option
     * @param max_iterations Maximum iterations for convergence
     * @param tolerance Convergence tolerance
     */
    static double impliedVolatility(double market_price, double spot_price,
                                   double strike_price, double time_to_expiry,
                                   double risk_free_rate, bool is_call = true,
                                   int max_iterations = 100, double tolerance = 1e-6);
    
    /**
     * @brief Calculate perpetual swap synthetic price
     * @param spot_price Current spot price
     * @param funding_rate Current funding rate
     * @param funding_interval Funding interval in hours (typically 8)
     */
    static double perpetualSyntheticPrice(double spot_price, double funding_rate, 
                                         double funding_interval = 8.0);
    
    /**
     * @brief Calculate futures synthetic price using cost of carry model
     * @param spot_price Current spot price
     * @param risk_free_rate Risk-free interest rate
     * @param time_to_expiry Time to expiry in years
     * @param convenience_yield Convenience yield (for commodities)
     */
    static double futuresSyntheticPrice(double spot_price, double risk_free_rate,
                                       double time_to_expiry, double convenience_yield = 0.0);
    
    /**
     * @brief Calculate basis (futures price - spot price)
     */
    static double calculateBasis(double futures_price, double spot_price);
    
    /**
     * @brief Calculate annualized basis
     */
    static double calculateAnnualizedBasis(double basis, double spot_price, double time_to_expiry);
    
    /**
     * @brief SIMD-optimized vector operations
     */
    static void vectorAdd(const std::vector<double>& a, const std::vector<double>& b, 
                         std::vector<double>& result);
    
    static void vectorMultiply(const std::vector<double>& a, const std::vector<double>& b,
                              std::vector<double>& result);
    
    static void vectorScale(const std::vector<double>& input, double scalar,
                           std::vector<double>& result);
    
    /**
     * @brief Calculate moving average
     */
    static double movingAverage(const std::vector<double>& values, size_t window_size);
    
    /**
     * @brief Calculate exponential moving average
     */
    static double exponentialMovingAverage(const std::vector<double>& values, double alpha);
    
    /**
     * @brief Calculate standard deviation
     */
    static double standardDeviation(const std::vector<double>& values);
    
    /**
     * @brief Calculate correlation coefficient
     */
    static double correlation(const std::vector<double>& x, const std::vector<double>& y);
    
    /**
     * @brief Time utilities
     */
    static double yearsBetween(const std::chrono::system_clock::time_point& start,
                              const std::chrono::system_clock::time_point& end);
    
    static double daysBetween(const std::chrono::system_clock::time_point& start,
                             const std::chrono::system_clock::time_point& end);
    
private:
    /**
     * @brief Helper function for normal CDF approximation
     */
    static double normalCDFApproximation(double x);
    
    /**
     * @brief Helper function for Black-Scholes d1 and d2 calculation
     */
    static std::pair<double, double> calculateD1D2(double spot_price, double strike_price,
                                                   double time_to_expiry, double risk_free_rate,
                                                   double volatility);
};

/**
 * @brief High-performance memory pool for frequent allocations
 */
template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t initial_size = 1024)
        : pool_size_(initial_size), available_count_(initial_size) {
        pool_.resize(initial_size);
        available_.reserve(initial_size);
        
        // Initialize available pointers
        for (size_t i = 0; i < initial_size; ++i) {
            available_.push_back(&pool_[i]);
        }
    }
    
    ~MemoryPool() = default;
    
    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.empty()) {
            // Pool exhausted - could expand or return nullptr
            return nullptr;
        }
        
        T* ptr = available_.back();
        available_.pop_back();
        --available_count_;
        
        return ptr;
    }
    
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(ptr);
        ++available_count_;
    }
    
    size_t size() const { return pool_size_; }
    size_t available() const { return available_count_; }
    
private:
    std::vector<T> pool_;
    std::vector<T*> available_;
    size_t pool_size_;
    size_t available_count_;
    mutable std::mutex mutex_;
};

/**
 * @brief High-performance calculation pipeline for parallel processing
 */
class CalculationPipeline {
public:
    struct Statistics {
        size_t total_tasks = 0;
        size_t completed_tasks = 0;
        std::chrono::milliseconds avg_execution_time{0};
        size_t pending_tasks = 0;
    };

    explicit CalculationPipeline(size_t num_threads = std::thread::hardware_concurrency());
    ~CalculationPipeline();

    template<typename F>
    auto submit(F&& func) -> std::future<typename std::invoke_result<F>::type> {
        using ReturnType = typename std::invoke_result<F>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<F>(func)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("Pipeline is stopped");
            }
            
            tasks_.emplace([task]() { (*task)(); });
            ++total_tasks_;
        }
        
        condition_.notify_one();
        return result;
    }

    Statistics getStatistics() const;

private:
    void workerLoop();
    
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool stop_;
    size_t total_tasks_;
    size_t completed_tasks_;
};

} // namespace core
} // namespace arbitrage


