#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include "../core/MathUtils.hpp"
#include "../core/MemoryManager.hpp"

using namespace arbitrage;

int main() {
    std::cout << "Performance Benchmark Tool\n";
    std::cout << "==========================\n\n";
    
    // Math operations benchmark
    std::cout << "Math Operations Benchmark:\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.1, 100.0);
    
    const int iterations = 1000000;
    double sum = 0.0;
    
    for (int i = 0; i < iterations; ++i) {
        double a = dis(gen);
        double b = dis(gen);
        sum += (b - a) / a; // Simple return calculation
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Processed " << iterations << " return calculations\n";
    std::cout << "  Total time: " << duration.count() << " microseconds\n";
    std::cout << "  Average time per operation: " << (double)duration.count() / iterations << " microseconds\n";
    std::cout << "  Sum (to prevent optimization): " << sum << "\n\n";
    
    // Memory management benchmark
    std::cout << "Memory Management Benchmark:\n";
    performance::MemoryManager memory_manager;
    
    start = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> allocations;
    for (int i = 0; i < 10000; ++i) {
        void* ptr = memory_manager.allocate(1024);
        allocations.push_back(ptr);
    }
    
    for (void* ptr : allocations) {
        memory_manager.deallocate(ptr, 1024);
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Performed 10,000 allocations and deallocations\n";
    std::cout << "  Total time: " << duration.count() << " microseconds\n";
    std::cout << "  Average time per alloc/dealloc pair: " << (double)duration.count() / 10000 << " microseconds\n\n";
    
    memory_manager.cleanup();
    
    std::cout << "Benchmark completed successfully!\n";
    return 0;
}