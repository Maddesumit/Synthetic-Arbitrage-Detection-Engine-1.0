#include "BybitClient.hpp"
#include "utils/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

namespace arbitrage {
namespace data {

BybitClient::BybitClient() : WebSocketClient(Exchange::BYBIT) {
    LOG_INFO("Initializing Bybit WebSocket client for real API connection");
    
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

BybitClient::~BybitClient() {
    LOG_INFO("Destroying Bybit WebSocket client");
    disconnect();
}

bool BybitClient::connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (is_connected_ || is_connecting_) {
        LOG_WARN("Bybit client already connected or connecting");
        return is_connected_;
    }
    
    LOG_INFO("Connecting to Bybit WebSocket API");
    
    try {
        is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to Bybit endpoint
        websocketpp::lib::error_code ec;
        std::string uri = BYBIT_WS_BASE;
        
        connection_ = ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create Bybit connection: " + ec.message());
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
        event_loop_thread_ = std::thread(&BybitClient::runEventLoop, this);
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        is_connecting_ = false;
        
        return is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during Bybit connection: " + std::string(e.what()));
        is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void BybitClient::disconnect() {
    LOG_INFO("Disconnecting from Bybit WebSocket API");
    
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
            LOG_WARN("Error closing Bybit connection: " + ec.message());
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

bool BybitClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = formatSymbolForBybit(symbol);
    std::string topic = "tickers." + bybit_symbol;
    
    if (active_subscriptions_.find(topic) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to Bybit ticker: " + topic);
        return true;
    }
    
    LOG_INFO("Subscribing to Bybit ticker: " + topic);
    
    // Bybit subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = formatSymbolForBybit(symbol);
    std::string topic = "orderbook.1." + bybit_symbol;
    
    if (active_subscriptions_.find(topic) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to Bybit order book: " + topic);
        return true;
    }
    
    LOG_INFO("Subscribing to Bybit order book: " + topic);
    
    // Bybit subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = formatSymbolForBybit(symbol);
    std::string topic = "publicTrade." + bybit_symbol;
    
    if (active_subscriptions_.find(topic) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to Bybit trades: " + topic);
        return true;
    }
    
    LOG_INFO("Subscribing to Bybit trades: " + topic);
    
    // Bybit subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::subscribeFundingRate(const std::string& symbol) {
    // Funding rate subscription for Bybit futures
    return true; // Placeholder for now
}

bool BybitClient::subscribeMarkPrice(const std::string& symbol) {
    // Mark price subscription for Bybit futures
    return true; // Placeholder for now
}

bool BybitClient::unsubscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string bybit_symbol = formatSymbolForBybit(symbol);
    std::string topic = "orderbook.1." + bybit_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string bybit_symbol = formatSymbolForBybit(symbol);
    std::string topic = "publicTrade." + bybit_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(topic);
        return true;
    }
    
    return false;
}

void BybitClient::parseMessage(const std::string& message) {
    try {
        auto data = nlohmann::json::parse(message);
        
        // Check if this is a data update
        if (data.contains("data") && data.contains("topic")) {
            std::string topic = data["topic"];
            auto data_content = data["data"];
            
            if (topic.find("tickers.") == 0) {
                parseTickerMessage(data_content);
            } else if (topic.find("orderbook.") == 0) {
                parseOrderBookMessage(data_content);
            } else if (topic.find("publicTrade.") == 0) {
                for (const auto& trade_data : data_content) {
                    parseTradeMessage(trade_data);
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit message: " + std::string(e.what()));
    }
}

bool BybitClient::sendSubscriptionMessage(const std::string& message) {
    if (!is_connected_ || !connection_) {
        LOG_WARN("Cannot send message: not connected to Bybit");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        ws_client_->send(connection_hdl_, message, websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            LOG_ERROR("Failed to send Bybit subscription message: " + ec.message());
            return false;
        }
        
        LOG_DEBUG("Sent Bybit subscription message: " + message);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception sending Bybit subscription message: " + std::string(e.what()));
        return false;
    }
}

void BybitClient::onOpen(websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to Bybit WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
        connection_hdl_ = hdl;
    }
    
    updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void BybitClient::onClose(websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from Bybit WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void BybitClient::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    parseMessage(msg->get_payload());
}

void BybitClient::onFail(websocketpp::connection_hdl hdl) {
    LOG_ERROR("Bybit WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::ERROR);
}

void BybitClient::parseTickerMessage(const nlohmann::json& data) {
    try {
        Ticker ticker;
        ticker.symbol = data["symbol"];
        ticker.exchange = Exchange::BYBIT;
        ticker.timestamp = std::chrono::system_clock::now();
        
        // Bybit ticker data format
        ticker.bid = std::stod(data["bid1Price"].get<std::string>());
        ticker.ask = std::stod(data["ask1Price"].get<std::string>());
        ticker.last = std::stod(data["lastPrice"].get<std::string>());
        ticker.volume = std::stod(data["volume24h"].get<std::string>());
        ticker.price_change_24h = std::stod(data["price24hPcnt"].get<std::string>());
        
        // Call parent class callback
        onTickerReceived(ticker);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit ticker message: " + std::string(e.what()));
    }
}

void BybitClient::parseOrderBookMessage(const nlohmann::json& data) {
    try {
        OrderBook order_book;
        order_book.symbol = data["s"];
        order_book.exchange = Exchange::BYBIT;
        order_book.timestamp = std::chrono::system_clock::now();
        
        // Parse bids
        if (data.contains("b")) {
            for (const auto& bid_array : data["b"]) {
                if (bid_array.size() >= 2) {
                    double price = std::stod(bid_array[0].get<std::string>());
                    double quantity = std::stod(bid_array[1].get<std::string>());
                    order_book.bids.emplace_back(price, quantity);
                }
            }
        }
        
        // Parse asks
        if (data.contains("a")) {
            for (const auto& ask_array : data["a"]) {
                if (ask_array.size() >= 2) {
                    double price = std::stod(ask_array[0].get<std::string>());
                    double quantity = std::stod(ask_array[1].get<std::string>());
                    order_book.asks.emplace_back(price, quantity);
                }
            }
        }
        
        // Call parent class callback
        onOrderBookReceived(order_book);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit order book message: " + std::string(e.what()));
    }
}

void BybitClient::parseTradeMessage(const nlohmann::json& data) {
    try {
        Trade trade;
        trade.symbol = data["s"];
        trade.exchange = Exchange::BYBIT;
        trade.timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(std::stoll(data["T"].get<std::string>()))
        );
        trade.price = std::stod(data["p"].get<std::string>());
        trade.quantity = std::stod(data["v"].get<std::string>());
        trade.is_buyer_maker = (data["S"].get<std::string>() == "Sell");
        
        // Call parent class callback
        onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit trade message: " + std::string(e.what()));
    }
}

void BybitClient::runEventLoop() {
    LOG_INFO("Starting Bybit WebSocket event loop");
    
    try {
        ws_client_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in Bybit event loop: " + std::string(e.what()));
    }
    
    LOG_INFO("Bybit WebSocket event loop stopped");
}

std::string BybitClient::formatSymbolForBybit(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "BTCUSDT" for Bybit)
    std::string result = symbol;
    
    // Remove dashes and convert to uppercase
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    
    return result;
}

} // namespace data
} // namespace arbitrage
