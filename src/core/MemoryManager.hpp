#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <new>

// Define cache line size as a constant value
const size_t CACHE_LINE_SIZE = 64;

namespace arbitrage {
namespace performance {

/**
 * @brief Custom allocator for market data structures with optimized memory layout
 */
template<typename T>
class MarketDataAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = MarketDataAllocator<U>;
    };

    MarketDataAllocator() noexcept = default;
    
    template<typename U>
    MarketDataAllocator(const MarketDataAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }
        
        // Align to cache line boundary (64 bytes)
        constexpr size_type alignment = 64;
        size_type bytes = n * sizeof(T);
        size_type aligned_bytes = (bytes + alignment - 1) & ~(alignment - 1);
        
        void* ptr = std::aligned_alloc(alignment, aligned_bytes);
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) noexcept {
        std::free(p);
    }

    bool operator==(const MarketDataAllocator&) const noexcept { return true; }
    bool operator!=(const MarketDataAllocator&) const noexcept { return false; }
};

/**
 * @brief Memory pool for high-frequency allocations and deallocations
 */
class MemoryPool {
private:
    struct Block {
        Block* next;
        alignas(std::max_align_t) char data[];
    };

    struct Pool {
        std::unique_ptr<char[]> memory;
        Block* free_list;
        size_t block_size;
        size_t block_count;
        std::atomic<size_t> allocated_count{0};
        std::mutex mutex;
    };

    std::unordered_map<size_t, std::unique_ptr<Pool>> pools_;
    mutable std::mutex pools_mutex_;
    
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t MIN_BLOCK_SIZE = 8;
    static constexpr size_t MAX_BLOCK_SIZE = 4096;
    static constexpr size_t DEFAULT_BLOCKS_PER_POOL = 1024;

public:
    MemoryPool() = default;
    ~MemoryPool() = default;

    // Non-copyable, non-movable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    /**
     * @brief Allocate memory from the pool
     * @param size Size in bytes
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size);

    /**
     * @brief Deallocate memory back to the pool
     * @param ptr Pointer to memory to deallocate
     * @param size Size in bytes
     */
    void deallocate(void* ptr, size_t size);

    /**
     * @brief Get pool statistics
     */
    struct PoolStats {
        size_t total_pools;
        size_t total_allocated_blocks;
        size_t total_free_blocks;
        size_t memory_usage_bytes;
    };

    PoolStats getStats() const;

private:
    Pool* getOrCreatePool(size_t size);
    size_t alignSize(size_t size) const;
};

/**
 * @brief NUMA-aware memory allocator for multi-core optimization
 */
class NUMAAllocator {
private:
    struct NUMANode {
        int node_id;
        size_t total_memory;
        size_t available_memory;
        std::vector<void*> allocated_blocks;
        std::mutex mutex;
        
        // Delete copy and move operations due to mutex
        NUMANode() = default;
        NUMANode(const NUMANode&) = delete;
        NUMANode& operator=(const NUMANode&) = delete;
        NUMANode(NUMANode&&) = delete;
        NUMANode& operator=(NUMANode&&) = delete;
    };

    std::vector<std::unique_ptr<NUMANode>> numa_nodes_;
    mutable std::mutex pools_mutex_; // Make mutex mutable to allow locking in const methods
    std::atomic<int> current_node_{0}; // Non-const atomic
    bool numa_available_;

public:
    NUMAAllocator();
    ~NUMAAllocator();

    /**
     * @brief Allocate memory on preferred NUMA node
     * @param size Size in bytes
     * @param preferred_node Preferred NUMA node (-1 for auto)
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size, int preferred_node = -1);

    /**
     * @brief Deallocate NUMA-allocated memory
     * @param ptr Pointer to memory
     * @param size Size in bytes
     */
    void deallocate(void* ptr, size_t size);

    /**
     * @brief Get current thread's preferred NUMA node
     */
    int getCurrentNUMANode() const;

    /**
     * @brief Get NUMA topology information
     */
    struct NUMATopology {
        int num_nodes;
        std::vector<size_t> node_memory;
        bool numa_available;
    };

    NUMATopology getTopology() const;

private:
    void initializeNUMA();
    int selectOptimalNode(size_t size); // Removed const to allow modifying current_node_
};

/**
 * @brief Lock-free memory allocator for zero-contention access
 */
class LockFreeAllocator {
private:
    struct FreeBlock {
        std::atomic<FreeBlock*> next;
        size_t size;
    };

    alignas(CACHE_LINE_SIZE) std::atomic<FreeBlock*> free_list_head_{nullptr};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> total_allocated_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> total_deallocated_{0};
    
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t MIN_ALLOCATION = 16;

public:
    LockFreeAllocator() = default;
    ~LockFreeAllocator();

    /**
     * @brief Allocate memory using lock-free algorithm
     * @param size Size in bytes
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size);

    /**
     * @brief Deallocate memory using lock-free algorithm
     * @param ptr Pointer to memory
     * @param size Size in bytes
     */
    void deallocate(void* ptr, size_t size);

    /**
     * @brief Get allocation statistics
     */
    struct Stats {
        size_t total_allocated;
        size_t total_deallocated;
        size_t current_usage;
        size_t free_blocks_count;
    };

    Stats getStats() const;

private:
    size_t alignSize(size_t size) const;
    FreeBlock* findBestFit(size_t size);
};

/**
 * @brief Global memory manager with different allocation strategies
 */
class MemoryManager {
private:
    std::unique_ptr<MemoryPool> memory_pool_;
    std::unique_ptr<NUMAAllocator> numa_allocator_;
    std::unique_ptr<LockFreeAllocator> lockfree_allocator_;
    
    // Strategy selection
    enum class AllocationStrategy {
        STANDARD,
        POOL,
        NUMA,
        LOCKFREE
    };
    
    std::atomic<AllocationStrategy> default_strategy_{AllocationStrategy::POOL};

public:
    MemoryManager();
    ~MemoryManager() = default;

    /**
     * @brief Get global memory manager instance
     */
    static MemoryManager& getInstance();

    /**
     * @brief Allocate memory using optimal strategy
     * @param size Size in bytes
     * @param strategy Specific strategy to use (optional)
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size, AllocationStrategy strategy = AllocationStrategy::POOL);

    /**
     * @brief Deallocate memory
     * @param ptr Pointer to memory
     * @param size Size in bytes
     * @param strategy Strategy used for allocation
     */
    void deallocate(void* ptr, size_t size, AllocationStrategy strategy = AllocationStrategy::POOL);

    /**
     * @brief Set default allocation strategy
     */
    void setDefaultStrategy(AllocationStrategy strategy);

    /**
     * @brief Get comprehensive memory statistics
     */
    struct MemoryStats {
        MemoryPool::PoolStats pool_stats;
        NUMAAllocator::NUMATopology numa_topology;
        LockFreeAllocator::Stats lockfree_stats;
        size_t total_system_memory;
        size_t available_system_memory;
    };

    MemoryStats getStats() const;

    /**
     * @brief Optimize memory layout for specific data access patterns
     */
    void optimizeForAccessPattern(const std::string& pattern_name);

    /**
     * @brief Perform memory cleanup and optimization
     */
    void cleanup();
};

} // namespace performance
} // namespace arbitrage
