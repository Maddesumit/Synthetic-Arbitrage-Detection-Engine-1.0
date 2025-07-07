#include "BinanceClient.hpp"
#include "utils/Logger.hpp"
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>
#include <set>

namespace arbitrage {
namespace data {

using client_type = ::websocketpp::client<::websocketpp::config::asio_tls_client>;
using message_ptr = ::websocketpp::config::asio_tls_client::message_type::ptr;

class BinanceClient::Impl {
public:
    std::unique_ptr<client_type> ws_client_;
    client_type::connection_ptr connection_;
    std::thread event_loop_thread_;
    std::atomic<bool> event_loop_running_{false};
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_connecting_{false};
    std::mutex connection_mutex_;
    std::mutex subscriptions_mutex_;
    std::set<std::string> active_subscriptions_;
    BinanceClient* parent_;  // Pointer back to parent for callbacks
    
    Impl(BinanceClient* parent) : parent_(parent) {
        ws_client_ = std::make_unique<client_type>();
        
        // Set up WebSocket++ client
        ws_client_->set_access_channels(::websocketpp::log::alevel::all);
        ws_client_->clear_access_channels(::websocketpp::log::alevel::frame_payload);
        ws_client_->set_error_channels(::websocketpp::log::elevel::all);
        
        // Initialize ASIO
        ws_client_->init_asio();
        
        // Set TLS init handler for secure connections
        ws_client_->set_tls_init_handler([](::websocketpp::connection_hdl) {
            return ::websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        });
    }
    
    // WebSocket event handlers
    void onOpen(::websocketpp::connection_hdl hdl);
    void onClose(::websocketpp::connection_hdl hdl);
    void onMessage(::websocketpp::connection_hdl hdl, message_ptr msg);
    void onFail(::websocketpp::connection_hdl hdl);
    
    // Message parsers
    void parseTickerMessage(const nlohmann::json& data);
    void parseOrderBookMessage(const nlohmann::json& data);
    void parseTradeMessage(const nlohmann::json& data);
    void parseFundingRateMessage(const nlohmann::json& data);
    void parseMarkPriceMessage(const nlohmann::json& data);
    
    // Utility methods
    void runEventLoop();
    std::string formatSymbolForBinance(const std::string& symbol) const;
};

BinanceClient::BinanceClient() : WebSocketClient(Exchange::BINANCE), pimpl_(std::make_unique<Impl>(this)) {
    LOG_INFO("Initializing Binance WebSocket client for real API connection");
    
    // Set conservative reconnection parameters to prevent crashes
    setReconnectParameters(5000, 60000, 1.5);  // 5s initial, 60s max, slower backoff
    setAutoReconnect(false);  // Temporarily disable auto-reconnect to prevent crashes
}

BinanceClient::~BinanceClient() {
    LOG_INFO("Destroying Binance WebSocket client");
    disconnect();
}

bool BinanceClient::connect() {
    std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
    
    if (pimpl_->is_connected_ || pimpl_->is_connecting_) {
        LOG_WARN("Binance client already connected or connecting");
        return pimpl_->is_connected_;
    }
    
    LOG_INFO("Connecting to Binance WebSocket API");
    
    try {
        pimpl_->is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to Binance stream endpoint
        ::websocketpp::lib::error_code ec;
        std::string uri = "wss://stream.binance.com:9443/ws";
        
        pimpl_->connection_ = pimpl_->ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create connection: " + ec.message());
            pimpl_->is_connecting_ = false;
            updateConnectionStatus(ConnectionStatus::ERROR);
            return false;
        }
        
        // Set connection event handlers
        pimpl_->connection_->set_open_handler([this](::websocketpp::connection_hdl hdl) {
            pimpl_->onOpen(hdl);
        });
        
        pimpl_->connection_->set_close_handler([this](::websocketpp::connection_hdl hdl) {
            pimpl_->onClose(hdl);
        });
        
        pimpl_->connection_->set_message_handler([this](::websocketpp::connection_hdl hdl, message_ptr msg) {
            pimpl_->onMessage(hdl, msg);
        });
        
        pimpl_->connection_->set_fail_handler([this](::websocketpp::connection_hdl hdl) {
            pimpl_->onFail(hdl);
        });
        
        // Connect
        pimpl_->ws_client_->connect(pimpl_->connection_);
        
        // Start event loop in separate thread
        pimpl_->event_loop_running_ = true;
        pimpl_->event_loop_thread_ = std::thread(&BinanceClient::Impl::runEventLoop, pimpl_.get());
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        pimpl_->is_connecting_ = false;
        
        return pimpl_->is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during Binance connection: " + std::string(e.what()));
        pimpl_->is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void BinanceClient::disconnect() {
    LOG_INFO("Disconnecting from Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
        pimpl_->is_connected_ = false;
        updateConnectionStatus(ConnectionStatus::DISCONNECTED);
    }
    
    // Stop event loop
    pimpl_->event_loop_running_ = false;
    
    // Close WebSocket connection
    if (pimpl_->connection_) {
        ::websocketpp::lib::error_code ec;
        pimpl_->ws_client_->close(pimpl_->connection_, ::websocketpp::close::status::normal, "Closing connection", ec);
        if (ec) {
            LOG_WARN("Error closing connection: " + ec.message());
        }
    }
    
    // Wait for event loop thread to finish
    if (pimpl_->event_loop_thread_.joinable()) {
        pimpl_->event_loop_thread_.join();
    }
    
    // Clear subscriptions
    {
        std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
        pimpl_->active_subscriptions_.clear();
    }
}

bool BinanceClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = pimpl_->formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@ticker";
    
    if (pimpl_->active_subscriptions_.find(stream) != pimpl_->active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to ticker stream: " + stream);
        return true;
    }
    
    LOG_INFO("Subscribing to Binance ticker stream: " + stream);
    
    // Send subscription message
    nlohmann::json sub_msg = {
        {"method", "SUBSCRIBE"},
        {"params", {stream}},
        {"id", 1}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        pimpl_->active_subscriptions_.insert(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = pimpl_->formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@depth";
    
    if (pimpl_->active_subscriptions_.find(stream) != pimpl_->active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to order book stream: " + stream);
        return true;
    }
    
    LOG_INFO("Subscribing to Binance order book stream: " + stream);
    
    // Send subscription message
    nlohmann::json sub_msg = {
        {"method", "SUBSCRIBE"},
        {"params", {stream}},
        {"id", 2}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        pimpl_->active_subscriptions_.insert(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = pimpl_->formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@trade";
    
    if (pimpl_->active_subscriptions_.find(stream) != pimpl_->active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to trade stream: " + stream);
        return true;
    }
    
    LOG_INFO("Subscribing to Binance trade stream: " + stream);
    
    // Send subscription message
    nlohmann::json sub_msg = {
        {"method", "SUBSCRIBE"},
        {"params", {stream}},
        {"id", 3}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        pimpl_->active_subscriptions_.insert(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::subscribeFundingRate(const std::string& symbol) {
    // Funding rate is for futures contracts, use different endpoint if needed
    return true; // Placeholder for now
}

bool BinanceClient::subscribeMarkPrice(const std::string& symbol) {
    // Mark price is for futures contracts, use different endpoint if needed  
    return true; // Placeholder for now
}

bool BinanceClient::unsubscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string binance_symbol = pimpl_->formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@depth";
    
    nlohmann::json unsub_msg = {
        {"method", "UNSUBSCRIBE"},
        {"params", {stream}},
        {"id", 4}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        pimpl_->active_subscriptions_.erase(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string binance_symbol = pimpl_->formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@trade";
    
    nlohmann::json unsub_msg = {
        {"method", "UNSUBSCRIBE"},
        {"params", {stream}},
        {"id", 5}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        pimpl_->active_subscriptions_.erase(stream);
        return true;
    }
    
    return false;
}

void BinanceClient::parseMessage(const std::string& message) {
    try {
        auto data = nlohmann::json::parse(message);
        
        // Check if this is a stream update
        if (data.contains("stream") && data.contains("data")) {
            std::string stream = data["stream"];
            auto stream_data = data["data"];
            
            if (stream.find("@ticker") != std::string::npos) {
                pimpl_->parseTickerMessage(stream_data);
            } else if (stream.find("@depth") != std::string::npos) {
                pimpl_->parseOrderBookMessage(stream_data);
            } else if (stream.find("@trade") != std::string::npos) {
                pimpl_->parseTradeMessage(stream_data);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance message: " + std::string(e.what()));
    }
}

bool BinanceClient::sendSubscriptionMessage(const std::string& message) {
    if (!pimpl_->is_connected_ || !pimpl_->connection_) {
        LOG_WARN("Cannot send message: not connected to Binance");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        pimpl_->ws_client_->send(pimpl_->connection_, message, websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            LOG_ERROR("Failed to send subscription message: " + ec.message());
            return false;
        }
        
        LOG_DEBUG("Sent subscription message: " + message);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception sending subscription message: " + std::string(e.what()));
        return false;
    }
}

void BinanceClient::Impl::onOpen(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void BinanceClient::Impl::onClose(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
        is_connecting_ = false;
        // Clear active subscriptions on disconnect
        active_subscriptions_.clear();
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void BinanceClient::Impl::onMessage(::websocketpp::connection_hdl hdl, message_ptr msg) {
    parent_->parseMessage(msg->get_payload());
}

void BinanceClient::Impl::onFail(::websocketpp::connection_hdl hdl) {
    LOG_ERROR("Binance WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::ERROR);
}

void BinanceClient::Impl::parseTickerMessage(const nlohmann::json& data) {
    try {
        // Check if required fields exist
        if (!data.contains("s") || !data.contains("b") || !data.contains("a") || 
            !data.contains("c") || !data.contains("v")) {
            LOG_WARN("Binance ticker message missing required fields");
            return;
        }
        
        Ticker ticker;
        ticker.symbol = data["s"];
        ticker.exchange = Exchange::BINANCE;
        ticker.timestamp = std::chrono::system_clock::now();
        ticker.bid = std::stod(data["b"].get<std::string>());
        ticker.ask = std::stod(data["a"].get<std::string>());
        ticker.last = std::stod(data["c"].get<std::string>());
        ticker.volume = std::stod(data["v"].get<std::string>());
        
        // Optional fields with defaults
        if (data.contains("P")) {
            ticker.price_change_24h = std::stod(data["P"].get<std::string>());
        }
        if (data.contains("p")) {
            ticker.price_change_percent_24h = std::stod(data["p"].get<std::string>());
        }
        
        // Call parent class callback
        parent_->onTickerReceived(ticker);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance ticker message: " + std::string(e.what()));
        LOG_DEBUG("Problematic data: " + data.dump());
    }
}

void BinanceClient::Impl::parseOrderBookMessage(const nlohmann::json& data) {
    try {
        OrderBook order_book;
        order_book.symbol = data["s"];
        order_book.exchange = Exchange::BINANCE;
        order_book.timestamp = std::chrono::system_clock::now();
        
        // Parse bids
        for (const auto& bid_array : data["bids"]) {
            if (bid_array.size() >= 2) {
                double price = std::stod(bid_array[0].get<std::string>());
                double quantity = std::stod(bid_array[1].get<std::string>());
                order_book.bids.emplace_back(price, quantity);
            }
        }
        
        // Parse asks
        for (const auto& ask_array : data["asks"]) {
            if (ask_array.size() >= 2) {
                double price = std::stod(ask_array[0].get<std::string>());
                double quantity = std::stod(ask_array[1].get<std::string>());
                order_book.asks.emplace_back(price, quantity);
            }
        }
        
        // Call parent class callback
        parent_->onOrderBookReceived(order_book);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance order book message: " + std::string(e.what()));
    }
}

void BinanceClient::Impl::parseTradeMessage(const nlohmann::json& data) {
    try {
        Trade trade;
        trade.symbol = data["s"];
        trade.exchange = Exchange::BINANCE;
        trade.timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(data["T"].get<int64_t>())
        );
        trade.price = std::stod(data["p"].get<std::string>());
        trade.quantity = std::stod(data["q"].get<std::string>());
        trade.is_buyer_maker = data["m"];
        
        // Call parent class callback
        parent_->onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance trade message: " + std::string(e.what()));
    }
}

void BinanceClient::Impl::parseFundingRateMessage(const nlohmann::json& data) {
    // Implementation for futures funding rate parsing
}

void BinanceClient::Impl::parseMarkPriceMessage(const nlohmann::json& data) {
    // Implementation for futures mark price parsing  
}

void BinanceClient::Impl::runEventLoop() {
    LOG_INFO("Starting Binance WebSocket event loop");
    event_loop_running_ = true;
    
    try {
        ws_client_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in Binance event loop: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in Binance event loop");
    }
    
    event_loop_running_ = false;
    LOG_INFO("Binance WebSocket event loop stopped");
}

std::string BinanceClient::Impl::formatSymbolForBinance(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "btcusdt")
    std::string result = symbol;
    
    // Remove dashes and convert to lowercase
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    return result;
}

} // namespace data
} // namespace arbitrage
