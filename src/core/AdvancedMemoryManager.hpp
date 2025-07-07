#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

namespace arbitrage {
namespace performance {

class AdvancedMemoryManager {
public:
    struct MemoryStats {
        std::size_t total_allocated;
        std::size_t current_usage;
        std::size_t peak_usage;
        std::size_t allocation_count;
        std::size_t deallocation_count;
        double average_allocation_time_ns;
        double fragmentation_ratio;
    };

    struct PoolConfig {
        std::size_t block_size;
        std::size_t initial_blocks;
        std::size_t max_blocks;
        bool numa_aware;
        int numa_node;
        bool enable_prefetch;
        bool enable_cache_align;
        std::size_t cache_line_size;
    };

    struct PoolStats {
        std::size_t total_blocks;
        std::size_t used_blocks;
        std::size_t free_blocks;
        std::size_t peak_blocks;
        double utilization_ratio;
        double fragmentation_ratio;
    };

    AdvancedMemoryManager();
    ~AdvancedMemoryManager();

    // Pool management
    void createPool(const std::string& pool_name, const PoolConfig& config);
    void destroyPool(const std::string& pool_name);
    PoolStats getPoolStats(const std::string& pool_name) const;

    // Memory allocation
    void* allocate(const std::string& pool_name, std::size_t size);
    void deallocate(const std::string& pool_name, void* ptr);
    void* allocateAligned(const std::string& pool_name, std::size_t size, std::size_t alignment);
    void deallocateAligned(const std::string& pool_name, void* ptr);

    // Memory optimization
    void prefetch(void* ptr, std::size_t size);
    void prefetchForRead(void* ptr, std::size_t size);
    void prefetchForWrite(void* ptr, std::size_t size);
    void flushCache(void* ptr, std::size_t size);
    void nonTemporalStore(void* ptr, const void* data, std::size_t size);

    // NUMA operations
    int getCurrentNUMANode() const;
    std::vector<int> getAvailableNUMANodes() const;
    void bindToNUMANode(int node);
    void* allocateOnNode(std::size_t size, int node);
    void deallocateOnNode(void* ptr, std::size_t size, int node);

    // Cache operations
    std::size_t getCacheLineSize() const;
    void* allocateCacheAligned(std::size_t size);
    void deallocateCacheAligned(void* ptr);
    void touchPages(void* ptr, std::size_t size);

    // Memory fence operations
    void memoryFence();
    void loadFence();
    void storeFence();

    // Statistics and monitoring
    MemoryStats getGlobalStats() const;
    void resetStats();
    void enableLeakDetection(bool enable);
    void dumpLeakReport() const;

private:
    struct MemoryPool;
    std::unordered_map<std::string, std::unique_ptr<MemoryPool>> pools_;
    std::atomic<std::size_t> total_allocated_;
    std::atomic<std::size_t> current_usage_;
    std::atomic<std::size_t> peak_usage_;
    std::atomic<std::size_t> allocation_count_;
    std::atomic<std::size_t> deallocation_count_;
    std::atomic<double> total_allocation_time_ns_;
    mutable std::mutex mutex_;
    bool leak_detection_enabled_;

    // Internal helper functions
    void updateStats(std::size_t size, bool is_allocation);
    void recordAllocationTime(double time_ns);
    MemoryPool* getPool(const std::string& pool_name);
    const MemoryPool* getPool(const std::string& pool_name) const;
    void validatePoolName(const std::string& pool_name) const;
    void validateAlignment(std::size_t alignment) const;
};

} // namespace performance
} // namespace arbitrage 