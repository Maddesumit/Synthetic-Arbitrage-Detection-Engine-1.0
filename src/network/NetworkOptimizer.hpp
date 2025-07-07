#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>
#include <functional>
#include <chrono>

namespace SyntheticArbitrage {
namespace Network {

// Forward declarations
class WebSocketConnection;
class ConnectionPool;
class SerializationEngine;
class BandwidthOptimizer;

// Network configuration structure
struct NetworkConfig {
    size_t max_connections_per_pool = 10;
    std::chrono::milliseconds connection_timeout{5000};
    std::chrono::milliseconds keepalive_interval{30000};
    size_t max_frame_size = 64 * 1024; // 64KB
    bool enable_compression = true;
    std::string compression_algorithm = "lz4";
    size_t max_bandwidth_mbps = 1000; // 1Gbps
    bool enable_latency_compensation = true;
    std::chrono::microseconds max_acceptable_latency{1000}; // 1ms
};

// WebSocket frame optimization
class OptimizedWebSocketFrame {
public:
    enum class OpCode : uint8_t {
        CONTINUATION = 0x0,
        TEXT = 0x1,
        BINARY = 0x2,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xA
    };
    
    OptimizedWebSocketFrame();
    OptimizedWebSocketFrame(OpCode opcode, const std::vector<uint8_t>& payload);
    ~OptimizedWebSocketFrame();
    
    // Efficient frame construction
    void setOpCode(OpCode opcode);
    void setPayload(const std::vector<uint8_t>& payload);
    void setPayload(std::vector<uint8_t>&& payload);
    
    // Zero-copy operations
    const std::vector<uint8_t>& getPayload() const;
    std::vector<uint8_t>&& movePayload();
    
    // Frame serialization with minimal copying
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    
    // Frame compression
    void compress();
    void decompress();
    
    // Validation
    bool isValid() const;
    size_t getFrameSize() const;

private:
    OpCode opcode_;
    bool fin_;
    bool compressed_;
    std::vector<uint8_t> payload_;
    mutable std::mutex payload_mutex_;
};

// High-performance WebSocket connection
class WebSocketConnection {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CLOSING,
        CLOSED,
        ERROR
    };
    
    WebSocketConnection(const std::string& url, const NetworkConfig& config);
    ~WebSocketConnection();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    State getState() const;
    
    // High-performance message sending
    bool send(const OptimizedWebSocketFrame& frame);
    bool send(const std::string& message);
    bool send(const std::vector<uint8_t>& binary_data);
    
    // Batch sending for reduced system calls
    bool sendBatch(const std::vector<OptimizedWebSocketFrame>& frames);
    
    // Message receiving with callbacks
    void setMessageHandler(std::function<void(const OptimizedWebSocketFrame&)> handler);
    void setErrorHandler(std::function<void(const std::string&)> handler);
    
    // Performance metrics
    struct ConnectionMetrics {
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> connection_errors{0};
        std::chrono::microseconds average_latency{0};
        std::chrono::steady_clock::time_point last_message_time;
    };
    
    const ConnectionMetrics& getMetrics() const;
    void resetMetrics();
    
    // Latency compensation
    void enableLatencyCompensation(bool enable);
    std::chrono::microseconds getEstimatedLatency() const;

private:
    std::string url_;
    NetworkConfig config_;
    std::atomic<State> state_;
    
    // Connection state
    std::unique_ptr<std::thread> connection_thread_;
    std::unique_ptr<std::thread> receiver_thread_;
    
    // Message handlers
    std::function<void(const OptimizedWebSocketFrame&)> message_handler_;
    std::function<void(const std::string&)> error_handler_;
    
    // Performance tracking
    mutable ConnectionMetrics metrics_;
    std::atomic<std::chrono::microseconds> estimated_latency_;
    
    // Thread synchronization
    mutable std::mutex state_mutex_;
    std::condition_variable state_cv_;
    
    // Internal methods
    void connectionLoop();
    void receiverLoop();
    void updateLatencyEstimate(std::chrono::microseconds round_trip_time);
    void handlePing(const OptimizedWebSocketFrame& frame);
    void handlePong(const OptimizedWebSocketFrame& frame);
};

// Connection pooling for multiple exchanges
class ConnectionPool {
public:
    ConnectionPool(const NetworkConfig& config);
    ~ConnectionPool();
    
    // Pool management
    std::shared_ptr<WebSocketConnection> getConnection(const std::string& exchange_id, const std::string& url);
    void releaseConnection(const std::string& exchange_id, std::shared_ptr<WebSocketConnection> connection);
    void closeAll();
    
    // Pool statistics
    struct PoolStatistics {
        size_t total_connections = 0;
        size_t active_connections = 0;
        size_t idle_connections = 0;
        std::unordered_map<std::string, size_t> connections_per_exchange;
    };
    
    PoolStatistics getStatistics() const;
    
    // Health monitoring
    void performHealthCheck();
    void removeUnhealthyConnections();

private:
    NetworkConfig config_;
    
    struct ConnectionInfo {
        std::shared_ptr<WebSocketConnection> connection;
        std::chrono::steady_clock::time_point last_used;
        bool in_use;
    };
    
    std::unordered_map<std::string, std::vector<ConnectionInfo>> connection_pools_;
    mutable std::mutex pools_mutex_;
    
    // Health monitoring
    std::unique_ptr<std::thread> health_monitor_thread_;
    std::atomic<bool> health_monitor_running_;
    
    void healthMonitorLoop();
    bool isConnectionHealthy(const std::shared_ptr<WebSocketConnection>& connection) const;
};

// High-performance serialization engine
class SerializationEngine {
public:
    enum class Format {
        JSON,
        MSGPACK,
        PROTOBUF,
        CUSTOM_BINARY
    };
    
    SerializationEngine(Format format = Format::MSGPACK);
    ~SerializationEngine();
    
    // Serialization methods
    std::vector<uint8_t> serialize(const std::string& json_data);
    std::string deserialize(const std::vector<uint8_t>& binary_data);
    
    // Batch operations for better performance
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::string>& json_data);
    std::vector<std::string> deserializeBatch(const std::vector<std::vector<uint8_t>>& binary_data);
    
    // Zero-copy operations when possible
    bool serializeInPlace(std::vector<uint8_t>& buffer, const std::string& json_data);
    bool deserializeInPlace(std::string& output, const std::vector<uint8_t>& binary_data);
    
    // Performance metrics
    struct SerializationMetrics {
        std::atomic<uint64_t> serializations{0};
        std::atomic<uint64_t> deserializations{0};
        std::atomic<uint64_t> total_input_bytes{0};
        std::atomic<uint64_t> total_output_bytes{0};
        std::atomic<uint64_t> compression_ratio_percent{100}; // 100% = no compression
    };
    
    const SerializationMetrics& getMetrics() const;
    void resetMetrics();

private:
    Format format_;
    mutable SerializationMetrics metrics_;
    
    // Format-specific implementations
    std::vector<uint8_t> serializeJSON(const std::string& json_data);
    std::vector<uint8_t> serializeMsgPack(const std::string& json_data);
    std::vector<uint8_t> serializeProtobuf(const std::string& json_data);
    std::vector<uint8_t> serializeBinary(const std::string& json_data);
    
    std::string deserializeJSON(const std::vector<uint8_t>& binary_data);
    std::string deserializeMsgPack(const std::vector<uint8_t>& binary_data);
    std::string deserializeProtobuf(const std::vector<uint8_t>& binary_data);
    std::string deserializeBinary(const std::vector<uint8_t>& binary_data);
};

// Bandwidth optimization and management
class BandwidthOptimizer {
public:
    BandwidthOptimizer(const NetworkConfig& config);
    ~BandwidthOptimizer();
    
    // Bandwidth monitoring
    void recordDataTransfer(size_t bytes_sent, size_t bytes_received);
    double getCurrentBandwidthUtilization() const; // Percentage
    size_t getCurrentThroughput() const; // Bytes per second
    
    // Traffic shaping
    bool shouldThrottleConnection(const std::string& exchange_id) const;
    std::chrono::milliseconds getThrottleDelay(const std::string& exchange_id) const;
    
    // Data compression optimization
    void enableCompressionFor(const std::string& exchange_id, bool enable);
    bool isCompressionEnabled(const std::string& exchange_id) const;
    
    // Adaptive optimization
    void optimizeForLatency(); // Prioritize low latency over bandwidth
    void optimizeForThroughput(); // Prioritize high throughput over latency
    void enableAdaptiveOptimization(bool enable);
    
    // Statistics and reporting
    struct BandwidthStatistics {
        double current_utilization_percent = 0.0;
        size_t total_bytes_sent = 0;
        size_t total_bytes_received = 0;
        size_t current_throughput_bps = 0;
        size_t peak_throughput_bps = 0;
        std::chrono::steady_clock::time_point measurement_start_time;
        std::unordered_map<std::string, size_t> exchange_bandwidth_usage;
    };
    
    BandwidthStatistics getStatistics() const;
    void resetStatistics();

private:
    NetworkConfig config_;
    
    // Bandwidth tracking
    std::atomic<size_t> bytes_sent_{0};
    std::atomic<size_t> bytes_received_{0};
    std::chrono::steady_clock::time_point measurement_start_;
    
    // Per-exchange tracking
    std::unordered_map<std::string, std::atomic<size_t>> exchange_bytes_sent_;
    std::unordered_map<std::string, std::atomic<size_t>> exchange_bytes_received_;
    std::unordered_map<std::string, bool> compression_enabled_;
    
    // Traffic shaping
    std::atomic<bool> adaptive_optimization_{true};
    std::atomic<bool> optimize_for_latency_{false};
    
    mutable std::mutex statistics_mutex_;
    
    // Monitoring thread
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitor_running_;
    
    void monitoringLoop();
    void adaptiveOptimizationLogic();
};

// Network optimization manager
class NetworkOptimizationManager {
public:
    NetworkOptimizationManager(const NetworkConfig& config = NetworkConfig{});
    ~NetworkOptimizationManager();
    
    // Component access
    ConnectionPool& getConnectionPool();
    SerializationEngine& getSerializationEngine();
    BandwidthOptimizer& getBandwidthOptimizer();
    
    // High-level operations
    std::shared_ptr<WebSocketConnection> createOptimizedConnection(const std::string& exchange_id, 
                                                                  const std::string& url);
    
    // Batch operations for multiple exchanges
    bool sendToMultipleExchanges(const std::unordered_map<std::string, std::string>& exchange_messages);
    
    // Performance monitoring
    struct NetworkPerformanceMetrics {
        size_t total_connections = 0;
        double average_latency_ms = 0.0;
        size_t total_throughput_bps = 0;
        double bandwidth_utilization_percent = 0.0;
        size_t serialization_rate = 0; // Operations per second
        std::unordered_map<std::string, double> exchange_latencies;
    };
    
    NetworkPerformanceMetrics getPerformanceMetrics() const;
    
    // Configuration updates
    void updateConfig(const NetworkConfig& config);
    const NetworkConfig& getConfig() const;

private:
    NetworkConfig config_;
    
    std::unique_ptr<ConnectionPool> connection_pool_;
    std::unique_ptr<SerializationEngine> serialization_engine_;
    std::unique_ptr<BandwidthOptimizer> bandwidth_optimizer_;
    
    mutable std::mutex config_mutex_;
};

} // namespace Network
} // namespace SyntheticArbitrage
