#pragma once

#include "MarketData.hpp"
#include "utils/Logger.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace arbitrage {
namespace data {

/**
 * @brief Connection status
 */
enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

/**
 * @brief Data callback functions
 */
using OrderBookCallback = std::function<void(const OrderBook&)>;
using TradeCallback = std::function<void(const Trade&)>;
using TickerCallback = std::function<void(const Ticker&)>;
using FundingRateCallback = std::function<void(const FundingRate&)>;
using MarkPriceCallback = std::function<void(const MarkPrice&)>;

/**
 * @brief Connection event callbacks
 */
using ConnectionCallback = std::function<void(ConnectionStatus)>;
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief Abstract base class for WebSocket clients
 */
class WebSocketClient {
public:
    /**
     * @brief Constructor
     * @param exchange Exchange identifier
     */
    explicit WebSocketClient(Exchange exchange);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~WebSocketClient();
    
    /**
     * @brief Connect to the WebSocket
     * @return true if connection initiated successfully
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Disconnect from the WebSocket
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Subscribe to order book updates
     * @param symbol Trading symbol (e.g., "BTCUSDT")
     * @return true if subscription successful
     */
    virtual bool subscribeOrderBook(const std::string& symbol) = 0;
    
    /**
     * @brief Subscribe to trade updates
     * @param symbol Trading symbol
     * @return true if subscription successful
     */
    virtual bool subscribeTrades(const std::string& symbol) = 0;
    
    /**
     * @brief Subscribe to ticker updates
     * @param symbol Trading symbol
     * @return true if subscription successful
     */
    virtual bool subscribeTicker(const std::string& symbol) = 0;
    
    /**
     * @brief Subscribe to funding rate updates (for perpetual contracts)
     * @param symbol Trading symbol
     * @return true if subscription successful
     */
    virtual bool subscribeFundingRate(const std::string& symbol) = 0;
    
    /**
     * @brief Subscribe to mark price updates
     * @param symbol Trading symbol
     * @return true if subscription successful
     */
    virtual bool subscribeMarkPrice(const std::string& symbol) = 0;
    
    /**
     * @brief Unsubscribe from order book updates
     * @param symbol Trading symbol
     * @return true if unsubscription successful
     */
    virtual bool unsubscribeOrderBook(const std::string& symbol) = 0;
    
    /**
     * @brief Unsubscribe from trade updates
     * @param symbol Trading symbol
     * @return true if unsubscription successful
     */
    virtual bool unsubscribeTrades(const std::string& symbol) = 0;
    
    /**
     * @brief Get current connection status
     */
    ConnectionStatus getConnectionStatus() const { return connection_status_; }
    
    /**
     * @brief Get exchange identifier
     */
    Exchange getExchange() const { return exchange_; }
    
    /**
     * @brief Check if client is connected
     */
    bool isConnected() const { return connection_status_ == ConnectionStatus::CONNECTED; }
    
    /**
     * @brief Set callback functions
     */
    void setOrderBookCallback(OrderBookCallback callback) { orderbook_callback_ = callback; }
    void setTradeCallback(TradeCallback callback) { trade_callback_ = callback; }
    void setTickerCallback(TickerCallback callback) { ticker_callback_ = callback; }
    void setFundingRateCallback(FundingRateCallback callback) { funding_rate_callback_ = callback; }
    void setMarkPriceCallback(MarkPriceCallback callback) { mark_price_callback_ = callback; }
    void setConnectionCallback(ConnectionCallback callback) { connection_callback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }
    
    /**
     * @brief Enable/disable automatic reconnection
     */
    void setAutoReconnect(bool enable) { auto_reconnect_ = enable; }
    
    /**
     * @brief Set reconnection parameters
     * @param initial_delay Initial delay in milliseconds
     * @param max_delay Maximum delay in milliseconds
     * @param backoff_multiplier Backoff multiplier
     */
    void setReconnectParameters(int initial_delay, int max_delay, double backoff_multiplier);

protected:
    /**
     * @brief Update connection status and notify callback
     */
    void updateConnectionStatus(ConnectionStatus status);
    
    /**
     * @brief Handle error and notify callback
     */
    void handleError(const std::string& error_message);
    
    /**
     * @brief Trigger callbacks for received data
     */
    void onOrderBookReceived(const OrderBook& orderbook);
    void onTradeReceived(const Trade& trade);
    void onTickerReceived(const Ticker& ticker);
    void onFundingRateReceived(const FundingRate& funding_rate);
    void onMarkPriceReceived(const MarkPrice& mark_price);
    
    /**
     * @brief Start reconnection process
     */
    void startReconnection();
    
    /**
     * @brief Reconnection loop (runs in separate thread)
     */
    void reconnectionLoop();
    
    /**
     * @brief Exchange-specific message parsing (to be implemented by derived classes)
     */
    virtual void parseMessage(const std::string& message) = 0;
    
    /**
     * @brief Send subscription message (to be implemented by derived classes)
     */
    virtual bool sendSubscriptionMessage(const std::string& message) = 0;

private:
    Exchange exchange_;
    std::atomic<ConnectionStatus> connection_status_{ConnectionStatus::DISCONNECTED};
    
    // Callback functions
    OrderBookCallback orderbook_callback_;
    TradeCallback trade_callback_;
    TickerCallback ticker_callback_;
    FundingRateCallback funding_rate_callback_;
    MarkPriceCallback mark_price_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
    
    // Reconnection parameters
    bool auto_reconnect_{true};
    int reconnect_initial_delay_{1000};  // 1 second
    int reconnect_max_delay_{30000};     // 30 seconds
    double reconnect_backoff_multiplier_{2.0};
    int max_reconnect_attempts_{5};      // Maximum reconnection attempts
    
    // Reconnection state
    std::atomic<bool> reconnection_active_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::thread reconnection_thread_;
    std::mutex reconnection_mutex_;
    int current_reconnect_delay_;
    std::atomic<int> reconnect_attempts_{0};
};

/**
 * @brief Factory function for creating WebSocket clients
 */
std::unique_ptr<WebSocketClient> createWebSocketClient(Exchange exchange);

} // namespace data
} // namespace arbitrage
