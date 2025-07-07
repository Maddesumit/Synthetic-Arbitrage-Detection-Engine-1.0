#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <fstream>
#include <queue>

namespace SyntheticArbitrage {
namespace Performance {

// High-precision timing utilities
class HighPrecisionTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;
    
    HighPrecisionTimer();
    ~HighPrecisionTimer();
    
    // Timer operations
    void start();
    void stop();
    void reset();
    void lap();
    
    // Get timing results
    Duration getElapsed() const;
    Duration getElapsedNanos() const;
    double getElapsedMicros() const;
    double getElapsedMillis() const;
    double getElapsedSeconds() const;
    
    // Lap timing
    std::vector<Duration> getLapTimes() const;
    Duration getLastLapTime() const;
    
    // Statistics
    Duration getMinLapTime() const;
    Duration getMaxLapTime() const;
    Duration getAverageLapTime() const;
    
    // Static timing utilities
    template<typename Func>
    static Duration timeFunction(Func&& func);
    
    template<typename Func>
    static double timeFunctionMicros(Func&& func);

private:
    TimePoint start_time_;
    TimePoint stop_time_;
    TimePoint last_lap_time_;
    std::vector<Duration> lap_times_;
    bool running_;
    mutable std::mutex timing_mutex_;
};

// Latency measurement and tracking
class LatencyTracker {
public:
    struct LatencyStats {
        double min_nanos = std::numeric_limits<double>::max();
        double max_nanos = 0.0;
        double avg_nanos = 0.0;
        double p50_nanos = 0.0;
        double p95_nanos = 0.0;
        double p99_nanos = 0.0;
        double p999_nanos = 0.0;
        uint64_t sample_count = 0;
        std::chrono::steady_clock::time_point last_update;
    };
    
    LatencyTracker(const std::string& name, size_t max_samples = 10000);
    ~LatencyTracker();
    
    // Record latency measurements
    void recordLatency(std::chrono::nanoseconds latency);
    void recordLatency(double latency_nanos);
    
    // Timer-based measurement
    class LatencyMeasurement {
    public:
        LatencyMeasurement(LatencyTracker& tracker);
        ~LatencyMeasurement();
        
    private:
        LatencyTracker& tracker_;
        HighPrecisionTimer::TimePoint start_time_;
    };
    
    LatencyMeasurement startMeasurement();
    
    // Statistics
    LatencyStats getStats() const;
    void resetStats();
    
    // Percentile calculations
    double getPercentile(double percentile) const;
    std::vector<double> getPercentiles(const std::vector<double>& percentiles) const;
    
    // Real-time monitoring
    void enableRealTimeMonitoring(bool enable);
    void setAlertThreshold(double threshold_nanos);
    void setAlertCallback(std::function<void(double)> callback);

private:
    std::string name_;
    size_t max_samples_;
    std::vector<double> samples_;
    mutable std::mutex samples_mutex_;
    
    // Statistics cache
    mutable LatencyStats cached_stats_;
    mutable bool stats_dirty_;
    mutable std::mutex stats_mutex_;
    
    // Alerting
    std::atomic<bool> real_time_monitoring_;
    std::atomic<double> alert_threshold_;
    std::function<void(double)> alert_callback_;
    
    void updateStatsCache() const;
    void checkAlert(double latency_nanos);
};

// Throughput measurement and monitoring
class ThroughputMonitor {
public:
    struct ThroughputStats {
        double current_ops_per_sec = 0.0;
        double peak_ops_per_sec = 0.0;
        double avg_ops_per_sec = 0.0;
        uint64_t total_operations = 0;
        std::chrono::steady_clock::time_point measurement_start;
        std::chrono::seconds measurement_duration{0};
    };
    
    ThroughputMonitor(const std::string& name, std::chrono::seconds window_size = std::chrono::seconds(60));
    ~ThroughputMonitor();
    
    // Record operations
    void recordOperation();
    void recordOperations(uint64_t count);
    void recordBatchOperation(uint64_t batch_size);
    
    // Statistics
    ThroughputStats getStats() const;
    void resetStats();
    
    // Real-time monitoring
    double getCurrentThroughput() const; // ops/sec
    double getAverageThroughput() const; // ops/sec over entire period
    double getPeakThroughput() const;
    
    // Validation
    bool validateThroughput(double expected_ops_per_sec, double tolerance_percent = 5.0) const;
    
    // Alerting
    void setThroughputTarget(double target_ops_per_sec);
    void setAlertCallback(std::function<void(double, double)> callback); // current, target

private:
    std::string name_;
    std::chrono::seconds window_size_;
    
    // Operation tracking
    struct OperationRecord {
        std::chrono::steady_clock::time_point timestamp;
        uint64_t operation_count;
    };
    
    std::queue<OperationRecord> operation_history_;
    mutable std::mutex history_mutex_;
    
    // Statistics
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<double> peak_throughput_{0.0};
    std::chrono::steady_clock::time_point start_time_;
    
    // Alerting
    std::atomic<double> throughput_target_{0.0};
    std::function<void(double, double)> alert_callback_;
    
    // Background monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitoring_active_;
    
    void cleanupOldRecords();
    void monitoringLoop();
    double calculateCurrentThroughput() const;
};

// Resource usage monitoring (CPU, Memory, Network)
class ResourceMonitor {
public:
    struct CPUStats {
        double cpu_usage_percent = 0.0;
        double user_cpu_percent = 0.0;
        double system_cpu_percent = 0.0;
        double idle_cpu_percent = 0.0;
        uint32_t core_count = 0;
        std::vector<double> per_core_usage;
    };
    
    struct MemoryStats {
        uint64_t total_memory_bytes = 0;
        uint64_t used_memory_bytes = 0;
        uint64_t free_memory_bytes = 0;
        uint64_t cached_memory_bytes = 0;
        uint64_t process_memory_bytes = 0;
        uint64_t process_peak_memory_bytes = 0;
        double memory_usage_percent = 0.0;
    };
    
    struct NetworkStats {
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        double current_send_rate_mbps = 0.0;
        double current_recv_rate_mbps = 0.0;
        double peak_send_rate_mbps = 0.0;
        double peak_recv_rate_mbps = 0.0;
    };
    
    ResourceMonitor();
    ~ResourceMonitor();
    
    // Resource monitoring
    CPUStats getCPUStats() const;
    MemoryStats getMemoryStats() const;
    NetworkStats getNetworkStats() const;
    
    // Continuous monitoring
    void startMonitoring(std::chrono::seconds interval = std::chrono::seconds(1));
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Historical data
    std::vector<CPUStats> getCPUHistory(std::chrono::minutes duration) const;
    std::vector<MemoryStats> getMemoryHistory(std::chrono::minutes duration) const;
    std::vector<NetworkStats> getNetworkHistory(std::chrono::minutes duration) const;
    
    // Alerts and thresholds
    void setCPUThreshold(double cpu_percent);
    void setMemoryThreshold(double memory_percent);
    void setNetworkThreshold(double bandwidth_mbps);
    
    void setResourceAlertCallback(std::function<void(const std::string&, double)> callback);
    
    // Memory leak detection
    bool detectMemoryLeak(std::chrono::minutes observation_period = std::chrono::minutes(10)) const;
    double getMemoryGrowthRate() const; // bytes per second

private:
    // Monitoring state
    std::atomic<bool> monitoring_active_{false};
    std::unique_ptr<std::thread> monitor_thread_;
    std::chrono::seconds monitoring_interval_;
    
    // Historical data storage
    struct TimestampedCPUStats {
        std::chrono::steady_clock::time_point timestamp;
        CPUStats stats;
    };
    
    struct TimestampedMemoryStats {
        std::chrono::steady_clock::time_point timestamp;
        MemoryStats stats;
    };
    
    struct TimestampedNetworkStats {
        std::chrono::steady_clock::time_point timestamp;
        NetworkStats stats;
    };
    
    std::vector<TimestampedCPUStats> cpu_history_;
    std::vector<TimestampedMemoryStats> memory_history_;
    std::vector<TimestampedNetworkStats> network_history_;
    mutable std::mutex history_mutex_;
    
    // Alert thresholds
    std::atomic<double> cpu_threshold_{80.0};
    std::atomic<double> memory_threshold_{85.0};
    std::atomic<double> network_threshold_{900.0}; // 900 Mbps
    
    std::function<void(const std::string&, double)> alert_callback_;
    
    // Platform-specific implementations
    CPUStats getCurrentCPUStats() const;
    MemoryStats getCurrentMemoryStats() const;
    NetworkStats getCurrentNetworkStats() const;
    
    void monitoringLoop();
    void checkThresholds(const CPUStats& cpu, const MemoryStats& memory, const NetworkStats& network);
    void cleanupOldHistory();
};

// Performance profiling integration
class ProfilerIntegration {
public:
    enum class ProfilerType {
        GPERFTOOLS,
        VALGRIND,
        PERF,
        CUSTOM
    };
    
    ProfilerIntegration();
    ~ProfilerIntegration();
    
    // Profiler control
    bool startProfiling(ProfilerType type, const std::string& output_file = "");
    void stopProfiling();
    bool isProfilingActive() const;
    
    // CPU profiling
    void enableCPUProfiling(const std::string& output_file);
    void disableCPUProfiling();
    
    // Memory profiling
    void enableMemoryProfiling(const std::string& output_file);
    void disableMemoryProfiling();
    
    // Heap profiling
    void enableHeapProfiling(const std::string& output_file);
    void disableHeapProfiling();
    void dumpHeapProfile();
    
    // Custom profiling markers
    void beginProfilerMarker(const std::string& name);
    void endProfilerMarker(const std::string& name);
    
    class ProfilerScope {
    public:
        ProfilerScope(ProfilerIntegration& profiler, const std::string& name);
        ~ProfilerScope();
        
    private:
        ProfilerIntegration& profiler_;
        std::string name_;
    };
    
    ProfilerScope createProfilerScope(const std::string& name);
    
    // Report generation
    bool generateReport(const std::string& profile_file, const std::string& output_file);
    std::string getLastReportPath() const;

private:
    ProfilerType active_profiler_;
    std::atomic<bool> profiling_active_{false};
    std::string current_output_file_;
    std::string last_report_path_;
    
    mutable std::mutex profiler_mutex_;
    
    // Platform-specific profiler implementations
    bool startGPerfToolsProfiling(const std::string& output_file);
    bool startValgrindProfiling(const std::string& output_file);
    bool startPerfProfiling(const std::string& output_file);
    
    void stopGPerfToolsProfiling();
    void stopValgrindProfiling();
    void stopPerfProfiling();
};

// Comprehensive performance metrics manager
class PerformanceMetricsManager {
public:
    PerformanceMetricsManager();
    ~PerformanceMetricsManager();
    
    // Component access
    LatencyTracker& getLatencyTracker(const std::string& name);
    ThroughputMonitor& getThroughputMonitor(const std::string& name);
    ResourceMonitor& getResourceMonitor();
    ProfilerIntegration& getProfilerIntegration();
    
    // High-level operations
    void startSystemMonitoring();
    void stopSystemMonitoring();
    
    // Market data processing validation
    bool validateMarketDataProcessing(double expected_updates_per_sec = 2000.0);
    
    // Performance regression testing
    struct PerformanceBaseline {
        std::string test_name;
        double baseline_latency_nanos;
        double baseline_throughput_ops_per_sec;
        double baseline_cpu_usage_percent;
        double baseline_memory_usage_mb;
        std::chrono::steady_clock::time_point recorded_time;
    };
    
    void recordPerformanceBaseline(const std::string& test_name);
    bool checkPerformanceRegression(const std::string& test_name, double tolerance_percent = 10.0);
    std::vector<PerformanceBaseline> getPerformanceBaselines() const;
    
    // Comprehensive performance report
    struct PerformanceReport {
        std::unordered_map<std::string, LatencyTracker::LatencyStats> latency_stats;
        std::unordered_map<std::string, ThroughputMonitor::ThroughputStats> throughput_stats;
        ResourceMonitor::CPUStats cpu_stats;
        ResourceMonitor::MemoryStats memory_stats;
        ResourceMonitor::NetworkStats network_stats;
        std::chrono::steady_clock::time_point report_time;
        std::string profiler_report_path;
    };
    
    PerformanceReport generatePerformanceReport();
    void exportPerformanceReport(const PerformanceReport& report, const std::string& file_path);
    
    // Configuration
    void setGlobalLatencyThreshold(double threshold_nanos);
    void setGlobalThroughputTarget(double target_ops_per_sec);
    void enableAutomaticReporting(std::chrono::minutes interval);
    void disableAutomaticReporting();

private:
    // Component storage
    std::unordered_map<std::string, std::unique_ptr<LatencyTracker>> latency_trackers_;
    std::unordered_map<std::string, std::unique_ptr<ThroughputMonitor>> throughput_monitors_;
    std::unique_ptr<ResourceMonitor> resource_monitor_;
    std::unique_ptr<ProfilerIntegration> profiler_integration_;
    
    // Performance baselines
    std::vector<PerformanceBaseline> performance_baselines_;
    mutable std::mutex baselines_mutex_;
    
    // Component management
    mutable std::mutex components_mutex_;
    
    // Automatic reporting
    std::atomic<bool> auto_reporting_enabled_{false};
    std::unique_ptr<std::thread> reporting_thread_;
    std::chrono::minutes reporting_interval_;
    
    void automaticReportingLoop();
};

// Template implementations
template<typename Func>
HighPrecisionTimer::Duration HighPrecisionTimer::timeFunction(Func&& func) {
    auto start = Clock::now();
    func();
    auto end = Clock::now();
    return std::chrono::duration_cast<Duration>(end - start);
}

template<typename Func>
double HighPrecisionTimer::timeFunctionMicros(Func&& func) {
    auto duration = timeFunction(std::forward<Func>(func));
    return static_cast<double>(duration.count()) / 1000.0; // Convert nanos to micros
}

} // namespace Performance
} // namespace SyntheticArbitrage
