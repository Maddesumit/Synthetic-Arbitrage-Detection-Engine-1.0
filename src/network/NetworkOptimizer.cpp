#include "NetworkOptimizer.hpp"
#include <algorithm>
#include <sstream>
#include <zlib.h>
#ifndef NO_LZ4_COMPRESSION
#include <lz4.h>
#endif

// Check if RapidJSON is available
#if __has_include(<rapidjson/document.h>)
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#define HAS_RAPIDJSON 1
#else
#define HAS_RAPIDJSON 0
#endif

namespace SyntheticArbitrage {
namespace Network {

// OptimizedWebSocketFrame implementation
OptimizedWebSocketFrame::OptimizedWebSocketFrame() 
    : opcode_(OpCode::TEXT), fin_(true), compressed_(false) {
}

OptimizedWebSocketFrame::OptimizedWebSocketFrame(OpCode opcode, const std::vector<uint8_t>& payload)
    : opcode_(opcode), fin_(true), compressed_(false), payload_(payload) {
}

OptimizedWebSocketFrame::~OptimizedWebSocketFrame() = default;

void OptimizedWebSocketFrame::setOpCode(OpCode opcode) {
    opcode_ = opcode;
}

void OptimizedWebSocketFrame::setPayload(const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(payload_mutex_);
    payload_ = payload;
    compressed_ = false;
}

void OptimizedWebSocketFrame::setPayload(std::vector<uint8_t>&& payload) {
    std::lock_guard<std::mutex> lock(payload_mutex_);
    payload_ = std::move(payload);
    compressed_ = false;
}

const std::vector<uint8_t>& OptimizedWebSocketFrame::getPayload() const {
    return payload_;
}

std::vector<uint8_t>&& OptimizedWebSocketFrame::movePayload() {
    return std::move(payload_);
}

std::vector<uint8_t> OptimizedWebSocketFrame::serialize() const {
    std::vector<uint8_t> frame;
    
    // First byte: FIN + RSV + Opcode
    uint8_t first_byte = static_cast<uint8_t>(opcode_);
    if (fin_) first_byte |= 0x80;
    if (compressed_) first_byte |= 0x40; // RSV1 for compression
    
    frame.push_back(first_byte);
    
    // Payload length
    size_t payload_length = payload_.size();
    if (payload_length < 126) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>(payload_length >> 8));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (i * 8)) & 0xFF));
        }
    }
    
    // Payload data
    frame.insert(frame.end(), payload_.begin(), payload_.end());
    
    return frame;
}

bool OptimizedWebSocketFrame::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    
    size_t offset = 0;
    
    // Parse first byte
    uint8_t first_byte = data[offset++];
    fin_ = (first_byte & 0x80) != 0;
    compressed_ = (first_byte & 0x40) != 0;
    opcode_ = static_cast<OpCode>(first_byte & 0x0F);
    
    // Parse payload length
    uint8_t length_byte = data[offset++];
    bool masked = (length_byte & 0x80) != 0;
    uint64_t payload_length = length_byte & 0x7F;
    
    if (payload_length == 126) {
        if (data.size() < offset + 2) return false;
        payload_length = (static_cast<uint64_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    } else if (payload_length == 127) {
        if (data.size() < offset + 8) return false;
        payload_length = 0;
        for (int i = 0; i < 8; ++i) {
            payload_length = (payload_length << 8) | data[offset + i];
        }
        offset += 8;
    }
    
    // Handle masking key if present
    std::array<uint8_t, 4> mask_key = {0};
    if (masked) {
        if (data.size() < offset + 4) return false;
        std::copy(data.begin() + offset, data.begin() + offset + 4, mask_key.begin());
        offset += 4;
    }
    
    // Extract payload
    if (data.size() < offset + payload_length) return false;
    
    payload_.resize(payload_length);
    std::copy(data.begin() + offset, data.begin() + offset + payload_length, payload_.begin());
    
    // Unmask payload if necessary
    if (masked) {
        for (size_t i = 0; i < payload_length; ++i) {
            payload_[i] ^= mask_key[i % 4];
        }
    }
    
    return true;
}

void OptimizedWebSocketFrame::compress() {
    if (compressed_ || payload_.empty()) return;
    
#ifndef NO_LZ4_COMPRESSION
    // Use LZ4 for fast compression
    const int max_compressed_size = LZ4_compressBound(payload_.size());
    std::vector<uint8_t> compressed_data(max_compressed_size);
    
    const int compressed_size = LZ4_compress_default(
        reinterpret_cast<const char*>(payload_.data()),
        reinterpret_cast<char*>(compressed_data.data()),
        payload_.size(),
        max_compressed_size
    );
    
    if (compressed_size > 0 && compressed_size < static_cast<int>(payload_.size())) {
        compressed_data.resize(compressed_size);
        payload_ = std::move(compressed_data);
        compressed_ = true;
    }
#else
    // Compression disabled - no-op
    // Keep payload_ as-is, don't set compressed_ flag
#endif
}

void OptimizedWebSocketFrame::decompress() {
    if (!compressed_ || payload_.empty()) return;
    
    // For decompression, we need to know the original size
    // This is typically stored in the first 4 bytes of compressed data
    if (payload_.size() < 4) return;
    
    uint32_t original_size;
    std::memcpy(&original_size, payload_.data(), sizeof(original_size));
    
    std::vector<uint8_t> decompressed_data(original_size);
    
#ifndef NO_LZ4_COMPRESSION
    const int result = LZ4_decompress_safe(
        reinterpret_cast<const char*>(payload_.data() + 4),
        reinterpret_cast<char*>(decompressed_data.data()),
        payload_.size() - 4,
        original_size
    );
    
    if (result == static_cast<int>(original_size)) {
        payload_ = std::move(decompressed_data);
        compressed_ = false;
    }
#else
    // Decompression disabled - no-op
    // Keep payload_ as-is since we don't compress anyway
#endif
}

bool OptimizedWebSocketFrame::isValid() const {
    return opcode_ != static_cast<OpCode>(0xFF); // Invalid opcode
}

size_t OptimizedWebSocketFrame::getFrameSize() const {
    size_t header_size = 2; // Minimum header size
    
    size_t payload_length = payload_.size();
    if (payload_length >= 126) {
        header_size += (payload_length >= 65536) ? 8 : 2;
    }
    
    return header_size + payload_length;
}

// WebSocketConnection implementation
WebSocketConnection::WebSocketConnection(const std::string& url, const NetworkConfig& config)
    : url_(url), config_(config), state_(State::DISCONNECTED), 
      estimated_latency_(std::chrono::microseconds(0)) {
    metrics_.last_message_time = std::chrono::steady_clock::now();
}

WebSocketConnection::~WebSocketConnection() {
    disconnect();
}

bool WebSocketConnection::connect() {
    std::unique_lock<std::mutex> lock(state_mutex_);
    
    if (state_ == State::CONNECTED || state_ == State::CONNECTING) {
        return state_ == State::CONNECTED;
    }
    
    state_ = State::CONNECTING;
    
    connection_thread_ = std::make_unique<std::thread>(&WebSocketConnection::connectionLoop, this);
    
    // Wait for connection with timeout
    bool connected = state_cv_.wait_for(lock, config_.connection_timeout, 
        [this] { return state_ == State::CONNECTED || state_ == State::ERROR; });
    
    return connected && state_ == State::CONNECTED;
}

void WebSocketConnection::disconnect() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == State::DISCONNECTED || state_ == State::CLOSING) {
            return;
        }
        state_ = State::CLOSING;
    }
    
    state_cv_.notify_all();
    
    if (connection_thread_ && connection_thread_->joinable()) {
        connection_thread_->join();
    }
    
    if (receiver_thread_ && receiver_thread_->joinable()) {
        receiver_thread_->join();
    }
    
    state_ = State::DISCONNECTED;
}

bool WebSocketConnection::isConnected() const {
    return state_ == State::CONNECTED;
}

WebSocketConnection::State WebSocketConnection::getState() const {
    return state_.load();
}

bool WebSocketConnection::send(const OptimizedWebSocketFrame& frame) {
    if (!isConnected()) return false;
    
    auto serialized = frame.serialize();
    
    // Update metrics
    metrics_.messages_sent++;
    metrics_.bytes_sent += serialized.size();
    
    // Simulate sending (in real implementation, this would use actual WebSocket library)
    // For now, just return true to indicate successful send
    return true;
}

bool WebSocketConnection::send(const std::string& message) {
    OptimizedWebSocketFrame frame(OptimizedWebSocketFrame::OpCode::TEXT, 
                                 std::vector<uint8_t>(message.begin(), message.end()));
    return send(frame);
}

bool WebSocketConnection::send(const std::vector<uint8_t>& binary_data) {
    OptimizedWebSocketFrame frame(OptimizedWebSocketFrame::OpCode::BINARY, binary_data);
    return send(frame);
}

bool WebSocketConnection::sendBatch(const std::vector<OptimizedWebSocketFrame>& frames) {
    if (!isConnected()) return false;
    
    // Batch multiple frames to reduce system calls
    std::vector<uint8_t> batch_data;
    size_t total_size = 0;
    
    // Calculate total size
    for (const auto& frame : frames) {
        total_size += frame.getFrameSize();
    }
    
    batch_data.reserve(total_size);
    
    // Serialize all frames into single buffer
    for (const auto& frame : frames) {
        auto serialized = frame.serialize();
        batch_data.insert(batch_data.end(), serialized.begin(), serialized.end());
    }
    
    // Update metrics
    metrics_.messages_sent += frames.size();
    metrics_.bytes_sent += batch_data.size();
    
    // Send batch (simulated)
    return true;
}

void WebSocketConnection::setMessageHandler(std::function<void(const OptimizedWebSocketFrame&)> handler) {
    message_handler_ = std::move(handler);
}

void WebSocketConnection::setErrorHandler(std::function<void(const std::string&)> handler) {
    error_handler_ = std::move(handler);
}

const WebSocketConnection::ConnectionMetrics& WebSocketConnection::getMetrics() const {
    return metrics_;
}

void WebSocketConnection::resetMetrics() {
    metrics_.messages_sent = 0;
    metrics_.messages_received = 0;
    metrics_.bytes_sent = 0;
    metrics_.bytes_received = 0;
    metrics_.connection_errors = 0;
    metrics_.average_latency = std::chrono::microseconds(0);
    metrics_.last_message_time = std::chrono::steady_clock::now();
}

void WebSocketConnection::enableLatencyCompensation(bool enable) {
    // Implementation would enable/disable latency compensation logic
}

std::chrono::microseconds WebSocketConnection::getEstimatedLatency() const {
    return estimated_latency_.load();
}

void WebSocketConnection::connectionLoop() {
    // Simulate connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = State::CONNECTED;
    }
    state_cv_.notify_all();
    
    // Start receiver thread
    receiver_thread_ = std::make_unique<std::thread>(&WebSocketConnection::receiverLoop, this);
    
    // Keep connection alive
    while (state_ == State::CONNECTED) {
        std::this_thread::sleep_for(config_.keepalive_interval);
        
        // Send ping frame for keepalive
        OptimizedWebSocketFrame ping_frame(OptimizedWebSocketFrame::OpCode::PING, {});
        send(ping_frame);
    }
}

void WebSocketConnection::receiverLoop() {
    while (state_ == State::CONNECTED) {
        // Simulate receiving messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Update metrics
        metrics_.messages_received++;
        metrics_.last_message_time = std::chrono::steady_clock::now();
    }
}

void WebSocketConnection::updateLatencyEstimate(std::chrono::microseconds round_trip_time) {
    // Simple exponential moving average
    auto current = estimated_latency_.load();
    auto new_estimate = std::chrono::microseconds(
        static_cast<long>(0.9 * current.count() + 0.1 * round_trip_time.count())
    );
    estimated_latency_.store(new_estimate);
}

void WebSocketConnection::handlePing(const OptimizedWebSocketFrame& frame) {
    // Respond with pong
    OptimizedWebSocketFrame pong_frame(OptimizedWebSocketFrame::OpCode::PONG, frame.getPayload());
    send(pong_frame);
}

void WebSocketConnection::handlePong(const OptimizedWebSocketFrame& frame) {
    // Calculate latency from ping timestamp
    auto now = std::chrono::steady_clock::now();
    // Implementation would extract timestamp from payload and calculate RTT
}

// ConnectionPool implementation
ConnectionPool::ConnectionPool(const NetworkConfig& config) 
    : config_(config), health_monitor_running_(true) {
    health_monitor_thread_ = std::make_unique<std::thread>(&ConnectionPool::healthMonitorLoop, this);
}

ConnectionPool::~ConnectionPool() {
    health_monitor_running_ = false;
    if (health_monitor_thread_ && health_monitor_thread_->joinable()) {
        health_monitor_thread_->join();
    }
    closeAll();
}

std::shared_ptr<WebSocketConnection> ConnectionPool::getConnection(const std::string& exchange_id, 
                                                                  const std::string& url) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto& pool = connection_pools_[exchange_id];
    
    // Look for available connection
    for (auto& info : pool) {
        if (!info.in_use && isConnectionHealthy(info.connection)) {
            info.in_use = true;
            info.last_used = std::chrono::steady_clock::now();
            return info.connection;
        }
    }
    
    // Create new connection if pool has capacity
    if (pool.size() < config_.max_connections_per_pool) {
        auto connection = std::make_shared<WebSocketConnection>(url, config_);
        if (connection->connect()) {
            ConnectionInfo info;
            info.connection = connection;
            info.last_used = std::chrono::steady_clock::now();
            info.in_use = true;
            
            pool.push_back(info);
            return connection;
        }
    }
    
    return nullptr;
}

void ConnectionPool::releaseConnection(const std::string& exchange_id, 
                                     std::shared_ptr<WebSocketConnection> connection) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = connection_pools_.find(exchange_id);
    if (it != connection_pools_.end()) {
        for (auto& info : it->second) {
            if (info.connection == connection) {
                info.in_use = false;
                info.last_used = std::chrono::steady_clock::now();
                break;
            }
        }
    }
}

void ConnectionPool::closeAll() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    for (auto& [exchange_id, pool] : connection_pools_) {
        for (auto& info : pool) {
            info.connection->disconnect();
        }
        pool.clear();
    }
    
    connection_pools_.clear();
}

ConnectionPool::PoolStatistics ConnectionPool::getStatistics() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    PoolStatistics stats;
    
    for (const auto& [exchange_id, pool] : connection_pools_) {
        stats.total_connections += pool.size();
        stats.connections_per_exchange[exchange_id] = pool.size();
        
        for (const auto& info : pool) {
            if (info.in_use) {
                stats.active_connections++;
            } else {
                stats.idle_connections++;
            }
        }
    }
    
    return stats;
}

void ConnectionPool::performHealthCheck() {
    removeUnhealthyConnections();
}

void ConnectionPool::removeUnhealthyConnections() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    for (auto& [exchange_id, pool] : connection_pools_) {
        pool.erase(
            std::remove_if(pool.begin(), pool.end(),
                [this](const ConnectionInfo& info) {
                    return !info.in_use && !isConnectionHealthy(info.connection);
                }),
            pool.end()
        );
    }
}

void ConnectionPool::healthMonitorLoop() {
    while (health_monitor_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        performHealthCheck();
    }
}

bool ConnectionPool::isConnectionHealthy(const std::shared_ptr<WebSocketConnection>& connection) const {
    if (!connection || !connection->isConnected()) {
        return false;
    }
    
    const auto& metrics = connection->getMetrics();
    auto time_since_last_message = std::chrono::steady_clock::now() - metrics.last_message_time;
    
    // Consider connection unhealthy if no activity for too long
    return time_since_last_message < std::chrono::minutes(5);
}

// SerializationEngine implementation (simplified - would need full implementation)
SerializationEngine::SerializationEngine(Format format) : format_(format) {
}

SerializationEngine::~SerializationEngine() = default;

std::vector<uint8_t> SerializationEngine::serialize(const std::string& json_data) {
    metrics_.serializations++;
    metrics_.total_input_bytes += json_data.size();
    
    std::vector<uint8_t> result;
    
    switch (format_) {
        case Format::JSON:
            result = serializeJSON(json_data);
            break;
        case Format::MSGPACK:
            result = serializeMsgPack(json_data);
            break;
        case Format::PROTOBUF:
            result = serializeProtobuf(json_data);
            break;
        case Format::CUSTOM_BINARY:
            result = serializeBinary(json_data);
            break;
    }
    
    metrics_.total_output_bytes += result.size();
    if (json_data.size() > 0) {
        metrics_.compression_ratio_percent = (result.size() * 100) / json_data.size();
    }
    
    return result;
}

std::string SerializationEngine::deserialize(const std::vector<uint8_t>& binary_data) {
    metrics_.deserializations++;
    
    switch (format_) {
        case Format::JSON:
            return deserializeJSON(binary_data);
        case Format::MSGPACK:
            return deserializeMsgPack(binary_data);
        case Format::PROTOBUF:
            return deserializeProtobuf(binary_data);
        case Format::CUSTOM_BINARY:
            return deserializeBinary(binary_data);
    }
    
    return "";
}

// Simplified implementations (would need full implementation)
std::vector<uint8_t> SerializationEngine::serializeJSON(const std::string& json_data) {
    return std::vector<uint8_t>(json_data.begin(), json_data.end());
}

std::vector<uint8_t> SerializationEngine::serializeMsgPack(const std::string& json_data) {
    // Would use msgpack library for actual implementation
    return std::vector<uint8_t>(json_data.begin(), json_data.end());
}

std::vector<uint8_t> SerializationEngine::serializeProtobuf(const std::string& json_data) {
    // Would use protobuf library for actual implementation
    return std::vector<uint8_t>(json_data.begin(), json_data.end());
}

std::vector<uint8_t> SerializationEngine::serializeBinary(const std::string& json_data) {
    // Custom binary format implementation
    return std::vector<uint8_t>(json_data.begin(), json_data.end());
}

std::string SerializationEngine::deserializeJSON(const std::vector<uint8_t>& binary_data) {
    return std::string(binary_data.begin(), binary_data.end());
}

std::string SerializationEngine::deserializeMsgPack(const std::vector<uint8_t>& binary_data) {
    return std::string(binary_data.begin(), binary_data.end());
}

std::string SerializationEngine::deserializeProtobuf(const std::vector<uint8_t>& binary_data) {
    return std::string(binary_data.begin(), binary_data.end());
}

std::string SerializationEngine::deserializeBinary(const std::vector<uint8_t>& binary_data) {
    return std::string(binary_data.begin(), binary_data.end());
}

const SerializationEngine::SerializationMetrics& SerializationEngine::getMetrics() const {
    return metrics_;
}

void SerializationEngine::resetMetrics() {
    metrics_.serializations = 0;
    metrics_.deserializations = 0;
    metrics_.total_input_bytes = 0;
    metrics_.total_output_bytes = 0;
    metrics_.compression_ratio_percent = 100;
}

// NetworkOptimizationManager implementation
NetworkOptimizationManager::NetworkOptimizationManager(const NetworkConfig& config) 
    : config_(config) {
    connection_pool_ = std::make_unique<ConnectionPool>(config_);
    serialization_engine_ = std::make_unique<SerializationEngine>();
    bandwidth_optimizer_ = std::make_unique<BandwidthOptimizer>(config_);
}

NetworkOptimizationManager::~NetworkOptimizationManager() = default;

ConnectionPool& NetworkOptimizationManager::getConnectionPool() {
    return *connection_pool_;
}

SerializationEngine& NetworkOptimizationManager::getSerializationEngine() {
    return *serialization_engine_;
}

BandwidthOptimizer& NetworkOptimizationManager::getBandwidthOptimizer() {
    return *bandwidth_optimizer_;
}

std::shared_ptr<WebSocketConnection> NetworkOptimizationManager::createOptimizedConnection(
    const std::string& exchange_id, const std::string& url) {
    return connection_pool_->getConnection(exchange_id, url);
}

bool NetworkOptimizationManager::sendToMultipleExchanges(
    const std::unordered_map<std::string, std::string>& exchange_messages) {
    // Simple implementation - can be enhanced
    for (const auto& [exchange_id, message] : exchange_messages) {
        // We need a URL for getConnection - use a dummy one for now
        auto connection = connection_pool_->getConnection(exchange_id, "wss://default.url");
        if (connection && connection->isConnected()) {
            std::vector<uint8_t> data(message.begin(), message.end());
            connection->send(OptimizedWebSocketFrame(OptimizedWebSocketFrame::OpCode::TEXT, data));
        }
    }
    return true;
}

NetworkOptimizationManager::NetworkPerformanceMetrics NetworkOptimizationManager::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    NetworkPerformanceMetrics metrics;
    
    // Get connection pool statistics
    auto pool_stats = connection_pool_->getStatistics();
    metrics.total_connections = pool_stats.active_connections;
    metrics.average_latency_ms = 0.0; // Not available in PoolStatistics
    
    // Get bandwidth statistics
    auto bw_stats = bandwidth_optimizer_->getStatistics();
    metrics.total_throughput_bps = bw_stats.current_throughput_bps;
    metrics.bandwidth_utilization_percent = bw_stats.current_utilization_percent;
    
    // Get serialization metrics - access atomic values individually
    const auto& ser_metrics = serialization_engine_->getMetrics();
    metrics.serialization_rate = ser_metrics.serializations.load(); // Simplified
    
    return metrics;
}

void NetworkOptimizationManager::updateConfig(const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

const NetworkConfig& NetworkOptimizationManager::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

} // namespace Network

// Missing BandwidthOptimizer implementation
namespace Network {

BandwidthOptimizer::BandwidthOptimizer(const NetworkConfig& config) {
    // Simple initialization based on config
}

BandwidthOptimizer::~BandwidthOptimizer() = default;

BandwidthOptimizer::BandwidthStatistics BandwidthOptimizer::getStatistics() const {
    BandwidthStatistics stats;
    stats.current_throughput_bps = 1000000; // 1 Mbps
    stats.peak_throughput_bps = 10000000;   // 10 Mbps
    stats.current_utilization_percent = 10.0;
    stats.total_bytes_sent = 1000;
    stats.total_bytes_received = 800;
    return stats;
}

} // namespace Network
} // namespace SyntheticArbitrage
