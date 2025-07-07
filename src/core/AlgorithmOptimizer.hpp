#pragma once

#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <atomic>
#include "MemoryManager.hpp"

// Architecture-specific SIMD headers
#if defined(__x86_64__) || defined(_M_X64)
    // x86_64 architecture
    #include <immintrin.h>
    #include <xmmintrin.h>
    #include <emmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM64 architecture
    #include <arm_neon.h>
#endif

// Check for SIMD support
#ifdef __AVX512F__
#define SIMD_WIDTH 16  // 512-bit / 32-bit = 16 floats
#define SIMD_ALIGNMENT 64
#elif defined(__AVX2__)
#define SIMD_WIDTH 8   // 256-bit / 32-bit = 8 floats
#define SIMD_ALIGNMENT 32
#elif defined(__SSE2__)
#define SIMD_WIDTH 4   // 128-bit / 32-bit = 4 floats
#define SIMD_ALIGNMENT 16
#elif defined(__ARM_NEON) || defined(__aarch64__)
#define SIMD_WIDTH 4   // 128-bit / 32-bit = 4 floats (NEON)
#define SIMD_ALIGNMENT 16
#else
#define SIMD_WIDTH 1
#define SIMD_ALIGNMENT 4
#endif

namespace arbitrage {
namespace performance {

/**
 * @brief SIMD-optimized vector operations for pricing calculations
 */
class SIMDVectorOps {
public:
    /**
     * @brief SIMD-optimized vector addition
     * @param a First vector
     * @param b Second vector
     * @param result Output vector
     * @param size Number of elements
     */
    static void vectorAdd(const float* a, const float* b, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized vector subtraction
     */
    static void vectorSub(const float* a, const float* b, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized vector multiplication
     */
    static void vectorMul(const float* a, const float* b, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized vector division
     */
    static void vectorDiv(const float* a, const float* b, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized dot product
     */
    static float dotProduct(const float* a, const float* b, size_t size);
    
    /**
     * @brief SIMD-optimized vector scaling
     */
    static void vectorScale(const float* input, float scalar, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized exponential function
     */
    static void vectorExp(const float* input, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized logarithm function
     */
    static void vectorLog(const float* input, float* result, size_t size);
    
    /**
     * @brief SIMD-optimized square root
     */
    static void vectorSqrt(const float* input, float* result, size_t size);

private:
    static void vectorAddSSE(const float* a, const float* b, float* result, size_t size);
    static void vectorAddAVX(const float* a, const float* b, float* result, size_t size);
    static void vectorAddAVX512(const float* a, const float* b, float* result, size_t size);
};

/**
 * @brief Lock-free data structures for concurrent access
 */
template<typename T, size_t Capacity>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T> data;
        std::atomic<size_t> sequence;
    };
    
    alignas(64) std::array<Node, Capacity> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};
    
    static constexpr size_t CACHE_LINE_SIZE = 64;

public:
    LockFreeQueue() {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Try to enqueue an item
     * @param item Item to enqueue
     * @return true if successful, false if queue is full
     */
    bool tryEnqueue(const T& item) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        while (true) {
            Node& node = buffer_[pos % Capacity];
            size_t seq = node.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    node.data.store(item, std::memory_order_relaxed);
                    node.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Queue is full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Try to dequeue an item
     * @param item Output item
     * @return true if successful, false if queue is empty
     */
    bool tryDequeue(T& item) {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        
        while (true) {
            Node& node = buffer_[pos % Capacity];
            size_t seq = node.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            
            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    item = node.data.load(std::memory_order_relaxed);
                    node.sequence.store(pos + Capacity, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Queue is empty
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
    
    /**
     * @brief Get approximate queue size
     */
    size_t size() const {
        size_t enqueue = enqueue_pos_.load(std::memory_order_relaxed);
        size_t dequeue = dequeue_pos_.load(std::memory_order_relaxed);
        return enqueue - dequeue;
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        return size() == 0;
    }
};

/**
 * @brief Cache-optimized data structures for CPU performance
 */
template<typename T>
class CacheOptimizedVector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;
    size_t alignment_;
    
    static constexpr size_t DEFAULT_ALIGNMENT = 64; // Cache line size

public:
    explicit CacheOptimizedVector(size_t initial_capacity = 1024, size_t alignment = DEFAULT_ALIGNMENT)
        : size_(0), capacity_(initial_capacity), alignment_(alignment) {
        data_ = static_cast<T*>(std::aligned_alloc(alignment_, capacity_ * sizeof(T)));
        if (!data_) {
            throw std::bad_alloc();
        }
    }
    
    ~CacheOptimizedVector() {
        if (data_) {
            for (size_t i = 0; i < size_; ++i) {
                data_[i].~T();
            }
            std::free(data_);
        }
    }
    
    // Move constructor
    CacheOptimizedVector(CacheOptimizedVector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_), alignment_(other.alignment_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    // Move assignment
    CacheOptimizedVector& operator=(CacheOptimizedVector&& other) noexcept {
        if (this != &other) {
            this->~CacheOptimizedVector();
            new (this) CacheOptimizedVector(std::move(other));
        }
        return *this;
    }
    
    // Disable copy operations
    CacheOptimizedVector(const CacheOptimizedVector&) = delete;
    CacheOptimizedVector& operator=(const CacheOptimizedVector&) = delete;
    
    /**
     * @brief Add element to vector
     */
    void push_back(const T& value) {
        if (size_ >= capacity_) {
            resize(capacity_ * 2);
        }
        new (data_ + size_) T(value);
        ++size_;
    }
    
    /**
     * @brief Add element to vector (move version)
     */
    void push_back(T&& value) {
        if (size_ >= capacity_) {
            resize(capacity_ * 2);
        }
        new (data_ + size_) T(std::move(value));
        ++size_;
    }
    
    /**
     * @brief Access element by index
     */
    T& operator[](size_t index) {
        return data_[index];
    }
    
    const T& operator[](size_t index) const {
        return data_[index];
    }
    
    /**
     * @brief Get size
     */
    size_t size() const { return size_; }
    
    /**
     * @brief Get capacity
     */
    size_t capacity() const { return capacity_; }
    
    /**
     * @brief Get raw data pointer
     */
    T* data() { return data_; }
    const T* data() const { return data_; }
    
    /**
     * @brief Iterator support
     */
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    void resize(size_t new_capacity) {
        T* new_data = static_cast<T*>(std::aligned_alloc(alignment_, new_capacity * sizeof(T)));
        if (!new_data) {
            throw std::bad_alloc();
        }
        
        // Move existing elements
        for (size_t i = 0; i < size_; ++i) {
            new (new_data + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        
        std::free(data_);
        data_ = new_data;
        capacity_ = new_capacity;
    }
};

/**
 * @brief Optimized synthetic construction algorithms
 */
class OptimizedSyntheticConstruction {
public:
    /**
     * @brief Vectorized batch processing for multiple instruments
     */
    struct InstrumentBatch {
        std::vector<float> spot_prices;
        std::vector<float> funding_rates;
        std::vector<float> time_to_expiry;
        std::vector<float> volatilities;
        std::vector<float> risk_free_rates;
        
        size_t size() const { return spot_prices.size(); }
    };
    
    /**
     * @brief SIMD-optimized perpetual swap pricing for batch
     */
    static void calculatePerpetualPrices(const InstrumentBatch& batch, std::vector<float>& results);
    
    /**
     * @brief SIMD-optimized futures pricing for batch
     */
    static void calculateFuturesPrices(const InstrumentBatch& batch, std::vector<float>& results);
    
    /**
     * @brief SIMD-optimized Black-Scholes pricing for batch
     */
    static void calculateOptionsPrices(const InstrumentBatch& batch, std::vector<float>& call_prices, std::vector<float>& put_prices);
    
    /**
     * @brief Optimized arbitrage opportunity detection
     */
    struct ArbitrageOpportunity {
        float expected_profit;
        float required_capital;
        float risk_score;
        float confidence;
        uint32_t instrument_id_1;
        uint32_t instrument_id_2;
    };
    
    static void detectArbitrageOpportunities(
        const std::vector<float>& real_prices,
        const std::vector<float>& synthetic_prices,
        const std::vector<float>& transaction_costs,
        std::vector<ArbitrageOpportunity>& opportunities,
        float min_profit_threshold = 0.001f
    );

private:
    /**
     * @brief Fast approximation functions for mathematical operations
     */
    static float fastExp(float x);
    static float fastLog(float x);
    static float fastSqrt(float x);
    static float fastNormalCDF(float x);
};

/**
 * @brief Branch prediction optimization for hot code paths
 */
class BranchOptimization {
public:
    /**
     * @brief Likely/unlikely macros for branch prediction
     */
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    
    /**
     * @brief Optimized conditional execution for pricing logic
     */
    template<typename Condition, typename TrueFunc, typename FalseFunc>
    static auto conditionalExecute(Condition&& cond, TrueFunc&& true_func, FalseFunc&& false_func) {
        if (LIKELY(cond)) {
            return true_func();
        } else {
            return false_func();
        }
    }
    
    /**
     * @brief Profile-guided optimization hints
     */
    static void profileHint(const std::string& location, bool condition_result);
    
private:
    static std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> profile_data_;
};

/**
 * @brief Algorithm optimization manager
 */
class AlgorithmOptimizer {
private:
    std::unique_ptr<MemoryManager> memory_manager_;
    bool simd_enabled_;
    bool cache_optimization_enabled_;
    bool branch_optimization_enabled_;

public:
    AlgorithmOptimizer();
    ~AlgorithmOptimizer() = default;

    /**
     * @brief Initialize optimization features
     */
    void initialize();
    
    /**
     * @brief Enable/disable SIMD optimizations
     */
    void enableSIMD(bool enable) { simd_enabled_ = enable; }
    
    /**
     * @brief Enable/disable cache optimizations
     */
    void enableCacheOptimization(bool enable) { cache_optimization_enabled_ = enable; }
    
    /**
     * @brief Enable/disable branch optimizations
     */
    void enableBranchOptimization(bool enable) { branch_optimization_enabled_ = enable; }
    
    /**
     * @brief Get optimization statistics
     */
    struct OptimizationStats {
        bool simd_available;
        std::string simd_features;
        size_t cache_line_size;
        size_t l1_cache_size;
        size_t l2_cache_size;
        size_t l3_cache_size;
        float simd_speedup_factor;
        float cache_hit_ratio;
    };
    
    OptimizationStats getStats() const;
    
    /**
     * @brief Benchmark different optimization strategies
     */
    void benchmarkOptimizations();

private:
    void detectSIMDCapabilities();
    void detectCacheTopology();
    void optimizeMemoryLayout();
};

} // namespace performance
} // namespace arbitrage
