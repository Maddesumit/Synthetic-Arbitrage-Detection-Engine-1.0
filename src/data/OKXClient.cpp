#include "OKXClient.hpp"
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

// OKX WebSocket API endpoint
static const std::string OKX_WS_BASE = "wss://ws.okx.com:8443/ws/v5/public";

using client_type = ::websocketpp::client<::websocketpp::config::asio_tls_client>;
using message_ptr = ::websocketpp::config::asio_tls_client::message_type::ptr;

class OKXClient::Impl {
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
    OKXClient* parent_;  // Pointer back to parent for callbacks
    
    Impl(OKXClient* parent) : parent_(parent) {
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
    std::string formatSymbolForOKX(const std::string& symbol) const;
};

OKXClient::OKXClient() : WebSocketClient(Exchange::OKX), pimpl_(std::make_unique<Impl>(this)) {
    LOG_INFO("Initializing OKX WebSocket client for real API connection");
    
    // Disable auto-reconnection to prevent crashes
    setAutoReconnect(false);
    setReconnectParameters(10000, 120000, 2.0);  // Conservative settings if needed
}

OKXClient::~OKXClient() {
    LOG_INFO("Destroying OKX WebSocket client");
    
    // Ensure clean disconnect
    try {
        disconnect();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during OKX client destruction: {}", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception during OKX client destruction");
    }
    
    // Final cleanup of pimpl resources
    if (pimpl_) {
        try {
            // Make sure event loop thread is stopped
            if (pimpl_->event_loop_thread_.joinable()) {
                pimpl_->event_loop_running_ = false;
                // Give it time to exit gracefully
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (pimpl_->event_loop_thread_.joinable()) {
                    try {
                        pimpl_->event_loop_thread_.join();
                    } catch (...) {
                        LOG_ERROR("Failed to join OKX event loop thread in destructor");
                        pimpl_->event_loop_thread_.detach();
                    }
                }
            }
        } catch (...) {
            LOG_ERROR("Exception during final OKX cleanup");
        }
    }
}

bool OKXClient::connect() {
    std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
    
    if (pimpl_->is_connected_ || pimpl_->is_connecting_) {
        LOG_WARN("OKX client already connected or connecting");
        return pimpl_->is_connected_;
    }
    
    LOG_INFO("Connecting to OKX WebSocket API");
    
    try {
        pimpl_->is_connecting_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        
        // Create WebSocket connection to OKX endpoint
        ::websocketpp::lib::error_code ec;
        std::string uri = OKX_WS_BASE;
        
        pimpl_->connection_ = pimpl_->ws_client_->get_connection(uri, ec);
        if (ec) {
            LOG_ERROR("Failed to create OKX connection: " + ec.message());
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
        if (pimpl_->event_loop_thread_.joinable()) {
            pimpl_->event_loop_thread_.join(); // Clean up previous thread
        }
        
        pimpl_->event_loop_running_ = true;
        pimpl_->event_loop_thread_ = std::thread(&OKXClient::Impl::runEventLoop, pimpl_.get());
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        pimpl_->is_connecting_ = false;
        
        return pimpl_->is_connected_;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during OKX connection: " + std::string(e.what()));
        pimpl_->is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception during OKX connection");
        pimpl_->is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void OKXClient::disconnect() {
    LOG_INFO("Disconnecting from OKX WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(pimpl_->connection_mutex_);
        pimpl_->is_connected_ = false;
        pimpl_->is_connecting_ = false;
        updateConnectionStatus(ConnectionStatus::DISCONNECTED);
    }
    
    // Stop event loop
    pimpl_->event_loop_running_ = false;
    
    // Close WebSocket connection
    if (pimpl_->connection_) {
        try {
            websocketpp::lib::error_code ec;
            pimpl_->ws_client_->close(pimpl_->connection_, websocketpp::close::status::normal, "Closing connection", ec);
            if (ec) {
                LOG_WARN("Error closing OKX connection: " + ec.message());
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception closing OKX connection: {}", e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception closing OKX connection");
        }
    }
    
    // Stop the WebSocket client to break out of event loop
    try {
        pimpl_->ws_client_->stop();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception stopping OKX WebSocket client: {}", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception stopping OKX WebSocket client");
    }
    
    // Wait for event loop thread to finish
    if (pimpl_->event_loop_thread_.joinable()) {
        try {
            // Wait up to 2 seconds for thread to finish
            auto start = std::chrono::steady_clock::now();
            while (pimpl_->event_loop_thread_.joinable() && 
                   std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            if (pimpl_->event_loop_thread_.joinable()) {
                pimpl_->event_loop_thread_.join();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception joining OKX event loop thread: {}", e.what());
            // Detach to prevent blocking
            try {
                pimpl_->event_loop_thread_.detach();
            } catch (...) {
                LOG_ERROR("Failed to detach OKX event loop thread");
            }
        }
    }
    
    // Clear subscriptions
    {
        std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
        pimpl_->active_subscriptions_.clear();
    }
    
    // Reset connection
    pimpl_->connection_.reset();
}

bool OKXClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to ticker: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = pimpl_->formatSymbolForOKX(symbol);
    std::string channel = "tickers";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (pimpl_->active_subscriptions_.find(subscription_key) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to order book: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = pimpl_->formatSymbolForOKX(symbol);
    std::string channel = "books";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (pimpl_->active_subscriptions_.find(subscription_key) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    if (!pimpl_->is_connected_) {
        LOG_WARN("Cannot subscribe to trades: not connected to OKX");
        return false;
    }
    
    std::string okx_symbol = pimpl_->formatSymbolForOKX(symbol);
    std::string channel = "trades";
    std::string subscription_key = channel + ":" + okx_symbol;
    
    if (pimpl_->active_subscriptions_.find(subscription_key) != pimpl_->active_subscriptions_.end()) {
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
        pimpl_->active_subscriptions_.insert(subscription_key);
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
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string okx_symbol = pimpl_->formatSymbolForOKX(symbol);
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
        pimpl_->active_subscriptions_.erase(subscription_key);
        return true;
    }
    
    return false;
}

bool OKXClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(pimpl_->subscriptions_mutex_);
    
    std::string okx_symbol = pimpl_->formatSymbolForOKX(symbol);
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
        pimpl_->active_subscriptions_.erase(subscription_key);
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
                        pimpl_->parseTickerMessage(ticker_data);
                    }
                } else if (channel == "books") {
                    for (const auto& book_data : data_array) {
                        pimpl_->parseOrderBookMessage(book_data);
                    }
                } else if (channel == "trades") {
                    for (const auto& trade_data : data_array) {
                        pimpl_->parseTradeMessage(trade_data);
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX message: " + std::string(e.what()));
    }
}

bool OKXClient::sendSubscriptionMessage(const std::string& message) {
    if (!pimpl_->is_connected_ || !pimpl_->connection_) {
        LOG_WARN("Cannot send message: not connected to OKX");
        return false;
    }
    
    try {
        websocketpp::lib::error_code ec;
        pimpl_->ws_client_->send(pimpl_->connection_, message, websocketpp::frame::opcode::text, ec);
        
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

void OKXClient::Impl::onOpen(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Connected to OKX WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = true;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::CONNECTED);
}

void OKXClient::Impl::onClose(::websocketpp::connection_hdl hdl) {
    LOG_INFO("Disconnected from OKX WebSocket API");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::DISCONNECTED);
}

void OKXClient::Impl::onMessage(::websocketpp::connection_hdl hdl, message_ptr msg) {
    parent_->parseMessage(msg->get_payload());
}

void OKXClient::Impl::onFail(::websocketpp::connection_hdl hdl) {
    LOG_ERROR("OKX WebSocket connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        is_connected_ = false;
    }
    
    parent_->updateConnectionStatus(ConnectionStatus::ERROR);
}

void OKXClient::Impl::parseTickerMessage(const nlohmann::json& data) {
    try {
        // Check if required fields exist
        if (!data.contains("instId") || !data.contains("bidPx") || !data.contains("askPx") || 
            !data.contains("last") || !data.contains("vol24h")) {
            LOG_WARN("OKX ticker message missing required fields");
            return;
        }
        
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
        parent_->onTickerReceived(ticker);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX ticker message: " + std::string(e.what()));
        LOG_DEBUG("Problematic data: " + data.dump());
    }
}

void OKXClient::Impl::parseOrderBookMessage(const nlohmann::json& data) {
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
        parent_->onOrderBookReceived(order_book);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX order book message: " + std::string(e.what()));
    }
}

void OKXClient::Impl::parseTradeMessage(const nlohmann::json& data) {
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
        parent_->onTradeReceived(trade);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing OKX trade message: " + std::string(e.what()));
    }
}

void OKXClient::Impl::runEventLoop() {
    LOG_INFO("Starting OKX WebSocket event loop");
    event_loop_running_ = true;
    
    try {
        while (event_loop_running_) {
            try {
                ws_client_->run();
                break; // Normal exit
            } catch (const websocketpp::exception& e) {
                LOG_ERROR("WebSocket++ exception in OKX event loop: {}", e.what());
                // Break on websocket-specific errors to avoid infinite loop
                break;
            } catch (const std::exception& e) {
                LOG_ERROR("Exception in OKX event loop: {}", e.what());
                if (!event_loop_running_) break;
                // Small delay before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (...) {
        LOG_ERROR("Unknown exception in OKX event loop");
    }
    
    event_loop_running_ = false;
    LOG_INFO("OKX WebSocket event loop stopped");
}

std::string OKXClient::Impl::formatSymbolForOKX(const std::string& symbol) const {
    // Convert symbol format (e.g., "BTC-USDT" to "BTC-USDT" - OKX uses same format)
    std::string result = symbol;
    
    // Convert to uppercase for OKX
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    
    return result;
}

} // namespace data
} // namespace arbitrage
