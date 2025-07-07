#include "OKXClient.hpp"
#include "utils/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

namespace arbitrage {
namespace data {

OKXClient::OKXClient() : WebSocketClient(Exchange::OKX) {
    LOG_INFO("Initializing OKX WebSocket client for real API connection");
    
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

OKXClient::~OKXClient() {
    LOG_INFO("Destroying OKX WebSocket client");
    disconnect();
}

bool OKXClient::connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (is_connected_ || is_connecting_) {
        LOG_WARN("OKX client already connected or connecting");
        return is_connected_;
    }
    
    LOG_INFO("Connecting to OKX WebSocket API");
    
    try {
        is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to OKX endpoint
        websocketpp::lib::error_code ec;
        std::string uri = OKX_WS_BASE;
        
        connection_ = ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create OKX connection: " + ec.message());
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
        event_loop_thread_ = std::thread(&OKXClient::runEventLoop, this);
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        is_connecting_ = false;
        
        return is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during OKX connection: " + std::string(e.what()));
        is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void OKXClient::disconnect() {
    LOG_INFO("Disconnecting from OKX WebSocket API");
    
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
            LOG_WARN("Error closing OKX connection: " + ec.message());
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

bool OKXClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = formatSymbolForOKX(symbol);
    std::string channel = "tickers";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (active_subscriptions_.find(subscription_key) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to OKX ticker: " + subscription_key);
        return true;
    }
    
    LOG_INFO("Subscribing to OKX ticker: " + subscription_key);
    
    // OKX subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {
            {
                {"channel", channel},
                {"instId", okx_symbol}
            }
        }}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = formatSymbolForOKX(symbol);
    std::string channel = "books";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (active_subscriptions_.find(subscription_key) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to OKX order book: " + subscription_key);
        return true;
    }
    
    LOG_INFO("Subscribing to OKX order book: " + subscription_key);
    
    // OKX subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {
            {
                {"channel", channel},
                {"instId", okx_symbol}
            }
        }}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (!is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = formatSymbolForOKX(symbol);
    std::string channel = "trades";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (active_subscriptions_.find(subscription_key) != active_subscriptions_.end()) {
        LOG_DEBUG("Already subscribed to OKX trades: " + subscription_key);
        return true;
    }
    
    LOG_INFO("Subscribing to OKX trades: " + subscription_key);
    
    // OKX subscription message format
    nlohmann::json sub_msg = {
        {"op", "subscribe"},
        {"args", {
            {
                {"channel", channel},
                {"instId", okx_symbol}
            }
        }}
    };
    
    if (sendSubscriptionMessage(sub_msg.dump())) {
        active_subscriptions_.insert(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::subscribeFundingRate(const std::string& symbol) {
    // Funding rate subscription for OKX futures
    return true; // Placeholder for now
}

bool OKXClient::subscribeMarkPrice(const std::string& symbol) {
    // Mark price subscription for OKX futures
    return true; // Placeholder for now
}

bool OKXClient::unsubscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string okx_symbol = formatSymbolForOKX(symbol);
    std::string channel = "books";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {
            {
                {"channel", channel},
                {"instId", okx_symbol}
            }
        }}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::string okx_symbol = formatSymbolForOKX(symbol);
    std::string channel = "trades";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {
            {
                {"channel", channel},
                {"instId", okx_symbol}
            }
        }}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        active_subscriptions_.erase(subscription_key);
        return true;
    }
    
    return false;
}

void OKXClient::parseMessage(const std::string& message) {
    try {
        auto data = nlohmann::json::parse(message);
        
        // Check if this is a data update
        if (data.contains("data") && data.contains("arg")) {
            auto arg = data["arg"];
            auto data_array = data["data"];
            
            if (arg.contains("channel")) {
                std::string channel = arg["channel"];
                
                if (channel == "tickers") {
                    for (const auto& ticker_data : data_array) {
                        parseTickerMessage(ticker_data);
                    }
                } else if (channel == "books") {
                    for (const auto& book_data : data_array) {
                        parseOrderBookMessage(book_data);
                    }
                } else if (channel == "trades") {
                    for (const auto& trade_data : data_array) {
                        parseTradeMessage(trade_data);
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX message: " + std::string(e.what()));
    }
}

bool OKXClient::sendSubscriptionMessage(const std::string& message) {
    if (!is_connected_ || !connection_) {
        LOG_WARN("Cannot send message: not connected to OKX");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        ws_client_->send(connection_hdl_, message, websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            LOG_ERROR("Failed to send OKX subscription message: " + ec.message());
            return false;
        }
        
        LOG_DEBUG("Sent OKX subscription message: " + message);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception sending OKX subscription message: " + std::string(e.what()));
        return false;
    }
}

void OKXClient::onOpen(websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to OKX WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
        connection_hdl_ = hdl;
    }
    
    updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void OKXClient::onClose(websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from OKX WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void OKXClient::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    parseMessage(msg->get_payload());
}

void OKXClient::onFail(websocketpp::connection_hdl hdl) {
    LOG_ERROR("OKX WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    updateConnectionStatus(ConnectionStatus::ERROR);
}

void OKXClient::parseTickerMessage(const nlohmann::json& data) {
    try {
        Ticker ticker;
        ticker.symbol = data["instId"];
        ticker.exchange = Exchange::OKX;
        ticker.timestamp = std::chrono::system_clock::now();
        
        // OKX ticker data format
        ticker.bid = std::stod(data["bidPx"].get<std::string>());
        ticker.ask = std::stod(data["askPx"].get<std::string>());
        ticker.last = std::stod(data["last"].get<std::string>());
        ticker.volume = std::stod(data["vol24h"].get<std::string>());
        
        // Call parent class callback
        onTickerReceived(ticker);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX ticker message: " + std::string(e.what()));
    }
}

void OKXClient::parseOrderBookMessage(const nlohmann::json& data) {
    try {
        OrderBook order_book;
        order_book.symbol = data["instId"];
        order_book.exchange = Exchange::OKX;
        order_book.timestamp = std::chrono::system_clock::now();
        
        // Parse bids
        if (data.contains("bids")) {
            for (const auto& bid_array : data["bids"]) {
                if (bid_array.size() >= 2) {
                    double price = std::stod(bid_array[0].get<std::string>());
                    double quantity = std::stod(bid_array[1].get<std::string>());
                    order_book.bids.emplace_back(price, quantity);
                }
            }
        }
        
        // Parse asks
        if (data.contains("asks")) {
            for (const auto& ask_array : data["asks"]) {
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
        LOG_ERROR("Error parsing OKX order book message: " + std::string(e.what()));
    }
}

void OKXClient::parseTradeMessage(const nlohmann::json& data) {
    try {
        Trade trade;
        trade.symbol = data["instId"];
        trade.exchange = Exchange::OKX;
        trade.timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(std::stoll(data["ts"].get<std::string>()))
        );
        trade.price = std::stod(data["px"].get<std::string>());
        trade.quantity = std::stod(data["sz"].get<std::string>());
        trade.is_buyer_maker = (data["side"].get<std::string>() == "sell");
        
        // Call parent class callback
        onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX trade message: " + std::string(e.what()));
    }
}

void OKXClient::runEventLoop() {
    LOG_INFO("Starting OKX WebSocket event loop");
    
    try {
        ws_client_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in OKX event loop: " + std::string(e.what()));
    }
    
    LOG_INFO("OKX WebSocket event loop stopped");
}

std::string OKXClient::formatSymbolForOKX(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "BTC-USDT" - OKX uses same format)
    std::string result = symbol;
    
    // Convert to uppercase for OKX
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    
    return result;
}

} // namespace data
} // namespace arbitrage
