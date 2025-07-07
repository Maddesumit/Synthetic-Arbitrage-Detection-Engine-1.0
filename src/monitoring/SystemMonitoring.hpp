#pragma once

#include "../performance/PerformanceMetrics.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <queue>
#include <fstream>

namespace SyntheticArbitrage {
namespace Monitoring {

// Forward declarations
class SystemHealthMonitor;
class PerformanceDashboard;
class BottleneckDetector;
class TrendAnalyzer;
class AlertManager;

// System health status enumeration
enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    UNKNOWN
};

// Health check result structure
struct HealthCheckResult {
    std::string component_name;
    HealthStatus status;
    double metric_value;
    double threshold_value;
    std::string description;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

// System health monitor
class SystemHealthMonitor {
public:
    SystemHealthMonitor();
    ~SystemHealthMonitor();
    
    // Health check management
    void startHealthChecks(std::chrono::seconds interval = std::chrono::seconds(30));
    void stopHealthChecks();
    bool isMonitoring() const;
    
    // Register health checks
    void registerHealthCheck(const std::string& name, 
                           std::function<HealthCheckResult()> check_function);
    void unregisterHealthCheck(const std::string& name);
    
    // Manual health checks
    HealthCheckResult runHealthCheck(const std::string& name);
    std::vector<HealthCheckResult> runAllHealthChecks();
    
    // Health status queries
    HealthStatus getOverallHealth() const;
    HealthStatus getComponentHealth(const std::string& component) const;
    std::vector<HealthCheckResult> getHealthHistory(std::chrono::minutes duration) const;
    
    // Predictive failure detection
    struct FailurePrediction {
        std::string component_name;
        double failure_probability; // 0.0 to 1.0
        std::chrono::minutes estimated_time_to_failure;
        std::string prediction_reason;
        std::vector<std::string> recommended_actions;
    };
    
    std::vector<FailurePrediction> getPredictedFailures() const;
    void enablePredictiveAnalysis(bool enable);
    
    // Alert callbacks
    void setHealthAlertCallback(std::function<void(const HealthCheckResult&)> callback);
    void setFailurePredictionCallback(std::function<void(const FailurePrediction&)> callback);

private:
    // Health check storage
    std::unordered_map<std::string, std::function<HealthCheckResult()>> health_checks_;
    std::vector<HealthCheckResult> health_history_;
    mutable std::mutex health_mutex_;
    
    // Monitoring control
    std::atomic<bool> monitoring_active_{false};
    std::unique_ptr<std::thread> monitoring_thread_;
    std::chrono::seconds check_interval_;
    
    // Predictive analysis
    std::atomic<bool> predictive_analysis_enabled_{true};
    std::unique_ptr<std::thread> prediction_thread_;
    
    // Alert callbacks
    std::function<void(const HealthCheckResult&)> health_alert_callback_;
    std::function<void(const FailurePrediction&)> failure_prediction_callback_;
    
    // Internal methods
    void monitoringLoop();
    void predictionLoop();
    void analyzeHealthTrends();
    FailurePrediction predictComponentFailure(const std::string& component) const;
    void cleanupOldHistory();
    
    // Built-in health checks
    HealthCheckResult checkCPUHealth();
    HealthCheckResult checkMemoryHealth();
    HealthCheckResult checkNetworkHealth();
    HealthCheckResult checkLatencyHealth();
    HealthCheckResult checkThroughputHealth();
};

// Real-time performance dashboard data provider
class PerformanceDashboard {
public:
    struct DashboardMetrics {
        // System metrics
        double cpu_usage_percent;
        double memory_usage_percent;
        double network_utilization_percent;
        
        // Performance metrics
        double average_latency_ms;
        double p95_latency_ms;
        double p99_latency_ms;
        double current_throughput_ops_per_sec;
        double peak_throughput_ops_per_sec;
        
        // Health status
        HealthStatus overall_health;
        std::vector<std::pair<std::string, HealthStatus>> component_health;
        
        // Alerts and warnings
        uint32_t active_alerts;
        uint32_t warnings_last_hour;
        
        // Timestamp
        std::chrono::steady_clock::time_point timestamp;
    };
    
    PerformanceDashboard(Performance::PerformanceMetricsManager& metrics_manager,
                        SystemHealthMonitor& health_monitor);
    ~PerformanceDashboard();
    
    // Dashboard data
    DashboardMetrics getCurrentMetrics() const;
    std::vector<DashboardMetrics> getMetricsHistory(std::chrono::hours duration) const;
    
    // Real-time updates
    void enableRealTimeUpdates(bool enable);
    void setUpdateInterval(std::chrono::seconds interval);
    void setMetricsUpdateCallback(std::function<void(const DashboardMetrics&)> callback);
    
    // Key performance indicators
    struct KPI {
        std::string name;
        double current_value;
        double target_value;
        double threshold_warning;
        double threshold_critical;
        std::string unit;
        bool higher_is_better;
    };
    
    std::vector<KPI> getKeyPerformanceIndicators() const;
    void addCustomKPI(const KPI& kpi);
    void removeCustomKPI(const std::string& name);
    
    // Dashboard configuration
    void configureDashboard(const std::unordered_map<std::string, std::string>& config);
    std::unordered_map<std::string, std::string> getDashboardConfig() const;

private:
    Performance::PerformanceMetricsManager& metrics_manager_;
    SystemHealthMonitor& health_monitor_;
    
    // Real-time updates
    std::atomic<bool> real_time_updates_{false};
    std::unique_ptr<std::thread> update_thread_;
    std::chrono::seconds update_interval_;
    std::function<void(const DashboardMetrics&)> metrics_callback_;
    
    // Historical data
    std::vector<DashboardMetrics> metrics_history_;
    mutable std::mutex history_mutex_;
    
    // Custom KPIs
    std::vector<KPI> custom_kpis_;
    mutable std::mutex kpi_mutex_;
    
    // Configuration
    std::unordered_map<std::string, std::string> dashboard_config_;
    mutable std::mutex config_mutex_;
    
    void updateLoop();
    void cleanupOldMetrics();
    DashboardMetrics collectCurrentMetrics() const;
};

// Performance bottleneck detection and analysis
class BottleneckDetector {
public:
    enum class BottleneckType {
        CPU_BOUND,
        MEMORY_BOUND,
        NETWORK_BOUND,
        IO_BOUND,
        LOCK_CONTENTION,
        ALGORITHM_INEFFICIENCY,
        UNKNOWN
    };
    
    struct BottleneckReport {
        BottleneckType type;
        std::string component_name;
        double severity_score; // 0.0 to 1.0
        std::string description;
        std::vector<std::string> symptoms;
        std::vector<std::string> recommended_solutions;
        std::chrono::steady_clock::time_point detection_time;
        std::unordered_map<std::string, double> supporting_metrics;
    };
    
    BottleneckDetector(Performance::PerformanceMetricsManager& metrics_manager);
    ~BottleneckDetector();
    
    // Bottleneck detection
    void startBottleneckDetection(std::chrono::seconds analysis_interval = std::chrono::seconds(60));
    void stopBottleneckDetection();
    bool isDetectionActive() const;
    
    // Analysis methods
    std::vector<BottleneckReport> detectBottlenecks();
    BottleneckReport analyzeComponent(const std::string& component_name);
    
    // Historical analysis
    std::vector<BottleneckReport> getBottleneckHistory(std::chrono::hours duration) const;
    std::vector<BottleneckReport> getBottlenecksByType(BottleneckType type) const;
    
    // Configuration
    void setDetectionThresholds(const std::unordered_map<std::string, double>& thresholds);
    void enableAutomaticRemediation(bool enable);
    
    // Alert callbacks
    void setBottleneckDetectedCallback(std::function<void(const BottleneckReport&)> callback);

private:
    Performance::PerformanceMetricsManager& metrics_manager_;
    
    // Detection control
    std::atomic<bool> detection_active_{false};
    std::unique_ptr<std::thread> detection_thread_;
    std::chrono::seconds analysis_interval_;
    
    // Historical data
    std::vector<BottleneckReport> bottleneck_history_;
    mutable std::mutex history_mutex_;
    
    // Configuration
    std::unordered_map<std::string, double> detection_thresholds_;
    std::atomic<bool> automatic_remediation_{false};
    
    // Alert callback
    std::function<void(const BottleneckReport&)> bottleneck_callback_;
    
    // Detection algorithms
    void detectionLoop();
    BottleneckReport detectCPUBottleneck();
    BottleneckReport detectMemoryBottleneck();
    BottleneckReport detectNetworkBottleneck();
    BottleneckReport detectLockContentionBottleneck();
    BottleneckReport detectAlgorithmBottleneck();
    
    // Analysis utilities
    double calculateSeverityScore(BottleneckType type, const std::unordered_map<std::string, double>& metrics);
    std::vector<std::string> generateRecommendations(const BottleneckReport& report);
    void attemptAutomaticRemediation(const BottleneckReport& report);
};

// Performance trend analysis and forecasting
class TrendAnalyzer {
public:
    struct TrendData {
        std::string metric_name;
        std::vector<double> values;
        std::vector<std::chrono::steady_clock::time_point> timestamps;
        double trend_slope; // Linear trend coefficient
        double trend_r_squared; // Correlation coefficient
        double seasonal_component; // Seasonal pattern strength
        std::chrono::minutes forecast_horizon;
        std::vector<double> forecast_values;
        std::vector<double> confidence_intervals;
    };
    
    enum class TrendDirection {
        INCREASING,
        DECREASING,
        STABLE,
        VOLATILE,
        SEASONAL
    };
    
    TrendAnalyzer();
    ~TrendAnalyzer();
    
    // Trend analysis
    TrendData analyzeMetricTrend(const std::string& metric_name,
                                const std::vector<double>& values,
                                const std::vector<std::chrono::steady_clock::time_point>& timestamps);
    
    TrendDirection classifyTrend(const TrendData& trend_data);
    
    // Forecasting
    std::vector<double> forecastMetric(const TrendData& trend_data, 
                                     std::chrono::minutes forecast_horizon);
    
    double calculateForecastAccuracy(const TrendData& historical_trend,
                                   const std::vector<double>& actual_values);
    
    // Anomaly detection
    struct AnomalyReport {
        std::string metric_name;
        double anomaly_score; // 0.0 to 1.0
        double expected_value;
        double actual_value;
        std::chrono::steady_clock::time_point timestamp;
        std::string anomaly_description;
    };
    
    std::vector<AnomalyReport> detectAnomalies(const TrendData& trend_data,
                                             const std::vector<double>& recent_values);
    
    // Performance regression detection
    struct RegressionReport {
        std::string metric_name;
        double baseline_value;
        double current_value;
        double regression_percentage;
        std::chrono::steady_clock::time_point regression_start;
        bool statistically_significant;
        std::string regression_description;
    };
    
    RegressionReport detectPerformanceRegression(const std::string& metric_name,
                                               double baseline_value,
                                               const std::vector<double>& current_values);
    
    // Capacity planning
    struct CapacityForecast {
        std::string resource_name;
        double current_utilization_percent;
        double predicted_utilization_percent;
        std::chrono::months time_to_capacity_limit;
        std::vector<std::string> scaling_recommendations;
    };
    
    CapacityForecast generateCapacityForecast(const std::string& resource_name,
                                            const TrendData& utilization_trend);

private:
    // Statistical analysis utilities
    double calculateLinearTrend(const std::vector<double>& values,
                              const std::vector<std::chrono::steady_clock::time_point>& timestamps);
    
    double calculateCorrelationCoefficient(const std::vector<double>& x,
                                         const std::vector<double>& y);
    
    std::vector<double> detectSeasonalPattern(const std::vector<double>& values,
                                            const std::vector<std::chrono::steady_clock::time_point>& timestamps);
    
    std::vector<double> smoothData(const std::vector<double>& values, size_t window_size);
    
    // Forecasting algorithms
    std::vector<double> exponentialSmoothing(const std::vector<double>& values,
                                           size_t forecast_steps,
                                           double alpha = 0.3);
    
    std::vector<double> linearExtrapolation(const std::vector<double>& values,
                                          const std::vector<std::chrono::steady_clock::time_point>& timestamps,
                                          std::chrono::minutes forecast_horizon);
    
    // Anomaly detection algorithms
    double calculateZScore(double value, const std::vector<double>& baseline_values);
    bool isStatisticalOutlier(double value, const std::vector<double>& baseline_values);
    
    // Configuration
    double anomaly_threshold_ = 2.0; // Z-score threshold
    size_t min_samples_for_analysis_ = 10;
    double regression_significance_threshold_ = 0.05; // p-value
};

// Comprehensive alert management system
class AlertManager {
public:
    enum class AlertSeverity {
        INFO,
        WARNING,
        CRITICAL,
        EMERGENCY
    };
    
    enum class AlertType {
        PERFORMANCE_DEGRADATION,
        RESOURCE_EXHAUSTION,
        BOTTLENECK_DETECTED,
        HEALTH_CHECK_FAILED,
        ANOMALY_DETECTED,
        PREDICTION_WARNING,
        CUSTOM
    };
    
    struct Alert {
        std::string alert_id;
        AlertType type;
        AlertSeverity severity;
        std::string title;
        std::string description;
        std::string component_name;
        std::unordered_map<std::string, std::string> metadata;
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point acknowledged_time;
        std::chrono::steady_clock::time_point resolved_time;
        bool acknowledged = false;
        bool resolved = false;
        std::string assigned_to;
        std::vector<std::string> actions_taken;
    };
    
    AlertManager();
    ~AlertManager();
    
    // Alert creation and management
    std::string createAlert(AlertType type, AlertSeverity severity,
                          const std::string& title, const std::string& description,
                          const std::string& component_name = "");
    
    void acknowledgeAlert(const std::string& alert_id, const std::string& acknowledged_by = "");
    void resolveAlert(const std::string& alert_id, const std::string& resolution_notes = "");
    void addActionToAlert(const std::string& alert_id, const std::string& action);
    
    // Alert queries
    std::vector<Alert> getActiveAlerts() const;
    std::vector<Alert> getAlertsByComponent(const std::string& component_name) const;
    std::vector<Alert> getAlertsBySeverity(AlertSeverity severity) const;
    std::vector<Alert> getAlertsInTimeRange(std::chrono::steady_clock::time_point start,
                                          std::chrono::steady_clock::time_point end) const;
    
    Alert getAlert(const std::string& alert_id) const;
    bool alertExists(const std::string& alert_id) const;
    
    // Alert statistics
    struct AlertStatistics {
        uint32_t total_alerts_created;
        uint32_t active_alerts;
        uint32_t acknowledged_alerts;
        uint32_t resolved_alerts;
        std::unordered_map<AlertSeverity, uint32_t> alerts_by_severity;
        std::unordered_map<AlertType, uint32_t> alerts_by_type;
        std::chrono::minutes average_acknowledgment_time;
        std::chrono::minutes average_resolution_time;
    };
    
    AlertStatistics getAlertStatistics() const;
    
    // Notification management
    void setNotificationCallback(std::function<void(const Alert&)> callback);
    void enableEmailNotifications(bool enable, const std::string& smtp_config = "");
    void enableSlackNotifications(bool enable, const std::string& webhook_url = "");
    void enableSMSNotifications(bool enable, const std::string& twilio_config = "");
    
    // Alert rules and escalation
    struct AlertRule {
        std::string rule_name;
        AlertType type;
        std::function<bool(const std::unordered_map<std::string, double>&)> condition;
        AlertSeverity severity;
        std::string title_template;
        std::string description_template;
        std::chrono::seconds cooldown_period;
        std::chrono::steady_clock::time_point last_triggered;
    };
    
    void addAlertRule(const AlertRule& rule);
    void removeAlertRule(const std::string& rule_name);
    void evaluateAlertRules(const std::unordered_map<std::string, double>& metrics);
    
    // Alert escalation
    struct EscalationLevel {
        std::chrono::minutes delay;
        AlertSeverity escalated_severity;
        std::vector<std::string> notification_channels;
        std::string escalation_message;
    };
    
    void configureEscalation(const std::vector<EscalationLevel>& escalation_levels);
    void checkEscalations();

private:
    // Alert storage
    std::unordered_map<std::string, Alert> alerts_;
    mutable std::mutex alerts_mutex_;
    
    // Alert rules
    std::vector<AlertRule> alert_rules_;
    mutable std::mutex rules_mutex_;
    
    // Escalation configuration
    std::vector<EscalationLevel> escalation_levels_;
    std::unique_ptr<std::thread> escalation_thread_;
    std::atomic<bool> escalation_monitoring_{true};
    
    // Notification callbacks
    std::function<void(const Alert&)> notification_callback_;
    std::atomic<bool> email_notifications_{false};
    std::atomic<bool> slack_notifications_{false};
    std::atomic<bool> sms_notifications_{false};
    
    std::string smtp_config_;
    std::string slack_webhook_;
    std::string twilio_config_;
    
    // Internal methods
    std::string generateAlertId();
    void sendNotification(const Alert& alert);
    void sendEmailNotification(const Alert& alert);
    void sendSlackNotification(const Alert& alert);
    void sendSMSNotification(const Alert& alert);
    void escalationLoop();
    void cleanupOldAlerts();
};

// System monitoring and analytics manager
class SystemMonitoringManager {
public:
    SystemMonitoringManager(Performance::PerformanceMetricsManager& metrics_manager);
    ~SystemMonitoringManager();
    
    // Component access
    SystemHealthMonitor& getHealthMonitor();
    PerformanceDashboard& getDashboard();
    BottleneckDetector& getBottleneckDetector();
    TrendAnalyzer& getTrendAnalyzer();
    AlertManager& getAlertManager();
    
    // Integrated monitoring control
    void startComprehensiveMonitoring();
    void stopComprehensiveMonitoring();
    bool isMonitoringActive() const;
    
    // Configuration
    void loadConfiguration(const std::string& config_file_path);
    void saveConfiguration(const std::string& config_file_path);
    
    // Comprehensive reporting
    struct SystemReport {
        PerformanceDashboard::DashboardMetrics current_metrics;
        std::vector<HealthCheckResult> health_status;
        std::vector<BottleneckDetector::BottleneckReport> detected_bottlenecks;
        std::vector<TrendAnalyzer::AnomalyReport> anomalies;
        std::vector<AlertManager::Alert> active_alerts;
        AlertManager::AlertStatistics alert_statistics;
        std::chrono::steady_clock::time_point report_timestamp;
    };
    
    SystemReport generateSystemReport();
    void exportSystemReport(const SystemReport& report, const std::string& file_path);
    
    // Automated actions
    void enableAutomatedRemediation(bool enable);
    void addRemediationAction(const std::string& condition, std::function<void()> action);

private:
    Performance::PerformanceMetricsManager& metrics_manager_;
    
    std::unique_ptr<SystemHealthMonitor> health_monitor_;
    std::unique_ptr<PerformanceDashboard> dashboard_;
    std::unique_ptr<BottleneckDetector> bottleneck_detector_;
    std::unique_ptr<TrendAnalyzer> trend_analyzer_;
    std::unique_ptr<AlertManager> alert_manager_;
    
    std::atomic<bool> monitoring_active_{false};
    std::atomic<bool> automated_remediation_{false};
    
    // Remediation actions
    std::unordered_map<std::string, std::function<void()>> remediation_actions_;
    mutable std::mutex remediation_mutex_;
    
    // Integration callbacks
    void setupIntegratedCallbacks();
    void handleHealthAlert(const HealthCheckResult& result);
    void handleBottleneckDetection(const BottleneckDetector::BottleneckReport& report);
    void executeRemediationIfApplicable(const std::string& condition);
};

} // namespace Monitoring
} // namespace SyntheticArbitrage
