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

namespace arbitrage {
namespace data {

BinanceClient::BinanceClient() : WebSocketClient(Exchange::BINANCE) {
    LOG_INFO("Initializing Binance WebSocket client for real API connection");
    
    // Initialize WebSocket++ client
    ws_client_ = std::make_unique<client_type>();
    
    // Set up WebSocket++ client
    ws_client_->set_access_channels(websocketpp::log::alevel::all);
    ws_client_->clear_access_channels(websocketpp::log::alevel::frame_payload);
    ws_client_->set_error_channels(websocketpp::log::elevel::all);
    
    // Initialize ASIO
    ws_client_->init_asio();
    
    // Set TLS init handler for secure connections
    ws_client_->set_tls_init_handler([](websocketpp::connection_hdl) {
        return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
    });
}

BinanceClient::~BinanceClient() {
    LOG_INFO("Destroying Binance WebSocket client");
    disconnect();
}

bool BinanceClient::connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (is_connected_ || is_connecting_) {
        LOG_WARN("Binance client already connected or connecting");
        return is_connected_;
    }
    
    LOG_INFO("Connecting to Binance WebSocket API");
    
    try {
        is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to Binance stream endpoint
        websocketpp::lib::error_code ec;
        std::string uri = "wss://stream.binance.com:9443/ws";
        
        connection_ = ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create connection: " + ec.message());
            is_connecting_ = false;
            updateConnectionStatus(ConnectionStatus::ERROR);
            return false;
        }
        
        // Set connection event handlers
        connection_->set_open_handler([this](websocketpp::connection_hdl hdl) {
            onOpen(hdl);
        });
        
        connection_->set_close_handler([this](websocketpp::connection_hdl hdl) {
            onClose(hdl);
        });
        
        connection_->set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            onMessage(hdl, msg);
        });
        
        connection_->set_fail_handler([this](websocketpp::connection_hdl hdl) {
            onFail(hdl);
        });
        
        // Connect
        ws_client_->connect(connection_);
        
        // Start event loop in separate thread
        event_loop_running_ = true;
        event_loop_thread_ = std::thread(&BinanceClient::runEventLoop, this);
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        is_connecting_ = false;
        
        return is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during Binance connection: " + std::string(e.what()));
        is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void BinanceClient::disconnect() {
    LOG_INFO("Disconnecting from Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
        updateConnectionStatus(ConnectionStatus::DISCONNECTED);
    }
    
    // Stop event loop
    event_loop_running_ = false;
    
    // Close WebSocket connection
    if (connection_) {
        websocketpp::lib::error_code ec;
        ws_client_->close(connection_, websocketpp::close::status::normal, "Closing connection", ec);
        if (ec) {
            LOG_WARN("Error closing connection: " + ec.message());
        }
    }
    
    // Wait for event loop thread to finish
    if (event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }
    
    // Clear subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        active_subscriptions_.clear();
    }
}

bool BinanceClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@ticker";
    
    if (active_subscriptions_.find(stream) != active_subscriptions_.end()) {
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
        active_subscriptions_.insert(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@depth";
    
    if (active_subscriptions_.find(stream) != active_subscriptions_.end()) {
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
        active_subscriptions_.insert(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to Binance");
        return false;
    }
    
    std::string binance_symbol = formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@trade";
    
    if (active_subscriptions_.find(stream) != active_subscriptions_.end()) {
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
        active_subscriptions_.insert(stream);
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
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string binance_symbol = formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@depth";
    
    nlohmann::json unsub_msg = {
        {"method", "UNSUBSCRIBE"},
        {"params", {stream}},
        {"id", 4}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(stream);
        return true;
    }
    
    return false;
}

bool BinanceClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string binance_symbol = formatSymbolForBinance(symbol);
    std::string stream = binance_symbol + "@trade";
    
    nlohmann::json unsub_msg = {
        {"method", "UNSUBSCRIBE"},
        {"params", {stream}},
        {"id", 5}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(stream);
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
                parseTickerMessage(stream_data);
            } else if (stream.find("@depth") != std::string::npos) {
                parseOrderBookMessage(stream_data);
            } else if (stream.find("@trade") != std::string::npos) {
                parseTradeMessage(stream_data);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance message: " + std::string(e.what()));
    }
}

bool BinanceClient::sendSubscriptionMessage(const std::string& message) {
    if (!is_connected_ || !connection_) {
        LOG_WARN("Cannot send message: not connected to Binance");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        ws_client_->send(connection_hdl_, message, websocketpp::frame::opcode::text, ec);
        
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

void BinanceClient::onOpen(websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
        connection_hdl_ = hdl;
    }
    
    updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void BinanceClient::onClose(websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from Binance WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void BinanceClient::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    parseMessage(msg->get_payload());
}

void BinanceClient::onFail(websocketpp::connection_hdl hdl) {
    LOG_ERROR("Binance WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::ERROR);
}

void BinanceClient::parseTickerMessage(const nlohmann::json& data) {
    try {
        Ticker ticker;
        ticker.symbol = data["s"];
        ticker.exchange = Exchange::BINANCE;
        ticker.timestamp = std::chrono::system_clock::now();
        ticker.bid = std::stod(data["b"].get<std::string>());
        ticker.ask = std::stod(data["a"].get<std::string>());
        ticker.last = std::stod(data["c"].get<std::string>());
        ticker.volume = std::stod(data["v"].get<std::string>());
        ticker.price_change_24h = std::stod(data["P"].get<std::string>());
        ticker.price_change_percent_24h = std::stod(data["p"].get<std::string>());
        
        // Call parent class callback
        onTickerReceived(ticker);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance ticker message: " + std::string(e.what()));
    }
}

void BinanceClient::parseOrderBookMessage(const nlohmann::json& data) {
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
        onOrderBookReceived(order_book);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance order book message: " + std::string(e.what()));
    }
}

void BinanceClient::parseTradeMessage(const nlohmann::json& data) {
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
        onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Binance trade message: " + std::string(e.what()));
    }
}

void BinanceClient::parseFundingRateMessage(const nlohmann::json& data) {
    // Implementation for futures funding rate parsing
}

void BinanceClient::parseMarkPriceMessage(const nlohmann::json& data) {
    // Implementation for futures mark price parsing  
}

void BinanceClient::runEventLoop() {
    LOG_INFO("Starting Binance WebSocket event loop");
    
    try {
        ws_client_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in Binance event loop: " + std::string(e.what()));
    }
    
    LOG_INFO("Binance WebSocket event loop stopped");
}

std::string BinanceClient::formatSymbolForBinance(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "btcusdt")
    std::string result = symbol;
    
    // Remove dashes and convert to lowercase
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    return result;
}

} // namespace data
} // namespace arbitrage
