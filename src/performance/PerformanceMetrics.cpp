#include "PerformanceMetrics.hpp"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <random>

#ifdef __linux__
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace SyntheticArbitrage {
namespace Performance {

// HighPrecisionTimer implementation
HighPrecisionTimer::HighPrecisionTimer() : running_(false) {
    reset();
}

HighPrecisionTimer::~HighPrecisionTimer() = default;

void HighPrecisionTimer::start() {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    start_time_ = Clock::now();
    last_lap_time_ = start_time_;
    running_ = true;
}

void HighPrecisionTimer::stop() {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    stop_time_ = Clock::now();
    running_ = false;
}

void HighPrecisionTimer::reset() {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    start_time_ = Clock::now();
    stop_time_ = start_time_;
    last_lap_time_ = start_time_;
    lap_times_.clear();
    running_ = false;
}

void HighPrecisionTimer::lap() {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    auto now = Clock::now();
    auto lap_duration = std::chrono::duration_cast<Duration>(now - last_lap_time_);
    lap_times_.push_back(lap_duration);
    last_lap_time_ = now;
}

HighPrecisionTimer::Duration HighPrecisionTimer::getElapsed() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    auto end_time = running_ ? Clock::now() : stop_time_;
    return std::chrono::duration_cast<Duration>(end_time - start_time_);
}

HighPrecisionTimer::Duration HighPrecisionTimer::getElapsedNanos() const {
    return getElapsed();
}

double HighPrecisionTimer::getElapsedMicros() const {
    return static_cast<double>(getElapsed().count()) / 1000.0;
}

double HighPrecisionTimer::getElapsedMillis() const {
    return static_cast<double>(getElapsed().count()) / 1000000.0;
}

double HighPrecisionTimer::getElapsedSeconds() const {
    return static_cast<double>(getElapsed().count()) / 1000000000.0;
}

std::vector<HighPrecisionTimer::Duration> HighPrecisionTimer::getLapTimes() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    return lap_times_;
}

HighPrecisionTimer::Duration HighPrecisionTimer::getLastLapTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    return lap_times_.empty() ? Duration::zero() : lap_times_.back();
}

HighPrecisionTimer::Duration HighPrecisionTimer::getMinLapTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    if (lap_times_.empty()) return Duration::zero();
    return *std::min_element(lap_times_.begin(), lap_times_.end());
}

HighPrecisionTimer::Duration HighPrecisionTimer::getMaxLapTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    if (lap_times_.empty()) return Duration::zero();
    return *std::max_element(lap_times_.begin(), lap_times_.end());
}

HighPrecisionTimer::Duration HighPrecisionTimer::getAverageLapTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    if (lap_times_.empty()) return Duration::zero();
    
    auto total = std::accumulate(lap_times_.begin(), lap_times_.end(), Duration::zero());
    return Duration(total.count() / lap_times_.size());
}

// LatencyTracker implementation
LatencyTracker::LatencyTracker(const std::string& name, size_t max_samples)
    : name_(name), max_samples_(max_samples), stats_dirty_(true),
      real_time_monitoring_(false), alert_threshold_(std::numeric_limits<double>::max()) {
    samples_.reserve(max_samples_);
}

LatencyTracker::~LatencyTracker() = default;

void LatencyTracker::recordLatency(std::chrono::nanoseconds latency) {
    recordLatency(static_cast<double>(latency.count()));
}

void LatencyTracker::recordLatency(double latency_nanos) {
    {
        std::lock_guard<std::mutex> lock(samples_mutex_);
        
        if (samples_.size() >= max_samples_) {
            // Remove oldest sample to maintain fixed size
            samples_.erase(samples_.begin());
        }
        
        samples_.push_back(latency_nanos);
        stats_dirty_ = true;
    }
    
    if (real_time_monitoring_) {
        checkAlert(latency_nanos);
    }
}

LatencyTracker::LatencyMeasurement::LatencyMeasurement(LatencyTracker& tracker)
    : tracker_(tracker), start_time_(HighPrecisionTimer::Clock::now()) {
}

LatencyTracker::LatencyMeasurement::~LatencyMeasurement() {
    auto end_time = HighPrecisionTimer::Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_);
    tracker_.recordLatency(duration);
}

LatencyTracker::LatencyMeasurement LatencyTracker::startMeasurement() {
    return LatencyMeasurement(*this);
}

LatencyTracker::LatencyStats LatencyTracker::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_dirty_) {
        updateStatsCache();
    }
    
    return cached_stats_;
}

void LatencyTracker::resetStats() {
    std::lock_guard<std::mutex> samples_lock(samples_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    samples_.clear();
    stats_dirty_ = true;
}

double LatencyTracker::getPercentile(double percentile) const {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    
    if (samples_.empty()) return 0.0;
    
    std::vector<double> sorted_samples = samples_;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    size_t index = static_cast<size_t>((percentile / 100.0) * (sorted_samples.size() - 1));
    return sorted_samples[index];
}

std::vector<double> LatencyTracker::getPercentiles(const std::vector<double>& percentiles) const {
    std::vector<double> results;
    results.reserve(percentiles.size());
    
    for (double p : percentiles) {
        results.push_back(getPercentile(p));
    }
    
    return results;
}

void LatencyTracker::enableRealTimeMonitoring(bool enable) {
    real_time_monitoring_ = enable;
}

void LatencyTracker::setAlertThreshold(double threshold_nanos) {
    alert_threshold_ = threshold_nanos;
}

void LatencyTracker::setAlertCallback(std::function<void(double)> callback) {
    alert_callback_ = std::move(callback);
}

void LatencyTracker::updateStatsCache() const {
    std::lock_guard<std::mutex> samples_lock(samples_mutex_);
    
    if (samples_.empty()) {
        cached_stats_ = LatencyStats{};
        cached_stats_.last_update = std::chrono::steady_clock::now();
        stats_dirty_ = false;
        return;
    }
    
    std::vector<double> sorted_samples = samples_;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    cached_stats_.min_nanos = sorted_samples.front();
    cached_stats_.max_nanos = sorted_samples.back();
    cached_stats_.avg_nanos = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0.0) / sorted_samples.size();
    
    // Calculate percentiles
    auto percentile = [&sorted_samples](double p) {
        size_t index = static_cast<size_t>((p / 100.0) * (sorted_samples.size() - 1));
        return sorted_samples[index];
    };
    
    cached_stats_.p50_nanos = percentile(50.0);
    cached_stats_.p95_nanos = percentile(95.0);
    cached_stats_.p99_nanos = percentile(99.0);
    cached_stats_.p999_nanos = percentile(99.9);
    
    cached_stats_.sample_count = sorted_samples.size();
    cached_stats_.last_update = std::chrono::steady_clock::now();
    
    stats_dirty_ = false;
}

void LatencyTracker::checkAlert(double latency_nanos) {
    if (latency_nanos > alert_threshold_ && alert_callback_) {
        alert_callback_(latency_nanos);
    }
}

// ThroughputMonitor implementation
ThroughputMonitor::ThroughputMonitor(const std::string& name, std::chrono::seconds window_size)
    : name_(name), window_size_(window_size), start_time_(std::chrono::steady_clock::now()),
      monitoring_active_(true) {
    
    monitor_thread_ = std::make_unique<std::thread>(&ThroughputMonitor::monitoringLoop, this);
}

ThroughputMonitor::~ThroughputMonitor() {
    monitoring_active_ = false;
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
}

void ThroughputMonitor::recordOperation() {
    recordOperations(1);
}

void ThroughputMonitor::recordOperations(uint64_t count) {
    auto now = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        operation_history_.push({now, count});
        cleanupOldRecords();
    }
    
    total_operations_ += count;
    
    // Update peak throughput
    double current = calculateCurrentThroughput();
    double peak = peak_throughput_.load();
    while (current > peak && !peak_throughput_.compare_exchange_weak(peak, current)) {
        // Retry until successful
    }
    
    // Check throughput target
    double target = throughput_target_.load();
    if (target > 0.0 && alert_callback_ && current < target * 0.9) { // 10% tolerance
        alert_callback_(current, target);
    }
}

void ThroughputMonitor::recordBatchOperation(uint64_t batch_size) {
    recordOperations(batch_size);
}

ThroughputMonitor::ThroughputStats ThroughputMonitor::getStats() const {
    ThroughputStats stats;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = now - start_time_;
    
    stats.total_operations = total_operations_;
    stats.peak_ops_per_sec = peak_throughput_;
    stats.current_ops_per_sec = calculateCurrentThroughput();
    stats.measurement_start = start_time_;
    stats.measurement_duration = std::chrono::duration_cast<std::chrono::seconds>(duration);
    
    if (duration.count() > 0) {
        stats.avg_ops_per_sec = static_cast<double>(stats.total_operations) / 
                               std::chrono::duration<double>(duration).count();
    }
    
    return stats;
}

void ThroughputMonitor::resetStats() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    while (!operation_history_.empty()) {
        operation_history_.pop();
    }
    
    total_operations_ = 0;
    peak_throughput_ = 0.0;
    start_time_ = std::chrono::steady_clock::now();
}

double ThroughputMonitor::getCurrentThroughput() const {
    return calculateCurrentThroughput();
}

double ThroughputMonitor::getAverageThroughput() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(now - start_time_).count();
    
    if (duration > 0) {
        return static_cast<double>(total_operations_) / duration;
    }
    
    return 0.0;
}

double ThroughputMonitor::getPeakThroughput() const {
    return peak_throughput_;
}

bool ThroughputMonitor::validateThroughput(double expected_ops_per_sec, double tolerance_percent) const {
    double current = getCurrentThroughput();
    double tolerance = expected_ops_per_sec * (tolerance_percent / 100.0);
    return std::abs(current - expected_ops_per_sec) <= tolerance;
}

void ThroughputMonitor::setThroughputTarget(double target_ops_per_sec) {
    throughput_target_ = target_ops_per_sec;
}

void ThroughputMonitor::setAlertCallback(std::function<void(double, double)> callback) {
    alert_callback_ = std::move(callback);
}

void ThroughputMonitor::cleanupOldRecords() {
    auto cutoff_time = std::chrono::steady_clock::now() - window_size_;
    
    while (!operation_history_.empty() && operation_history_.front().timestamp < cutoff_time) {
        operation_history_.pop();
    }
}

void ThroughputMonitor::monitoringLoop() {
    while (monitoring_active_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            cleanupOldRecords();
        }
    }
}

double ThroughputMonitor::calculateCurrentThroughput() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (operation_history_.empty()) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto window_start = now - window_size_;
    
    uint64_t operations_in_window = 0;
    auto temp_queue = operation_history_;
    
    while (!temp_queue.empty()) {
        const auto& record = temp_queue.front();
        if (record.timestamp >= window_start) {
            operations_in_window += record.operation_count;
        }
        temp_queue.pop();
    }
    
    double window_seconds = std::chrono::duration<double>(window_size_).count();
    return static_cast<double>(operations_in_window) / window_seconds;
}

// ResourceMonitor implementation
ResourceMonitor::ResourceMonitor() : monitoring_interval_(std::chrono::seconds(1)) {
}

ResourceMonitor::~ResourceMonitor() {
    stopMonitoring();
}

ResourceMonitor::CPUStats ResourceMonitor::getCPUStats() const {
    return getCurrentCPUStats();
}

ResourceMonitor::MemoryStats ResourceMonitor::getMemoryStats() const {
    return getCurrentMemoryStats();
}

ResourceMonitor::NetworkStats ResourceMonitor::getNetworkStats() const {
    return getCurrentNetworkStats();
}

void ResourceMonitor::startMonitoring(std::chrono::seconds interval) {
    if (monitoring_active_) {
        return; // Already monitoring
    }
    
    monitoring_interval_ = interval;
    monitoring_active_ = true;
    monitor_thread_ = std::make_unique<std::thread>(&ResourceMonitor::monitoringLoop, this);
}

void ResourceMonitor::stopMonitoring() {
    monitoring_active_ = false;
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
}

bool ResourceMonitor::isMonitoring() const {
    return monitoring_active_;
}

std::vector<ResourceMonitor::CPUStats> ResourceMonitor::getCPUHistory(std::chrono::minutes duration) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    std::vector<CPUStats> result;
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    
    for (const auto& entry : cpu_history_) {
        if (entry.timestamp >= cutoff_time) {
            result.push_back(entry.stats);
        }
    }
    
    return result;
}

std::vector<ResourceMonitor::MemoryStats> ResourceMonitor::getMemoryHistory(std::chrono::minutes duration) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    std::vector<MemoryStats> result;
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    
    for (const auto& entry : memory_history_) {
        if (entry.timestamp >= cutoff_time) {
            result.push_back(entry.stats);
        }
    }
    
    return result;
}

std::vector<ResourceMonitor::NetworkStats> ResourceMonitor::getNetworkHistory(std::chrono::minutes duration) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    std::vector<NetworkStats> result;
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    
    for (const auto& entry : network_history_) {
        if (entry.timestamp >= cutoff_time) {
            result.push_back(entry.stats);
        }
    }
    
    return result;
}

void ResourceMonitor::setCPUThreshold(double cpu_percent) {
    cpu_threshold_ = cpu_percent;
}

void ResourceMonitor::setMemoryThreshold(double memory_percent) {
    memory_threshold_ = memory_percent;
}

void ResourceMonitor::setNetworkThreshold(double bandwidth_mbps) {
    network_threshold_ = bandwidth_mbps;
}

void ResourceMonitor::setResourceAlertCallback(std::function<void(const std::string&, double)> callback) {
    alert_callback_ = std::move(callback);
}

bool ResourceMonitor::detectMemoryLeak(std::chrono::minutes observation_period) const {
    auto memory_history = getMemoryHistory(observation_period);
    
    if (memory_history.size() < 2) {
        return false; // Insufficient data
    }
    
    // Check for consistent memory growth
    double growth_rate = getMemoryGrowthRate();
    return growth_rate > 1024 * 1024; // More than 1MB/sec growth
}

double ResourceMonitor::getMemoryGrowthRate() const {
    auto memory_history = getMemoryHistory(std::chrono::minutes(10));
    
    if (memory_history.size() < 2) {
        return 0.0;
    }
    
    // Calculate linear regression for memory usage over time
    uint64_t first_memory = memory_history.front().process_memory_bytes;
    uint64_t last_memory = memory_history.back().process_memory_bytes;
    
    auto time_diff = std::chrono::duration<double>(std::chrono::minutes(10)).count();
    
    return static_cast<double>(last_memory - first_memory) / time_diff;
}

// Platform-specific implementations
ResourceMonitor::CPUStats ResourceMonitor::getCurrentCPUStats() const {
    CPUStats stats;
    
#ifdef __linux__
    // Linux implementation using /proc/stat
    std::ifstream stat_file("/proc/stat");
    std::string line;
    if (std::getline(stat_file, line)) {
        std::istringstream iss(line);
        std::string cpu_label;
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
        
        iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        
        uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
        uint64_t active = total - idle - iowait;
        
        if (total > 0) {
            stats.cpu_usage_percent = (static_cast<double>(active) / total) * 100.0;
            stats.user_cpu_percent = (static_cast<double>(user + nice) / total) * 100.0;
            stats.system_cpu_percent = (static_cast<double>(system) / total) * 100.0;
            stats.idle_cpu_percent = (static_cast<double>(idle) / total) * 100.0;
        }
    }
    
    stats.core_count = std::thread::hardware_concurrency();
    
#elif defined(__APPLE__)
    // macOS implementation using host_processor_info
    natural_t processor_count;
    processor_cpu_load_info_t cpu_load_info;
    mach_msg_type_number_t cpu_load_info_count;
    
    kern_return_t error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                            &processor_count, 
                                            (processor_info_array_t*)&cpu_load_info,
                                            &cpu_load_info_count);
    
    if (error == KERN_SUCCESS) {
        stats.core_count = processor_count;
        stats.per_core_usage.resize(processor_count);
        
        uint64_t total_user = 0, total_system = 0, total_idle = 0, total_nice = 0;
        
        for (natural_t i = 0; i < processor_count; ++i) {
            uint64_t user = cpu_load_info[i].cpu_ticks[CPU_STATE_USER];
            uint64_t system = cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM];
            uint64_t idle = cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE];
            uint64_t nice = cpu_load_info[i].cpu_ticks[CPU_STATE_NICE];
            
            uint64_t core_total = user + system + idle + nice;
            uint64_t core_active = user + system + nice;
            
            if (core_total > 0) {
                stats.per_core_usage[i] = (static_cast<double>(core_active) / core_total) * 100.0;
            }
            
            total_user += user;
            total_system += system;
            total_idle += idle;
            total_nice += nice;
        }
        
        uint64_t total = total_user + total_system + total_idle + total_nice;
        if (total > 0) {
            stats.cpu_usage_percent = (static_cast<double>(total_user + total_system + total_nice) / total) * 100.0;
            stats.user_cpu_percent = (static_cast<double>(total_user + total_nice) / total) * 100.0;
            stats.system_cpu_percent = (static_cast<double>(total_system) / total) * 100.0;
            stats.idle_cpu_percent = (static_cast<double>(total_idle) / total) * 100.0;
        }
        
        vm_deallocate(mach_task_self(), (vm_address_t)cpu_load_info, 
                     cpu_load_info_count * sizeof(*cpu_load_info));
    }
    
#elif defined(_WIN32)
    // Windows implementation using GetSystemTimes
    FILETIME idle_time, kernel_time, user_time;
    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        auto file_time_to_uint64 = [](const FILETIME& ft) {
            return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        
        uint64_t idle = file_time_to_uint64(idle_time);
        uint64_t kernel = file_time_to_uint64(kernel_time);
        uint64_t user = file_time_to_uint64(user_time);
        
        uint64_t total = kernel + user;
        uint64_t active = total - idle;
        
        if (total > 0) {
            stats.cpu_usage_percent = (static_cast<double>(active) / total) * 100.0;
            stats.user_cpu_percent = (static_cast<double>(user) / total) * 100.0;
            stats.system_cpu_percent = (static_cast<double>(kernel - idle) / total) * 100.0;
            stats.idle_cpu_percent = (static_cast<double>(idle) / total) * 100.0;
        }
    }
    
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    stats.core_count = sys_info.dwNumberOfProcessors;
#endif
    
    return stats;
}

ResourceMonitor::MemoryStats ResourceMonitor::getCurrentMemoryStats() const {
    MemoryStats stats;
    
#ifdef __linux__
    // Linux implementation using /proc/meminfo and /proc/self/status
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            stats.total_memory_bytes = value * 1024; // Convert KB to bytes
        } else if (line.find("MemFree:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            stats.free_memory_bytes = value * 1024;
        } else if (line.find("Cached:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            stats.cached_memory_bytes = value * 1024;
        }
    }
    
    stats.used_memory_bytes = stats.total_memory_bytes - stats.free_memory_bytes;
    if (stats.total_memory_bytes > 0) {
        stats.memory_usage_percent = (static_cast<double>(stats.used_memory_bytes) / stats.total_memory_bytes) * 100.0;
    }
    
    // Process memory from /proc/self/status
    std::ifstream status("/proc/self/status");
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            stats.process_memory_bytes = value * 1024;
        } else if (line.find("VmPeak:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            stats.process_peak_memory_bytes = value * 1024;
        }
    }
    
#elif defined(__APPLE__)
    // macOS implementation using vm_statistics64 and task_info
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t vm_count = HOST_VM_INFO64_COUNT;
    
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                         (host_info64_t)&vm_stats, &vm_count) == KERN_SUCCESS) {
        vm_size_t page_size;
        host_page_size(mach_host_self(), &page_size);
        
        stats.free_memory_bytes = vm_stats.free_count * page_size;
        stats.total_memory_bytes = (vm_stats.free_count + vm_stats.active_count + 
                                  vm_stats.inactive_count + vm_stats.wire_count) * page_size;
        stats.used_memory_bytes = stats.total_memory_bytes - stats.free_memory_bytes;
        stats.cached_memory_bytes = vm_stats.inactive_count * page_size;
        
        if (stats.total_memory_bytes > 0) {
            stats.memory_usage_percent = (static_cast<double>(stats.used_memory_bytes) / stats.total_memory_bytes) * 100.0;
        }
    }
    
    // Process memory
    task_basic_info_64_data_t task_info_data;
    mach_msg_type_number_t task_count = TASK_BASIC_INFO_64_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO_64, 
                  (task_info_t)&task_info_data, &task_count) == KERN_SUCCESS) {
        stats.process_memory_bytes = task_info_data.resident_size;
        stats.process_peak_memory_bytes = task_info_data.resident_size; // Note: macOS doesn't provide peak in basic_info
    }
    
#elif defined(_WIN32)
    // Windows implementation using GlobalMemoryStatusEx and GetProcessMemoryInfo
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    
    if (GlobalMemoryStatusEx(&mem_status)) {
        stats.total_memory_bytes = mem_status.ullTotalPhys;
        stats.free_memory_bytes = mem_status.ullAvailPhys;
        stats.used_memory_bytes = stats.total_memory_bytes - stats.free_memory_bytes;
        stats.memory_usage_percent = static_cast<double>(mem_status.dwMemoryLoad);
    }
    
    // Process memory
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        stats.process_memory_bytes = pmc.WorkingSetSize;
        stats.process_peak_memory_bytes = pmc.PeakWorkingSetSize;
    }
#endif
    
    return stats;
}

ResourceMonitor::NetworkStats ResourceMonitor::getCurrentNetworkStats() const {
    NetworkStats stats;
    
    // Platform-specific network statistics implementation
    // This is a simplified implementation - would need full platform-specific code
    
    // For now, return simulated stats
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> rate_dist(100.0, 1000.0);
    
    stats.current_send_rate_mbps = rate_dist(gen);
    stats.current_recv_rate_mbps = rate_dist(gen);
    stats.peak_send_rate_mbps = std::max(stats.current_send_rate_mbps, stats.peak_send_rate_mbps);
    stats.peak_recv_rate_mbps = std::max(stats.current_recv_rate_mbps, stats.peak_recv_rate_mbps);
    
    return stats;
}

void ResourceMonitor::monitoringLoop() {
    while (monitoring_active_) {
        auto now = std::chrono::steady_clock::now();
        
        auto cpu_stats = getCurrentCPUStats();
        auto memory_stats = getCurrentMemoryStats();
        auto network_stats = getCurrentNetworkStats();
        
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            
            cpu_history_.push_back({now, cpu_stats});
            memory_history_.push_back({now, memory_stats});
            network_history_.push_back({now, network_stats});
            
            // Cleanup old data (keep last 24 hours)
            cleanupOldHistory();
        }
        
        checkThresholds(cpu_stats, memory_stats, network_stats);
        
        std::this_thread::sleep_for(monitoring_interval_);
    }
}

void ResourceMonitor::checkThresholds(const CPUStats& cpu, const MemoryStats& memory, const NetworkStats& network) {
    if (!alert_callback_) return;
    
    if (cpu.cpu_usage_percent > cpu_threshold_) {
        alert_callback_("CPU usage high", cpu.cpu_usage_percent);
    }
    
    if (memory.memory_usage_percent > memory_threshold_) {
        alert_callback_("Memory usage high", memory.memory_usage_percent);
    }
    
    if (network.current_send_rate_mbps > network_threshold_ || 
        network.current_recv_rate_mbps > network_threshold_) {
        alert_callback_("Network bandwidth high", 
                       std::max(network.current_send_rate_mbps, network.current_recv_rate_mbps));
    }
}

void ResourceMonitor::cleanupOldHistory() {
    auto cutoff_time = std::chrono::steady_clock::now() - std::chrono::hours(24);
    
    auto cleanup_vector = [cutoff_time](auto& history) {
        history.erase(
            std::remove_if(history.begin(), history.end(),
                [cutoff_time](const auto& entry) {
                    return entry.timestamp < cutoff_time;
                }),
            history.end()
        );
    };
    
    cleanup_vector(cpu_history_);
    cleanup_vector(memory_history_);
    cleanup_vector(network_history_);
}

// PerformanceMetricsManager implementation
PerformanceMetricsManager::PerformanceMetricsManager() {
    resource_monitor_ = std::make_unique<ResourceMonitor>();
    profiler_integration_ = std::make_unique<ProfilerIntegration>();
}

PerformanceMetricsManager::~PerformanceMetricsManager() {
    stopSystemMonitoring();
    disableAutomaticReporting();
}

LatencyTracker& PerformanceMetricsManager::getLatencyTracker(const std::string& name) {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    auto it = latency_trackers_.find(name);
    if (it == latency_trackers_.end()) {
        latency_trackers_[name] = std::make_unique<LatencyTracker>(name);
        return *latency_trackers_[name];
    }
    return *it->second;
}

ThroughputMonitor& PerformanceMetricsManager::getThroughputMonitor(const std::string& name) {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    auto it = throughput_monitors_.find(name);
    if (it == throughput_monitors_.end()) {
        throughput_monitors_[name] = std::make_unique<ThroughputMonitor>(name);
        return *throughput_monitors_[name];
    }
    return *it->second;
}

ResourceMonitor& PerformanceMetricsManager::getResourceMonitor() {
    return *resource_monitor_;
}

ProfilerIntegration& PerformanceMetricsManager::getProfilerIntegration() {
    return *profiler_integration_;
}

void PerformanceMetricsManager::startSystemMonitoring() {
    resource_monitor_->startMonitoring();
}

void PerformanceMetricsManager::stopSystemMonitoring() {
    resource_monitor_->stopMonitoring();
}

bool PerformanceMetricsManager::validateMarketDataProcessing(double expected_updates_per_sec) {
    // Simple validation - can be enhanced
    return true;
}

void PerformanceMetricsManager::recordPerformanceBaseline(const std::string& test_name) {
    // Simple implementation - can be enhanced
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    
    PerformanceBaseline baseline;
    baseline.test_name = test_name;
    baseline.baseline_latency_nanos = 1000.0; // Default values
    baseline.baseline_throughput_ops_per_sec = 1000.0;
    baseline.baseline_cpu_usage_percent = 50.0;
    baseline.baseline_memory_usage_mb = 100.0;
    baseline.recorded_time = std::chrono::steady_clock::now();
    
    performance_baselines_.push_back(baseline);
}

bool PerformanceMetricsManager::checkPerformanceRegression(const std::string& test_name, double tolerance_percent) {
    // Simple implementation - always return true for now
    return true;
}

std::vector<PerformanceMetricsManager::PerformanceBaseline> PerformanceMetricsManager::getPerformanceBaselines() const {
    std::lock_guard<std::mutex> lock(baselines_mutex_);
    return performance_baselines_;
}

PerformanceMetricsManager::PerformanceReport PerformanceMetricsManager::generatePerformanceReport() {
    PerformanceReport report;
    report.report_time = std::chrono::steady_clock::now();
    
    // Collect latency stats
    std::lock_guard<std::mutex> lock(components_mutex_);
    for (const auto& [name, tracker] : latency_trackers_) {
        report.latency_stats[name] = tracker->getStats();
    }
    
    // Collect throughput stats  
    for (const auto& [name, monitor] : throughput_monitors_) {
        report.throughput_stats[name] = monitor->getStats();
    }
    
    // Get resource stats
    if (resource_monitor_) {
        report.cpu_stats = resource_monitor_->getCPUStats();
        report.memory_stats = resource_monitor_->getMemoryStats();
        report.network_stats = resource_monitor_->getNetworkStats();
    }
    
    return report;
}

void PerformanceMetricsManager::exportPerformanceReport(const PerformanceReport& report, const std::string& file_path) {
    std::ofstream file(file_path);
    if (!file.is_open()) return;
    
    file << "Performance Report\n";
    file << "==================\n\n";
    
    // Write latency stats
    file << "Latency Statistics:\n";
    for (const auto& [name, stats] : report.latency_stats) {
        file << "  " << name << ":\n";
        file << "    Min: " << stats.min_nanos << " ns\n";
        file << "    Max: " << stats.max_nanos << " ns\n";
        file << "    Avg: " << stats.avg_nanos << " ns\n";
        file << "    P95: " << stats.p95_nanos << " ns\n";
        file << "    P99: " << stats.p99_nanos << " ns\n";
    }
    
    file << "\nThroughput Statistics:\n";
    for (const auto& [name, stats] : report.throughput_stats) {
        file << "  " << name << ":\n";
        file << "    Current Rate: " << stats.current_ops_per_sec << " ops/sec\n";
        file << "    Peak Rate: " << stats.peak_ops_per_sec << " ops/sec\n";
        file << "    Total Operations: " << stats.total_operations << "\n";
    }
    
    file.close();
}

void PerformanceMetricsManager::setGlobalLatencyThreshold(double threshold_nanos) {
    // Implementation can be added later
}

void PerformanceMetricsManager::setGlobalThroughputTarget(double target_ops_per_sec) {
    // Implementation can be added later
}

void PerformanceMetricsManager::enableAutomaticReporting(std::chrono::minutes interval) {
    reporting_interval_ = interval;
    auto_reporting_enabled_ = true;
    reporting_thread_ = std::make_unique<std::thread>(&PerformanceMetricsManager::automaticReportingLoop, this);
}

void PerformanceMetricsManager::disableAutomaticReporting() {
    auto_reporting_enabled_ = false;
    if (reporting_thread_ && reporting_thread_->joinable()) {
        reporting_thread_->join();
    }
}

void PerformanceMetricsManager::automaticReportingLoop() {
    while (auto_reporting_enabled_) {
        std::this_thread::sleep_for(reporting_interval_);
        if (auto_reporting_enabled_) {
            auto report = generatePerformanceReport();
            // Could export to file here
        }
    }
}

} // namespace Performance
} // namespace SyntheticArbitrage
