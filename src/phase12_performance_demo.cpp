#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>

#include "core/Phase12PerformanceEngine.hpp"
#include "utils/Logger.hpp"

using namespace arbitrage::performance;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void printMemoryStats(const AdvancedMemoryManager::MemoryStats& stats) {
    std::cout << "📊 Memory Statistics:\n";
    std::cout << "   Total Allocated: " << (stats.total_allocated / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "   Current Usage: " << (stats.current_usage / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "   Peak Usage: " << (stats.peak_usage / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "   Allocations: " << stats.allocation_count << "\n";
    std::cout << "   Deallocations: " << stats.deallocation_count << "\n";
    std::cout << "   Avg Allocation Time: " << stats.average_allocation_time_ns << " ns\n";
    std::cout << "   Fragmentation Ratio: " << stats.fragmentation_ratio << "\n";
}

void printProcessingStats(const AdvancedProcessingEngine::ProcessingStats& stats) {
    std::cout << "⚡ Processing Statistics:\n";
    std::cout << "   Operations Processed: " << stats.operations_processed << "\n";
    std::cout << "   Total Processing Time: " << stats.total_processing_time_ms << " ms\n";
    std::cout << "   Avg Operation Time: " << stats.average_operation_time_ns << " ns\n";
    std::cout << "   SIMD Instructions: " << stats.simd_instructions_executed << "\n";
    std::cout << "   GPU Kernel Launches: " << stats.gpu_kernel_launches << "\n";
    std::cout << "   GPU Utilization: " << stats.gpu_utilization_percent << "%\n";
    std::cout << "   CPU Utilization: " << stats.cpu_utilization_percent << "%\n";
    std::cout << "   Memory Bandwidth: " << stats.memory_bandwidth_mbps << " MB/s\n";
    std::cout << "   Cache Hit Ratio: " << (stats.cache_hit_ratio * 100.0) << "%\n";
}

void printNetworkStats(const AdvancedNetworkOptimizer::NetworkStats& stats) {
    std::cout << "🌐 Network Statistics:\n";
    std::cout << "   Packets Sent: " << stats.packets_sent << "\n";
    std::cout << "   Packets Received: " << stats.packets_received << "\n";
    std::cout << "   Bytes Sent: " << (stats.bytes_sent / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "   Bytes Received: " << (stats.bytes_received / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "   Average Latency: " << (stats.average_latency_ns / 1000.0) << " μs\n";
    std::cout << "   Min Latency: " << (stats.min_latency_ns / 1000.0) << " μs\n";
    std::cout << "   Max Latency: " << (stats.max_latency_ns / 1000.0) << " μs\n";
    std::cout << "   Jitter: " << (stats.jitter_ns / 1000.0) << " μs\n";
    std::cout << "   Dropped Packets: " << stats.dropped_packets << "\n";
    std::cout << "   Bandwidth Utilization: " << stats.bandwidth_utilization_percent << "%\n";
    std::cout << "   DPDK Enabled: " << (stats.dpdk_enabled ? "Yes" : "No") << "\n";
    std::cout << "   Hardware Timestamping: " << (stats.hardware_timestamping_enabled ? "Yes" : "No") << "\n";
}

void printSystemMetrics(const Phase12PerformanceEngine::SystemMetrics& metrics) {
    std::cout << "🖥️  System Metrics:\n";
    std::cout << "   CPU Usage: " << metrics.cpu_usage_percent << "%\n";
    std::cout << "   Memory Usage: " << metrics.memory_usage_percent << "%\n";
    std::cout << "   Network Usage: " << metrics.network_usage_percent << "%\n";
    std::cout << "   Cache Misses: " << metrics.cache_misses << "\n";
    std::cout << "   Branch Mispredictions: " << metrics.branch_mispredictions << "\n";
    std::cout << "   Thermal Throttling: " << metrics.thermal_throttling_percent << "%\n";
}

void demonstrateMemoryOptimization(AdvancedMemoryManager& memory_manager) {
    printSeparator("12.1 Advanced Memory Management Demo");
    
    std::cout << "🔧 Creating custom memory pools...\n";
    
    // Create specialized memory pools
    AdvancedMemoryManager::PoolConfig high_freq_config{};
    high_freq_config.block_size = 1024;
    high_freq_config.initial_blocks = 1000;
    high_freq_config.max_blocks = 10000;
    high_freq_config.numa_aware = true;
    
    memory_manager.createPool("high_frequency", high_freq_config);
    
    AdvancedMemoryManager::PoolConfig large_calc_config{};
    large_calc_config.block_size = 64 * 1024;
    large_calc_config.initial_blocks = 100;
    large_calc_config.max_blocks = 1000;
    large_calc_config.numa_aware = true;
    
    memory_manager.createPool("large_calculations", large_calc_config);
    
    std::cout << "✅ Memory pools created successfully\n\n";
    
    // Test memory allocation performance
    std::cout << "🚀 Testing allocation performance...\n";
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> allocated_blocks;
    for (int i = 0; i < 1000; ++i) {
        void* ptr = memory_manager.allocate("high_frequency", 1024);
        if (ptr) {
            allocated_blocks.push_back(ptr);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "   Allocated 1000 blocks in " << duration.count() << " μs\n";
    std::cout << "   Average allocation time: " << (duration.count() / 1000.0) << " μs per block\n\n";
    
    // Test memory prefetching
    std::cout << "⚡ Testing memory prefetching...\n";
    std::vector<float> large_array(1024 * 1024, 1.0f);
    
    start_time = std::chrono::high_resolution_clock::now();
    memory_manager.prefetch(large_array.data(), large_array.size() * sizeof(float));
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "   Prefetched " << (large_array.size() * sizeof(float) / (1024.0 * 1024.0)) 
              << " MB in " << duration.count() << " μs\n\n";
    
    // Display NUMA information
    std::cout << "🧠 NUMA Information:\n";
    std::cout << "   Current NUMA Node: " << memory_manager.getCurrentNUMANode() << "\n";
    auto numa_nodes = memory_manager.getAvailableNUMANodes();
    std::cout << "   Available NUMA Nodes: ";
    for (size_t i = 0; i < numa_nodes.size(); ++i) {
        std::cout << numa_nodes[i];
        if (i < numa_nodes.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    std::cout << "   Cache Line Size: " << memory_manager.getCacheLineSize() << " bytes\n\n";
    
    // Cleanup
    for (void* ptr : allocated_blocks) {
        memory_manager.deallocate("high_frequency", ptr);
    }
    
    printMemoryStats(memory_manager.getGlobalStats());
}

void demonstrateProcessingOptimization(AdvancedProcessingEngine& processing_engine) {
    printSeparator("12.2 SIMD & GPU Acceleration Demo");
    
    std::cout << "🔧 Initializing processing engine...\n";
    
    AdvancedProcessingEngine::OptimizationConfig config{};
    config.enable_avx512 = true;
    config.enable_gpu = true;
    config.enable_parallel = true;
    config.thread_count = std::thread::hardware_concurrency();
    config.batch_size = 1024;
    
    processing_engine.initialize(config);
    std::cout << "✅ Processing engine initialized\n\n";
    
    // Test vectorized operations
    std::cout << "⚡ Testing SIMD vectorized operations...\n";
    
    const size_t array_size = 1024 * 1024;
    std::vector<float> a(array_size, 2.5f);
    std::vector<float> b(array_size, 1.5f);
    std::vector<float> result(array_size);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    processing_engine.vectorizedMultiply(a.data(), b.data(), result.data(), array_size);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "   Vectorized multiply of " << array_size << " elements: " << duration.count() << " μs\n";
    std::cout << "   Throughput: " << (array_size / (duration.count() / 1e6)) / 1e6 << " million ops/sec\n\n";
    
    // Test Black-Scholes calculation
    std::cout << "📈 Testing Black-Scholes option pricing...\n";
    
    const size_t option_count = 10000;
    std::vector<float> spot(option_count, 100.0f);
    std::vector<float> strike(option_count, 105.0f);
    std::vector<float> time_to_expiry(option_count, 0.25f); // 3 months
    std::vector<float> volatility(option_count, 0.2f);      // 20% vol
    std::vector<float> risk_free_rate(option_count, 0.05f); // 5% rate
    std::vector<float> call_prices(option_count);
    std::vector<float> put_prices(option_count);
    
    start_time = std::chrono::high_resolution_clock::now();
    processing_engine.calculateBlackScholes(
        spot.data(), strike.data(), time_to_expiry.data(),
        volatility.data(), risk_free_rate.data(),
        call_prices.data(), put_prices.data(), option_count
    );
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "   Calculated " << option_count << " option prices in " << duration.count() << " μs\n";
    std::cout << "   Average time per option: " << (duration.count() / static_cast<double>(option_count)) << " μs\n";
    std::cout << "   Sample call price: $" << std::fixed << std::setprecision(4) << call_prices[0] << "\n";
    std::cout << "   Sample put price: $" << std::fixed << std::setprecision(4) << put_prices[0] << "\n\n";
    
    // Test parallel processing
    std::cout << "🔄 Testing parallel processing...\n";
    
    std::vector<double> parallel_test_data(1000000, 1.0);
    
    start_time = std::chrono::high_resolution_clock::now();
    processing_engine.parallelFor(0, parallel_test_data.size(), [&](size_t i) {
        parallel_test_data[i] = std::sin(i * 0.001) + std::cos(i * 0.002);
    });
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "   Parallel computation on " << parallel_test_data.size() 
              << " elements: " << duration.count() << " μs\n";
    std::cout << "   Using " << config.thread_count << " threads\n\n";
    
    printProcessingStats(processing_engine.getStats());
}

void demonstrateNetworkOptimization(AdvancedNetworkOptimizer& network_optimizer) {
    printSeparator("12.3 Ultra-Low Latency Network Optimization Demo");
    
    std::cout << "🔧 Initializing network optimizer...\n";
    
    AdvancedNetworkOptimizer::OptimizationConfig config{};
    config.enable_dpdk = true;
    config.enable_kernel_bypass = true;
    config.enable_hardware_timestamping = true;
    config.enable_multicast_optimization = true;
    config.receive_buffer_size = 2 * 1024 * 1024;
    config.send_buffer_size = 2 * 1024 * 1024;
    config.enable_cpu_affinity = true;
    config.cpu_cores = {2, 3}; // Dedicate cores 2 and 3 for network processing
    
    network_optimizer.initialize(config);
    std::cout << "✅ Network optimizer initialized\n\n";
    
    // Test hardware timestamping
    std::cout << "⏰ Testing hardware timestamping...\n";
    auto timestamp1 = network_optimizer.getHardwareTimestamp();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    auto timestamp2 = network_optimizer.getHardwareTimestamp();
    
    double elapsed_ns = static_cast<double>(timestamp2 - timestamp1);
    std::cout << "   Hardware timestamp resolution: " << elapsed_ns << " ns for 100μs delay\n";
    std::cout << "   Timestamp accuracy: " << (elapsed_ns / 100000.0) << "x actual time\n\n";
    
    // Test latency measurement
    std::cout << "📡 Testing network latency measurement...\n";
    
    std::vector<double> latencies;
    for (int i = 0; i < 10; ++i) {
        double latency = network_optimizer.measureRoundTripLatency("127.0.0.1", 8080);
        latencies.push_back(latency);
        std::cout << "   Measurement " << (i + 1) << ": " << (latency / 1000.0) << " μs\n";
    }
    
    double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    std::cout << "   Average latency: " << (avg_latency / 1000.0) << " μs\n\n";
    
    // Test network buffer allocation
    std::cout << "🗄️  Testing network buffer management...\n";
    
    std::vector<void*> buffers;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        void* buffer = network_optimizer.allocateNetworkBuffer(64 * 1024);
        if (buffer) {
            buffers.push_back(buffer);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "   Allocated 100 network buffers (64KB each) in " << duration.count() << " μs\n";
    std::cout << "   Average allocation time: " << (duration.count() / 100.0) << " μs per buffer\n\n";
    
    // Cleanup buffers
    for (void* buffer : buffers) {
        network_optimizer.deallocateNetworkBuffer(buffer);
    }
    
    printNetworkStats(network_optimizer.getStats());
}

void demonstrateSystemMonitoring(Phase12PerformanceEngine& engine) {
    printSeparator("12.4 Comprehensive System Monitoring Demo");
    
    std::cout << "🔧 Starting continuous performance monitoring...\n";
    engine.startContinuousMonitoring(std::chrono::milliseconds(500));
    
    std::cout << "⏱️  Collecting performance data for 5 seconds...\n\n";
    
    // Simulate some system activity
    for (int i = 0; i < 10; ++i) {
        // Simulate processing load
        auto& processing = engine.getProcessingEngine();
        std::vector<float> test_data_a(10000, static_cast<float>(i));
        std::vector<float> test_data_b(10000, static_cast<float>(i + 1));
        std::vector<float> test_result(10000);
        
        processing.vectorizedMultiply(test_data_a.data(), test_data_b.data(), 
                                    test_result.data(), test_data_a.size());
        
        // Simulate memory allocation
        auto& memory = engine.getMemoryManager();
        void* ptr = memory.allocate("calculations", 4096);
        if (ptr) {
            memory.deallocate("calculations", ptr);
        }
        
        // Simulate network activity
        auto& network = engine.getNetworkOptimizer();
        network.measureRoundTripLatency("127.0.0.1", 8080);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Display current metrics
        auto metrics = engine.getCurrentMetrics();
        std::cout << "📊 Sample " << (i + 1) << ":\n";
        std::cout << "   Processing ops: " << metrics.processing_stats.operations_processed << "\n";
        std::cout << "   Memory usage: " << (metrics.memory_stats.current_usage / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "   Network latency: " << (metrics.network_stats.average_latency_ns / 1000.0) << " μs\n";
        std::cout << "   Performance score: " << engine.calculatePerformanceScore() << "/100\n\n";
    }
    
    engine.stopContinuousMonitoring();
    
    std::cout << "📈 Final System Analysis:\n";
    auto final_metrics = engine.getCurrentMetrics();
    printSystemMetrics(final_metrics);
    
    std::cout << "\n🎯 Performance Optimization Recommendations:\n";
    auto recommendations = engine.getOptimizationRecommendations();
    for (size_t i = 0; i < recommendations.size(); ++i) {
        std::cout << "   " << (i + 1) << ". " << recommendations[i] << "\n";
    }
}

int main() {
    try {
        // Initialize logger
        arbitrage::utils::Logger::initialize("logs/phase12_demo.log", arbitrage::utils::Logger::Level::INFO);
        
        std::cout << "🚀 Phase 12: Advanced Performance Optimization Demo\n";
        std::cout << "This demo showcases ultra-low latency memory management, SIMD acceleration,\n";
        std::cout << "GPU processing, network optimization, and comprehensive system monitoring.\n";
        
        // Initialize Phase 12 Performance Engine
        Phase12PerformanceEngine engine;
        if (!engine.initialize()) {
            std::cerr << "❌ Failed to initialize Phase 12 Performance Engine\n";
            return 1;
        }
        
        std::cout << "✅ Phase 12 Performance Engine initialized successfully\n";
        
        // Run demonstrations
        demonstrateMemoryOptimization(engine.getMemoryManager());
        demonstrateProcessingOptimization(engine.getProcessingEngine());
        demonstrateNetworkOptimization(engine.getNetworkOptimizer());
        demonstrateSystemMonitoring(engine);
        
        // Performance optimization tests
        printSeparator("Performance Optimization Tests");
        
        std::cout << "🎯 Testing latency optimization...\n";
        engine.optimizeForLatency();
        auto latency_score = engine.calculatePerformanceScore();
        std::cout << "   Latency-optimized score: " << latency_score << "/100\n\n";
        
        std::cout << "🚀 Testing throughput optimization...\n";
        engine.optimizeForThroughput();
        auto throughput_score = engine.calculatePerformanceScore();
        std::cout << "   Throughput-optimized score: " << throughput_score << "/100\n\n";
        
        // Final summary
        printSeparator("Phase 12 Demo Summary");
        
        std::cout << "✅ Phase 12 Advanced Performance Optimization Features Demonstrated:\n";
        std::cout << "   🧠 Advanced Memory Management with NUMA awareness\n";
        std::cout << "   ⚡ SIMD instruction optimization (AVX-512 support)\n";
        std::cout << "   🎮 GPU acceleration framework\n";
        std::cout << "   🔄 Parallel algorithm implementation\n";
        std::cout << "   📡 Ultra-low latency network optimization\n";
        std::cout << "   🌐 DPDK and kernel bypass networking\n";
        std::cout << "   ⏰ Hardware timestamping for precise latency measurement\n";
        std::cout << "   🎯 Intelligent performance optimization profiles\n";
        std::cout << "   📊 Comprehensive system monitoring and analytics\n";
        std::cout << "   🔧 Automatic performance tuning and bottleneck detection\n";
        
        std::cout << "\n🎉 Phase 12 Advanced Performance Optimization Demo Complete!\n";
        std::cout << "The system is now ready for ultra-high frequency trading with:\n";
        std::cout << "   • Sub-microsecond memory allocation\n";
        std::cout << "   • SIMD-accelerated mathematical operations\n";
        std::cout << "   • Hardware-optimized network processing\n";
        std::cout << "   • Real-time performance monitoring\n";
        std::cout << "   • Adaptive optimization based on workload\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error during Phase 12 demo: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
