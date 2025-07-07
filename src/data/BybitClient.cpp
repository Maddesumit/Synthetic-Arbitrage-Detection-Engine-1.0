#include "BybitClient.hpp"
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

// Bybit WebSocket API endpoint
static const std::string BYBIT_WS_BASE = "wss://stream.bybit.com/v5/public/spot";

using client_type = ::websocketpp::client<::websocketpp::config::asio_tls_client>;
using message_ptr = ::websocketpp::config::asio_tls_client::message_type::ptr;

class BybitClient::Impl {
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
    BybitClient* parent_;  // Pointer back to parent for callbacks
    
    Impl(BybitClient* parent) : parent_(parent) {
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
    
    // Helper methods
    void runEventLoop();
    std::string formatSymbolForBybit(const std::string& symbol) const;
};

BybitClient::BybitClient() : WebSocketClient(Exchange::BYBIT), pimpl_(std::make_unique<Impl>(this)) {
    LOG_INFO("Initializing Bybit WebSocket client for real API connection");
    
    // Disable auto-reconnection to prevent crashes
    setAutoReconnect(false);
    setReconnectParameters(10000, 120000, 2.0);  // Conservative settings if needed
}

BybitClient::~BybitClient() {
    LOG_INFO("Destroying Bybit WebSocket client");
    disconnect();
}

bool BybitClient::connect() {
    std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
    
    if (pimpl_->is_connected_ || pimpl_->is_connecting_) {
        LOG_WARN("Bybit client already connected or connecting");
        return pimpl_->is_connected_;
    }
    
    LOG_INFO("Connecting to Bybit WebSocket API");
    
    try {
        pimpl_->is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to Bybit endpoint
        ::websocketpp::lib::error_code ec;
        std::string uri = BYBIT_WS_BASE;
        
        pimpl_->connection_ = pimpl_->ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create Bybit connection: " + ec.message());
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
        pimpl_->event_loop_thread_ = std::thread(&BybitClient::Impl::runEventLoop, pimpl_.get());
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        pimpl_->is_connecting_ = false;
        
        return pimpl_->is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during Bybit connection: " + std::string(e.what()));
        pimpl_->is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void BybitClient::disconnect() {
    LOG_INFO("Disconnecting from Bybit WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
        pimpl_->is_connected_ = false;
        updateConnectionStatus(ConnectionStatus::DISCONNECTED);
    }
    
    // Stop event loop
    pimpl_->event_loop_running_ = false;
    
    // Close WebSocket connection
    if (pimpl_->connection_) {
        websocketpp::lib::error_code ec;
        pimpl_->ws_client_->close(pimpl_->connection_, websocketpp::close::status::normal, "Closing connection", ec);
        if (ec) {
            LOG_WARN("Error closing Bybit connection: " + ec.message());
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

bool BybitClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = pimpl_->formatSymbolForBybit(symbol);
    std::string topic = "tickers." + bybit_symbol;
    
    if (pimpl_->active_subscriptions_.find(topic) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = pimpl_->formatSymbolForBybit(symbol);
    std::string topic = "orderbook.1." + bybit_symbol;
    
    if (pimpl_->active_subscriptions_.find(topic) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to Bybit");
        return false;
    }
    
    std::string bybit_symbol = pimpl_->formatSymbolForBybit(symbol);
    std::string topic = "publicTrade." + bybit_symbol;
    
    if (pimpl_->active_subscriptions_.find(topic) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(topic);
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
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string bybit_symbol = pimpl_->formatSymbolForBybit(symbol);
    std::string topic = "orderbook.1." + bybit_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        pimpl_->active_subscriptions_.erase(topic);
        return true;
    }
    
    return false;
}

bool BybitClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string bybit_symbol = pimpl_->formatSymbolForBybit(symbol);
    std::string topic = "publicTrade." + bybit_symbol;
    
    nlohmann::json unsub_msg = {
        {"op", "unsubscribe"},
        {"args", {topic}}
    };
    
    if (sendSubscriptionMessage(unsub_msg.dump())) {
        pimpl_->active_subscriptions_.erase(topic);
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
                pimpl_->parseTickerMessage(data_content);
            } else if (topic.find("orderbook.") == 0) {
                pimpl_->parseOrderBookMessage(data_content);
            } else if (topic.find("publicTrade.") == 0) {
                for (const auto& trade_data : data_content) {
                    pimpl_->parseTradeMessage(trade_data);
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit message: " + std::string(e.what()));
    }
}

bool BybitClient::sendSubscriptionMessage(const std::string& message) {
    if (!pimpl_->is_connected_ || !pimpl_->connection_) {
        LOG_WARN("Cannot send message: not connected to Bybit");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        pimpl_->ws_client_->send(pimpl_->connection_, message, websocketpp::frame::opcode::text, ec);
        
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

void BybitClient::Impl::onOpen(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to Bybit WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void BybitClient::Impl::onClose(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from Bybit WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void BybitClient::Impl::onMessage(::websocketpp::connection_hdl hdl, message_ptr msg) {
    parent_->parseMessage(msg->get_payload());
}

void BybitClient::Impl::onFail(::websocketpp::connection_hdl hdl) {
    LOG_ERROR("Bybit WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::ERROR);
}

void BybitClient::Impl::parseTickerMessage(const nlohmann::json& data) {
    try {
        // Debug: log the actual message structure (only for first few messages)
        static int debug_count = 0;
        if (debug_count < 3) {
            LOG_DEBUG("Bybit ticker message received: " + data.dump());
            debug_count++;
        }
        
        // Bybit V5 ticker format: data might be nested under "data" array
        nlohmann::json ticker_data = data;
        if (data.contains("data") && data["data"].is_array() && !data["data"].empty()) {
            ticker_data = data["data"][0];
        }
        
        // Check for Bybit V5 ticker format fields
        if (!ticker_data.contains("symbol")) {
            LOG_WARN("Bybit ticker message missing symbol field");
            return;
        }
        
        Ticker ticker;
        ticker.symbol = ticker_data["symbol"];
        ticker.exchange = Exchange::BYBIT;
        ticker.timestamp = std::chrono::system_clock::now();
        
        // Bybit V5 ticker data format - use available fields
        if (ticker_data.contains("bid1Price") && !ticker_data["bid1Price"].is_null()) {
            ticker.bid = std::stod(ticker_data["bid1Price"].get<std::string>());
        }
        if (ticker_data.contains("ask1Price") && !ticker_data["ask1Price"].is_null()) {
            ticker.ask = std::stod(ticker_data["ask1Price"].get<std::string>());
        }
        if (ticker_data.contains("lastPrice") && !ticker_data["lastPrice"].is_null()) {
            ticker.last = std::stod(ticker_data["lastPrice"].get<std::string>());
        }
        if (ticker_data.contains("volume24h") && !ticker_data["volume24h"].is_null()) {
            ticker.volume = std::stod(ticker_data["volume24h"].get<std::string>());
        }
        
        // Optional fields
        if (ticker_data.contains("price24hPcnt") && !ticker_data["price24hPcnt"].is_null()) {
            ticker.price_change_24h = std::stod(ticker_data["price24hPcnt"].get<std::string>());
        }
        
        // Only process if we have essential data
        if (ticker.bid > 0 || ticker.ask > 0 || ticker.last > 0) {
            // Call parent class callback
            parent_->onTickerReceived(ticker);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit ticker message: " + std::string(e.what()));
        LOG_DEBUG("Problematic data: " + data.dump());
    }
}

void BybitClient::Impl::parseOrderBookMessage(const nlohmann::json& data) {
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
        parent_->onOrderBookReceived(order_book);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit order book message: " + std::string(e.what()));
    }
}

void BybitClient::Impl::parseTradeMessage(const nlohmann::json& data) {
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
        parent_->onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing Bybit trade message: " + std::string(e.what()));
    }
}

void BybitClient::Impl::runEventLoop() {
    LOG_INFO("Starting Bybit WebSocket event loop");
    
    try {
        ws_client_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in Bybit event loop: " + std::string(e.what()));
    }
    
    LOG_INFO("Bybit WebSocket event loop stopped");
}

std::string BybitClient::Impl::formatSymbolForBybit(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "BTCUSDT" for Bybit)
    std::string result = symbol;
    
    // Remove dashes and convert to uppercase
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    
    return result;
}

} // namespace data
} // namespace arbitrage
