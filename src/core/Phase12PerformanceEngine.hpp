#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <string>
#include <mutex>
#include <thread>
// Only include x86 SIMD headers on x86/x64 architectures
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <immintrin.h>
#endif

namespace arbitrage {
namespace performance {

// Forward declarations
class MemoryPool;
class SIMDProcessor;
class NetworkOptimizer;

/**
 * Advanced Memory Management with NUMA awareness and custom allocators
 */
class AdvancedMemoryManager {
public:
    enum class AllocatorType {
        MARKET_DATA,
        CALCULATIONS,
        RESULTS,
        NETWORK_BUFFERS,
        CACHE_ALIGNED
    };

    struct MemoryStats {
        size_t total_allocated = 0;
        size_t peak_usage = 0;
        size_t current_usage = 0;
        size_t fragmentation_ratio = 0;
        size_t allocation_count = 0;
        size_t deallocation_count = 0;
        double average_allocation_time_ns = 0.0;
        size_t numa_node_usage[8] = {0}; // Support up to 8 NUMA nodes
    };

    struct PoolConfig {
        size_t block_size = 0;
        size_t initial_blocks = 0;
        size_t max_blocks = 0;
        bool numa_aware = true;
        int preferred_numa_node = -1;
    };

    AdvancedMemoryManager();
    ~AdvancedMemoryManager();

    // Pool management
    bool createPool(const std::string& name, const PoolConfig& config);
    void* allocate(const std::string& pool_name, size_t size, size_t alignment = 64);
    void deallocate(const std::string& pool_name, void* ptr);
    
    // NUMA operations
    void setNUMAAffinity(int numa_node);
    int getCurrentNUMANode();
    std::vector<int> getAvailableNUMANodes();
    
    // Memory prefetching
    void prefetch(const void* addr, size_t size);
    void prefetchAsync(const void* addr, size_t size);
    
    // Statistics
    MemoryStats getGlobalStats() const;
    MemoryStats getPoolStats(const std::string& pool_name) const;
    void resetStats();
    
    // Cache optimization
    void optimizeCacheLayout();
    size_t getCacheLineSize() const;
    
private:
    std::unordered_map<std::string, std::unique_ptr<MemoryPool>> pools_;
    mutable std::mutex stats_mutex_;
    MemoryStats global_stats_;
    std::atomic<bool> numa_initialized_{false};
    
    void initializeNUMA();
    void updateStats(size_t allocated, bool is_allocation);
};

/**
 * High-performance SIMD and GPU acceleration engine
 */
class AdvancedProcessingEngine {
public:
    enum class ProcessorType {
        CPU_SCALAR,
        CPU_SSE,
        CPU_AVX2,
        CPU_AVX512,
        GPU_CUDA,
        GPU_OPENCL
    };

    struct ProcessingStats {
        uint64_t operations_processed = 0;
        double total_processing_time_ms = 0.0;
        double average_operation_time_ns = 0.0;
        uint64_t simd_instructions_executed = 0;
        uint64_t gpu_kernel_launches = 0;
        double gpu_utilization_percent = 0.0;
        double cpu_utilization_percent = 0.0;
        size_t memory_bandwidth_mbps = 0;
        double cache_hit_ratio = 0.0;
    };

    struct OptimizationConfig {
        bool enable_avx512 = true;
        bool enable_gpu = true;
        bool enable_parallel = true;
        size_t thread_count = 0; // 0 = auto-detect
        size_t batch_size = 1024;
        bool prefer_gpu_for_large_datasets = true;
        size_t gpu_threshold_elements = 10000;
    };

    AdvancedProcessingEngine();
    ~AdvancedProcessingEngine();

    // Initialization
    bool initialize(const OptimizationConfig& config);
    bool detectCapabilities();
    
    // SIMD Operations
    void vectorizedMultiply(const float* a, const float* b, float* result, size_t count);
    void vectorizedAdd(const float* a, const float* b, float* result, size_t count);
    void vectorizedFMA(const float* a, const float* b, const float* c, float* result, size_t count);
    
    // Complex financial calculations
    void calculateBlackScholes(const float* spot, const float* strike, const float* time_to_expiry,
                              const float* volatility, const float* risk_free_rate,
                              float* call_prices, float* put_prices, size_t count);
    
    void calculateVolatilitySurface(const float* strikes, const float* expiries,
                                   const float* market_prices, float* implied_vols,
                                   size_t strike_count, size_t expiry_count);
    
    // GPU acceleration
    bool initializeGPU();
    void gpuMatrixMultiply(const float* a, const float* b, float* c, 
                          size_t m, size_t n, size_t k);
    void gpuFFT(const float* input, float* output, size_t size);
    
    // Parallel processing
    template<typename Func>
    void parallelFor(size_t start, size_t end, Func&& func);
    
    // Statistics and monitoring
    ProcessingStats getStats() const;
    void resetStats();
    ProcessorType getBestProcessor(size_t data_size) const;
    
private:
    OptimizationConfig config_;
    ProcessingStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Capability flags
    bool avx512_available_ = false;
    bool gpu_available_ = false;
    bool cuda_available_ = false;
    bool opencl_available_ = false;
    
    // GPU context
    void* gpu_context_ = nullptr;
    void* cuda_context_ = nullptr;
    
    void detectSIMDCapabilities();
    void detectGPUCapabilities();
    ProcessorType selectOptimalProcessor(size_t data_size) const;
};

/**
 * Ultra-low latency network optimization
 */
class AdvancedNetworkOptimizer {
public:
    struct NetworkStats {
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        double average_latency_ns = 0.0;
        double min_latency_ns = 0.0;
        double max_latency_ns = 0.0;
        double jitter_ns = 0.0;
        uint64_t dropped_packets = 0;
        double bandwidth_utilization_percent = 0.0;
        bool dpdk_enabled = false;
        bool hardware_timestamping_enabled = false;
    };

    struct OptimizationConfig {
        bool enable_dpdk = true;
        bool enable_kernel_bypass = true;
        bool enable_hardware_timestamping = true;
        bool enable_multicast_optimization = true;
        std::string preferred_interface;
        size_t receive_buffer_size = 2 * 1024 * 1024; // 2MB
        size_t send_buffer_size = 2 * 1024 * 1024;    // 2MB
        bool enable_cpu_affinity = true;
        std::vector<int> cpu_cores;
    };

    AdvancedNetworkOptimizer();
    ~AdvancedNetworkOptimizer();

    // Initialization
    bool initialize(const OptimizationConfig& config);
    bool initializeDPDK();
    
    // Connection optimization
    bool optimizeSocketSettings(int socket_fd);
    bool enableHardwareTimestamping(int socket_fd);
    bool setCPUAffinity(const std::vector<int>& cores);
    
    // Latency measurement
    uint64_t getHardwareTimestamp();
    double measureRoundTripLatency(const std::string& target_host, int port);
    
    // Buffer management
    void* allocateNetworkBuffer(size_t size);
    void deallocateNetworkBuffer(void* buffer);
    
    // Multicast optimization
    bool joinMulticastGroup(const std::string& group_address, int port);
    bool optimizeMulticastReceive();
    
    // Statistics
    NetworkStats getStats() const;
    void resetStats();
    void updateLatencyStats(double latency_ns);
    
    // Network topology optimization
    std::vector<std::string> getOptimalExchangeRoutes();
    bool optimizeRoutingTable();
    
private:
    OptimizationConfig config_;
    NetworkStats stats_;
    mutable std::mutex stats_mutex_;
    
    bool dpdk_initialized_ = false;
    void* dpdk_context_ = nullptr;
    
    std::vector<void*> network_buffers_;
    std::mutex buffer_mutex_;
    
    void initializeNetworkBuffers();
    void cleanupNetworkBuffers();
    bool detectNetworkCapabilities();
};

/**
 * Main Performance Optimization Engine that coordinates all subsystems
 */
class Phase12PerformanceEngine {
public:
    struct SystemMetrics {
        AdvancedMemoryManager::MemoryStats memory_stats;
        AdvancedProcessingEngine::ProcessingStats processing_stats;
        AdvancedNetworkOptimizer::NetworkStats network_stats;
        
        // System-wide metrics
        double cpu_usage_percent = 0.0;
        double memory_usage_percent = 0.0;
        double network_usage_percent = 0.0;
        uint64_t cache_misses = 0;
        uint64_t branch_mispredictions = 0;
        double thermal_throttling_percent = 0.0;
        
        std::chrono::steady_clock::time_point timestamp;
    };

    struct OptimizationProfile {
        std::string name;
        AdvancedMemoryManager::PoolConfig memory_config;
        AdvancedProcessingEngine::OptimizationConfig processing_config;
        AdvancedNetworkOptimizer::OptimizationConfig network_config;
        bool auto_tune = true;
        double target_latency_ns = 1000.0; // 1 microsecond
    };

    Phase12PerformanceEngine();
    ~Phase12PerformanceEngine();

    // Initialization
    bool initialize();
    bool loadOptimizationProfile(const std::string& profile_name);
    bool saveOptimizationProfile(const std::string& profile_name, const OptimizationProfile& profile);
    
    // Component access
    AdvancedMemoryManager& getMemoryManager() { return *memory_manager_; }
    AdvancedProcessingEngine& getProcessingEngine() { return *processing_engine_; }
    AdvancedNetworkOptimizer& getNetworkOptimizer() { return *network_optimizer_; }
    
    // System optimization
    bool optimizeForLatency();
    bool optimizeForThroughput();
    bool optimizeForMemoryUsage();
    bool autoTune();
    
    // Monitoring and analytics
    SystemMetrics getCurrentMetrics();
    std::vector<SystemMetrics> getHistoricalMetrics(size_t count = 100);
    void startContinuousMonitoring(std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    void stopContinuousMonitoring();
    
    // Performance analysis
    bool analyzeBottlenecks();
    std::vector<std::string> getOptimizationRecommendations();
    double calculatePerformanceScore();
    
    // Regression testing
    bool runPerformanceBenchmark();
    bool compareWithBaseline(const std::string& baseline_name);
    void saveCurrentAsBaseline(const std::string& baseline_name);

private:
    std::unique_ptr<AdvancedMemoryManager> memory_manager_;
    std::unique_ptr<AdvancedProcessingEngine> processing_engine_;
    std::unique_ptr<AdvancedNetworkOptimizer> network_optimizer_;
    
    // Monitoring
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    std::vector<SystemMetrics> metrics_history_;
    mutable std::mutex metrics_mutex_;
    
    // Optimization profiles
    std::unordered_map<std::string, OptimizationProfile> optimization_profiles_;
    std::string current_profile_;
    
    void monitoringLoop(std::chrono::milliseconds interval);
    void collectSystemMetrics(SystemMetrics& metrics);
    void initializeDefaultProfiles();
};

} // namespace performance
} // namespace arbitrage
