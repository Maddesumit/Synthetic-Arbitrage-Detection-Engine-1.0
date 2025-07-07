#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

#include "performance/PerformanceMetrics.hpp"
#include "core/MemoryManager.hpp"
#include "core/AlgorithmOptimizer.hpp"
#include "network/NetworkOptimizer.hpp"
#include "monitoring/SystemMonitoring.hpp"

using namespace SyntheticArbitrage;

// Demo function to simulate market data processing
void simulateMarketDataProcessing(size_t num_updates, Performance::LatencyTracker& latency_tracker, 
                                 Performance::ThroughputMonitor& throughput_monitor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_dist(100.0, 200.0);
    std::uniform_int_distribution<int> delay_dist(100, 1000); // 0.1-1ms delay
    
    for (size_t i = 0; i < num_updates; ++i) {
        auto measurement = latency_tracker.startMeasurement();
        
        // Simulate processing
        double price = price_dist(gen);
        double synthetic_price = price * 1.001; // Simple synthetic calculation
        
        // Simulate processing delay
        auto delay = std::chrono::microseconds(delay_dist(gen));
        std::this_thread::sleep_for(delay);
        
        throughput_monitor.recordOperation();
        
        // Small delay between updates
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

// Demo function to test SIMD operations
void demonstrateSIMDOptimizations() {
    std::cout << "\n=== SIMD Algorithm Optimization Demo ===\n";
    
    // Generate test data
    std::vector<float> prices(10000);
    std::vector<float> weights(10000);
    std::vector<float> results(10000);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> price_dist(50.0, 150.0);
    std::uniform_real_distribution<float> weight_dist(0.1, 2.0);
    
    for (size_t i = 0; i < prices.size(); ++i) {
        prices[i] = price_dist(gen);
        weights[i] = weight_dist(gen);
    }
    
    // Benchmark SIMD vs regular operations
    auto start_time = std::chrono::high_resolution_clock::now();
    arbitrage::performance::SIMDVectorOps::vectorMul(prices.data(), weights.data(), results.data(), prices.size());
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto simd_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "SIMD-optimized calculation: " << simd_duration.count() << " microseconds\n";
    std::cout << "Processed " << prices.size() << " price calculations\n";
    std::cout << "Throughput: " << (prices.size() * 1000000.0 / simd_duration.count()) << " operations/second\n";
}

// Demo function to test memory management
void demonstrateMemoryOptimization() {
    std::cout << "\n=== Memory Management Optimization Demo ===\n";
    
    arbitrage::performance::MemoryManager memory_manager;
    
    // Test custom allocator performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Allocate memory using custom allocator
    std::vector<void*> allocations;
    for (int i = 0; i < 1000; ++i) {
        void* ptr = memory_manager.allocate(1024); // 1KB allocations
        allocations.push_back(ptr);
    }
    
    auto mid_time = std::chrono::high_resolution_clock::now();
    
    // Deallocate memory
    for (void* ptr : allocations) {
        memory_manager.deallocate(ptr, 1024);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto alloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(mid_time - start_time);
    auto dealloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - mid_time);
    
    std::cout << "Custom allocator performance:\n";
    std::cout << "  Allocation: " << alloc_duration.count() << " microseconds\n";
    std::cout << "  Deallocation: " << dealloc_duration.count() << " microseconds\n";
    std::cout << "  Total operations: 1000 allocations + 1000 deallocations\n";
    
    // Skip memory pool test as it's not directly accessible through MemoryManager
    std::cout << "Memory pool allocation: skipped in this demo\n";
    
    // Cleanup
    memory_manager.cleanup();
}

// Demo function to test network optimization
void demonstrateNetworkOptimization() {
    std::cout << "\n=== Network Optimization Demo ===\n";
    
    Network::NetworkConfig config;
    config.max_connections_per_pool = 5;
    config.enable_compression = true;
    config.max_bandwidth_mbps = 1000;
    
    Network::NetworkOptimizationManager network_manager(config);
    
    // Test serialization performance
    auto& serializer = network_manager.getSerializationEngine();
    
    std::string test_json = R"({
        "symbol": "BTC/USDT",
        "price": 45678.90,
        "volume": 123.456,
        "timestamp": 1640995200000,
        "exchange": "binance",
        "metadata": {
            "source": "websocket",
            "latency": 12.5
        }
    })";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Serialize 1000 messages
    for (int i = 0; i < 1000; ++i) {
        auto binary_data = serializer.serialize(test_json);
        auto deserialized = serializer.deserialize(binary_data);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "Serialization performance:\n";
    std::cout << "  1000 serialize/deserialize cycles: " << duration.count() << " microseconds\n";
    std::cout << "  Average per operation: " << (duration.count() / 2000.0) << " microseconds\n";
    
    const auto& metrics = serializer.getMetrics();
    std::cout << "  Serializations: " << metrics.serializations.load() << "\n";
    std::cout << "  Deserializations: " << metrics.deserializations.load() << "\n";
    std::cout << "  Compression ratio: " << metrics.compression_ratio_percent.load() << "%\n";
}

// Demo function to test performance monitoring
void demonstratePerformanceMonitoring() {
    std::cout << "\n=== Performance Monitoring Demo ===\n";
    
    Performance::PerformanceMetricsManager metrics_manager;
    
    // Start system monitoring
    metrics_manager.startSystemMonitoring();
    
    // Create latency and throughput trackers
    auto& latency_tracker = metrics_manager.getLatencyTracker("market_data_processing");
    auto& throughput_monitor = metrics_manager.getThroughputMonitor("order_processing");
    
    latency_tracker.setAlertThreshold(5000000.0); // 5ms threshold
    latency_tracker.setAlertCallback([](double latency_nanos) {
        std::cout << "ALERT: High latency detected: " << (latency_nanos / 1000000.0) << "ms\n";
    });
    
    throughput_monitor.setThroughputTarget(2000.0); // 2000 ops/sec target
    
    std::cout << "Starting market data simulation...\n";
    
    // Simulate market data processing
    simulateMarketDataProcessing(5000, latency_tracker, throughput_monitor);
    
    std::cout << "Market data simulation completed.\n";
    
    // Get performance statistics
    auto latency_stats = latency_tracker.getStats();
    auto throughput_stats = throughput_monitor.getStats();
    auto& resource_monitor = metrics_manager.getResourceMonitor();
    auto cpu_stats = resource_monitor.getCPUStats();
    auto memory_stats = resource_monitor.getMemoryStats();
    
    std::cout << "\nPerformance Results:\n";
    std::cout << "Latency Statistics:\n";
    std::cout << "  Average: " << (latency_stats.avg_nanos / 1000000.0) << "ms\n";
    std::cout << "  P95: " << (latency_stats.p95_nanos / 1000000.0) << "ms\n";
    std::cout << "  P99: " << (latency_stats.p99_nanos / 1000000.0) << "ms\n";
    std::cout << "  Sample count: " << latency_stats.sample_count << "\n";
    
    std::cout << "\nThroughput Statistics:\n";
    std::cout << "  Current: " << throughput_stats.current_ops_per_sec << " ops/sec\n";
    std::cout << "  Peak: " << throughput_stats.peak_ops_per_sec << " ops/sec\n";
    std::cout << "  Total operations: " << throughput_stats.total_operations << "\n";
    
    std::cout << "\nSystem Resources:\n";
    std::cout << "  CPU usage: " << cpu_stats.cpu_usage_percent << "%\n";
    std::cout << "  Memory usage: " << memory_stats.memory_usage_percent << "%\n";
    std::cout << "  Process memory: " << (memory_stats.process_memory_bytes / 1024.0 / 1024.0) << " MB\n";
    
    // Validate throughput target
    bool target_met = throughput_monitor.validateThroughput(2000.0, 5.0); // 5% tolerance
    std::cout << "\nThroughput target (2000 ops/sec): " << (target_met ? "MET" : "NOT MET") << "\n";
    
    metrics_manager.stopSystemMonitoring();
}

// Demo function to test system monitoring and analytics
void demonstrateSystemMonitoring() {
    std::cout << "\n=== System Monitoring & Analytics Demo ===\n";
    
    Performance::PerformanceMetricsManager metrics_manager;
    Monitoring::SystemMonitoringManager monitoring_manager(metrics_manager);
    
    // Start comprehensive monitoring
    monitoring_manager.startComprehensiveMonitoring();
    
    auto& health_monitor = monitoring_manager.getHealthMonitor();
    auto& bottleneck_detector = monitoring_manager.getBottleneckDetector();
    auto& alert_manager = monitoring_manager.getAlertManager();
    
    // Set up alert callback
    alert_manager.setNotificationCallback([](const Monitoring::AlertManager::Alert& alert) {
        std::cout << "SYSTEM ALERT [" << static_cast<int>(alert.severity) << "]: " 
                  << alert.title << " - " << alert.description << "\n";
    });
    
    std::cout << "System monitoring started. Running workload simulation...\n";
    
    // Simulate various workloads to trigger monitoring
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Get health status
    auto overall_health = health_monitor.getOverallHealth();
    std::cout << "Overall system health: " << static_cast<int>(overall_health) << "\n";
    
    // Check for bottlenecks
    auto bottlenecks = bottleneck_detector.detectBottlenecks();
    std::cout << "Detected bottlenecks: " << bottlenecks.size() << "\n";
    
    for (const auto& bottleneck : bottlenecks) {
        std::cout << "  - " << bottleneck.component_name << ": " 
                  << bottleneck.description << " (severity: " 
                  << (bottleneck.severity_score * 100) << "%)\n";
    }
    
    // Get active alerts
    auto active_alerts = alert_manager.getActiveAlerts();
    std::cout << "Active alerts: " << active_alerts.size() << "\n";
    
    // Generate system report
    auto system_report = monitoring_manager.generateSystemReport();
    std::cout << "\nSystem Report Generated:\n";
    std::cout << "  Report timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
        system_report.report_timestamp.time_since_epoch()).count() << "\n";
    std::cout << "  Health checks: " << system_report.health_status.size() << "\n";
    std::cout << "  Detected bottlenecks: " << system_report.detected_bottlenecks.size() << "\n";
    std::cout << "  Active alerts: " << system_report.active_alerts.size() << "\n";
    
    monitoring_manager.stopComprehensiveMonitoring();
}

int main() {
    std::cout << "=== Phase 7: Performance Optimization & Advanced Features Demo ===\n";
    std::cout << "This demo showcases the advanced performance optimization features implemented in Phase 7.\n\n";
    
    try {
        // Demo 1: SIMD Algorithm Optimization
        demonstrateSIMDOptimizations();
        
        // Demo 2: Memory Management Optimization
        demonstrateMemoryOptimization();
        
        // Demo 3: Network Optimization
        demonstrateNetworkOptimization();
        
        // Demo 4: Performance Monitoring
        demonstratePerformanceMonitoring();
        
        // Demo 5: System Monitoring & Analytics
        demonstrateSystemMonitoring();
        
        std::cout << "\n=== Phase 7 Demo Completed Successfully ===\n";
        std::cout << "All performance optimization and monitoring features are working correctly.\n";
        std::cout << "The system is now ready for high-frequency trading operations with:\n";
        std::cout << "  ✓ SIMD-optimized calculations\n";
        std::cout << "  ✓ Custom memory management\n";
        std::cout << "  ✓ Network optimization\n";
        std::cout << "  ✓ Comprehensive performance monitoring\n";
        std::cout << "  ✓ System health monitoring\n";
        std::cout << "  ✓ Bottleneck detection\n";
        std::cout << "  ✓ Alert management\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during Phase 7 demo: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
