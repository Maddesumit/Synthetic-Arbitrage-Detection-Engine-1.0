#include "MemoryManager.hpp"
#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <numa.h>
#include <numaif.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace arbitrage {
namespace performance {

// MemoryPool Implementation
void* MemoryPool::allocate(size_t size) {
    if (size == 0 || size > MAX_BLOCK_SIZE) {
        // Fall back to standard allocation for large blocks
        return std::malloc(size);
    }

    size_t aligned_size = alignSize(size);
    Pool* pool = getOrCreatePool(aligned_size);
    
    std::lock_guard<std::mutex> lock(pool->mutex);
    
    if (pool->free_list) {
        Block* block = pool->free_list;
        pool->free_list = block->next;
        pool->allocated_count.fetch_add(1, std::memory_order_relaxed);
        return block->data;
    }
    
    // Pool is empty, fall back to standard allocation
    return std::malloc(size);
}

void MemoryPool::deallocate(void* ptr, size_t size) {
    if (!ptr) return;
    
    if (size == 0 || size > MAX_BLOCK_SIZE) {
        std::free(ptr);
        return;
    }

    size_t aligned_size = alignSize(size);
    
    // Find the appropriate pool
    std::lock_guard<std::mutex> pools_lock(pools_mutex_);
    auto it = pools_.find(aligned_size);
    if (it == pools_.end()) {
        std::free(ptr);
        return;
    }

    Pool* pool = it->second.get();
    std::lock_guard<std::mutex> lock(pool->mutex);
    
    // Return block to free list
    Block* block = reinterpret_cast<Block*>(static_cast<char*>(ptr) - offsetof(Block, data));
    block->next = pool->free_list;
    pool->free_list = block;
    pool->allocated_count.fetch_sub(1, std::memory_order_relaxed);
}

MemoryPool::Pool* MemoryPool::getOrCreatePool(size_t size) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(size);
    if (it != pools_.end()) {
        return it->second.get();
    }

    // Create new pool
    auto pool = std::make_unique<Pool>();
    pool->block_size = size;
    pool->block_count = DEFAULT_BLOCKS_PER_POOL;
    
    size_t total_size = (sizeof(Block) + size) * pool->block_count;
    pool->memory = std::make_unique<char[]>(total_size);
    
    // Initialize free list
    pool->free_list = nullptr;
    char* current = pool->memory.get();
    
    for (size_t i = 0; i < pool->block_count; ++i) {
        Block* block = reinterpret_cast<Block*>(current);
        block->next = pool->free_list;
        pool->free_list = block;
        current += sizeof(Block) + size;
    }
    
    Pool* result = pool.get();
    pools_[size] = std::move(pool);
    return result;
}

size_t MemoryPool::alignSize(size_t size) const {
    // Align to 8-byte boundary minimum
    constexpr size_t alignment = 8;
    return (size + alignment - 1) & ~(alignment - 1);
}

MemoryPool::PoolStats MemoryPool::getStats() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    PoolStats stats{};
    stats.total_pools = pools_.size();
    
    for (const auto& [size, pool] : pools_) {
        stats.total_allocated_blocks += pool->allocated_count.load(std::memory_order_relaxed);
        stats.total_free_blocks += (pool->block_count - pool->allocated_count.load(std::memory_order_relaxed));
        stats.memory_usage_bytes += pool->block_count * (sizeof(Block) + size);
    }
    
    return stats;
}

// NUMAAllocator Implementation
NUMAAllocator::NUMAAllocator() : numa_available_(false) {
    initializeNUMA();
}

NUMAAllocator::~NUMAAllocator() {
    // Clean up all allocated blocks
    for (auto& node : numa_nodes_) {
        std::lock_guard<std::mutex> lock(node->mutex);
        for (void* ptr : node->allocated_blocks) {
            std::free(ptr);
        }
    }
}

void NUMAAllocator::initializeNUMA() {
#ifdef __linux__
    if (numa_available() >= 0) {
        numa_available_ = true;
        int max_node = numa_max_node();
        numa_nodes_.reserve(max_node + 1);
        
        for (int i = 0; i <= max_node; ++i) {
            auto node = std::make_unique<NUMANode>();
            node->node_id = i;
            node->total_memory = numa_node_size64(i, nullptr);
            node->available_memory = node->total_memory;
            numa_nodes_.push_back(std::move(node));
        }
    }
#endif
    
    if (!numa_available_) {
        // Create single node for non-NUMA systems
        auto node = std::make_unique<NUMANode>();
        node->node_id = 0;
        node->total_memory = 0; // Will be determined dynamically
        node->available_memory = 0;
        numa_nodes_.push_back(std::move(node));
    }
}

void* NUMAAllocator::allocate(size_t size, int preferred_node) {
    if (!numa_available_) {
        return std::malloc(size);
    }

    int node = (preferred_node >= 0) ? preferred_node : selectOptimalNode(size);
    if (node >= static_cast<int>(numa_nodes_.size())) {
        node = 0;
    }

    void* ptr = nullptr;
    
#ifdef __linux__
    // Allocate on specific NUMA node
    ptr = numa_alloc_onnode(size, node);
#else
    ptr = std::malloc(size);
#endif

    if (ptr) {
        std::lock_guard<std::mutex> lock(numa_nodes_[node]->mutex);
        numa_nodes_[node]->allocated_blocks.push_back(ptr);
        numa_nodes_[node]->available_memory -= size;
    }

    return ptr;
}

void NUMAAllocator::deallocate(void* ptr, size_t size) {
    if (!ptr) return;

#ifdef __linux__
    if (numa_available_) {
        numa_free(ptr, size);
        
        // Remove from tracking (find which node it belongs to)
        for (auto& node : numa_nodes_) {
            std::lock_guard<std::mutex> lock(node->mutex);
            auto it = std::find(node->allocated_blocks.begin(), node->allocated_blocks.end(), ptr);
            if (it != node->allocated_blocks.end()) {
                node->allocated_blocks.erase(it);
                node->available_memory += size;
                break;
            }
        }
        return;
    }
#endif

    std::free(ptr);
}

int NUMAAllocator::getCurrentNUMANode() const {
#ifdef __linux__
    if (numa_available_) {
        return numa_node_of_cpu(sched_getcpu());
    }
#endif
    return 0;
}

int NUMAAllocator::selectOptimalNode(size_t size) {
    if (!numa_available_ || numa_nodes_.empty()) {
        return 0;
    }

    // Round-robin with preference for nodes with more available memory
    int best_node = current_node_.load(std::memory_order_relaxed);
    size_t best_available = 0;
    
    for (size_t i = 0; i < numa_nodes_.size(); ++i) {
        int node_idx = (best_node + i) % numa_nodes_.size();
        size_t available = numa_nodes_[node_idx]->available_memory;
        
        if (available >= size && available > best_available) {
            best_node = node_idx;
            best_available = available;
        }
    }
    
    // Update current node for next allocation
    int next_node = (best_node + 1) % numa_nodes_.size();
    current_node_.store(next_node, std::memory_order_relaxed);
    return best_node;
}

NUMAAllocator::NUMATopology NUMAAllocator::getTopology() const {
    NUMATopology topology;
    topology.numa_available = numa_available_;
    topology.num_nodes = static_cast<int>(numa_nodes_.size());
    
    for (const auto& node : numa_nodes_) {
        topology.node_memory.push_back(node->total_memory);
    }
    
    return topology;
}

// LockFreeAllocator Implementation
LockFreeAllocator::~LockFreeAllocator() {
    // Clean up all remaining blocks
    FreeBlock* current = free_list_head_.load(std::memory_order_acquire);
    while (current) {
        FreeBlock* next = current->next.load(std::memory_order_relaxed);
        std::free(current);
        current = next;
    }
}

void* LockFreeAllocator::allocate(size_t size) {
    size_t aligned_size = alignSize(size);
    
    // Try to find a suitable free block
    FreeBlock* block = findBestFit(aligned_size);
    if (block) {
        total_allocated_.fetch_add(aligned_size, std::memory_order_relaxed);
        return reinterpret_cast<char*>(block) + sizeof(FreeBlock);
    }
    
    // No suitable block found, allocate new memory
    size_t total_size = sizeof(FreeBlock) + aligned_size;
    void* ptr = std::malloc(total_size);
    if (!ptr) {
        return nullptr;
    }
    
    FreeBlock* new_block = static_cast<FreeBlock*>(ptr);
    new_block->size = aligned_size;
    new_block->next.store(nullptr, std::memory_order_relaxed);
    
    total_allocated_.fetch_add(aligned_size, std::memory_order_relaxed);
    return reinterpret_cast<char*>(new_block) + sizeof(FreeBlock);
}

void LockFreeAllocator::deallocate(void* ptr, size_t size) {
    if (!ptr) return;
    
    size_t aligned_size = alignSize(size);
    FreeBlock* block = reinterpret_cast<FreeBlock*>(static_cast<char*>(ptr) - sizeof(FreeBlock));
    block->size = aligned_size;
    
    // Add to free list using lock-free algorithm
    FreeBlock* current_head = free_list_head_.load(std::memory_order_acquire);
    do {
        block->next.store(current_head, std::memory_order_relaxed);
    } while (!free_list_head_.compare_exchange_weak(current_head, block, 
                                                   std::memory_order_release, 
                                                   std::memory_order_acquire));
    
    total_deallocated_.fetch_add(aligned_size, std::memory_order_relaxed);
}

size_t LockFreeAllocator::alignSize(size_t size) const {
    size = std::max(size, MIN_ALLOCATION);
    // Align to pointer size
    constexpr size_t alignment = sizeof(void*);
    return (size + alignment - 1) & ~(alignment - 1);
}

LockFreeAllocator::FreeBlock* LockFreeAllocator::findBestFit(size_t size) {
    FreeBlock* prev = nullptr;
    FreeBlock* current = free_list_head_.load(std::memory_order_acquire);
    FreeBlock* best_fit = nullptr;
    FreeBlock* best_prev = nullptr;
    
    while (current) {
        FreeBlock* next = current->next.load(std::memory_order_acquire);
        
        if (current->size >= size) {
            if (!best_fit || current->size < best_fit->size) {
                best_fit = current;
                best_prev = prev;
            }
        }
        
        prev = current;
        current = next;
    }
    
    if (best_fit) {
        // Remove from free list
        if (best_prev) {
            best_prev->next.store(best_fit->next.load(std::memory_order_relaxed), 
                                 std::memory_order_release);
        } else {
            FreeBlock* expected = best_fit;
            FreeBlock* next = best_fit->next.load(std::memory_order_relaxed);
            free_list_head_.compare_exchange_strong(expected, next, 
                                                   std::memory_order_release, 
                                                   std::memory_order_acquire);
        }
    }
    
    return best_fit;
}

LockFreeAllocator::Stats LockFreeAllocator::getStats() const {
    Stats stats;
    stats.total_allocated = total_allocated_.load(std::memory_order_relaxed);
    stats.total_deallocated = total_deallocated_.load(std::memory_order_relaxed);
    stats.current_usage = stats.total_allocated - stats.total_deallocated;
    
    // Count free blocks
    stats.free_blocks_count = 0;
    FreeBlock* current = free_list_head_.load(std::memory_order_acquire);
    while (current) {
        stats.free_blocks_count++;
        current = current->next.load(std::memory_order_acquire);
    }
    
    return stats;
}

// MemoryManager Implementation
MemoryManager::MemoryManager() 
    : memory_pool_(std::make_unique<MemoryPool>())
    , numa_allocator_(std::make_unique<NUMAAllocator>())
    , lockfree_allocator_(std::make_unique<LockFreeAllocator>()) {
}

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

void* MemoryManager::allocate(size_t size, AllocationStrategy strategy) {
    if (strategy == AllocationStrategy::POOL) {
        strategy = default_strategy_.load(std::memory_order_relaxed);
    }
    
    switch (strategy) {
        case AllocationStrategy::POOL:
            return memory_pool_->allocate(size);
        case AllocationStrategy::NUMA:
            return numa_allocator_->allocate(size);
        case AllocationStrategy::LOCKFREE:
            return lockfree_allocator_->allocate(size);
        case AllocationStrategy::STANDARD:
        default:
            return std::malloc(size);
    }
}

void MemoryManager::deallocate(void* ptr, size_t size, AllocationStrategy strategy) {
    if (strategy == AllocationStrategy::POOL) {
        strategy = default_strategy_.load(std::memory_order_relaxed);
    }
    
    switch (strategy) {
        case AllocationStrategy::POOL:
            memory_pool_->deallocate(ptr, size);
            break;
        case AllocationStrategy::NUMA:
            numa_allocator_->deallocate(ptr, size);
            break;
        case AllocationStrategy::LOCKFREE:
            lockfree_allocator_->deallocate(ptr, size);
            break;
        case AllocationStrategy::STANDARD:
        default:
            std::free(ptr);
            break;
    }
}

void MemoryManager::setDefaultStrategy(AllocationStrategy strategy) {
    default_strategy_.store(strategy, std::memory_order_relaxed);
}

MemoryManager::MemoryStats MemoryManager::getStats() const {
    MemoryStats stats;
    stats.pool_stats = memory_pool_->getStats();
    stats.numa_topology = numa_allocator_->getTopology();
    stats.lockfree_stats = lockfree_allocator_->getStats();
    
    // Get system memory information
#ifdef _WIN32
    MEMORYSTATUSEX memory_status;
    memory_status.dwLength = sizeof(memory_status);
    if (GlobalMemoryStatusEx(&memory_status)) {
        stats.total_system_memory = memory_status.ullTotalPhys;
        stats.available_system_memory = memory_status.ullAvailPhys;
    }
#elif defined(__linux__)
    // Read from /proc/meminfo
    stats.total_system_memory = 0;
    stats.available_system_memory = 0;
    // Implementation would read from /proc/meminfo
#endif
    
    return stats;
}

void MemoryManager::optimizeForAccessPattern(const std::string& pattern_name) {
    if (pattern_name == "sequential") {
        setDefaultStrategy(AllocationStrategy::POOL);
    } else if (pattern_name == "random") {
        setDefaultStrategy(AllocationStrategy::LOCKFREE);
    } else if (pattern_name == "numa_aware") {
        setDefaultStrategy(AllocationStrategy::NUMA);
    } else {
        setDefaultStrategy(AllocationStrategy::STANDARD);
    }
}

void MemoryManager::cleanup() {
    // This would trigger garbage collection and memory compaction
    // Implementation depends on specific allocator capabilities
}

} // namespace performance
} // namespace arbitrage
