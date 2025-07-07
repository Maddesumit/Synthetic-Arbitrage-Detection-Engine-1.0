#pragma once

#include "WebSocketClient.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <set>

namespace arbitrage {
namespace data {

/**
 * @brief Binance WebSocket client implementation - Real connection to Binance API
 */
class BinanceClient : public WebSocketClient {
public:
    /**
     * @brief Constructor
     */
    BinanceClient();
    
    /**
     * @brief Destructor
     */
    ~BinanceClient() override;
    
    /**
     * @brief Connect to Binance WebSocket API
     */
    bool connect() override;
    
    /**
     * @brief Disconnect from Binance WebSocket API
     */
    void disconnect() override;
    
    /**
     * @brief Subscribe to order book updates
     */
    bool subscribeOrderBook(const std::string& symbol) override;
    
    /**
     * @brief Subscribe to trade updates
     */
    bool subscribeTrades(const std::string& symbol) override;
    
    /**
     * @brief Subscribe to ticker updates
     */
    bool subscribeTicker(const std::string& symbol) override;
    
    /**
     * @brief Subscribe to funding rate updates
     */
    bool subscribeFundingRate(const std::string& symbol) override;
    
    /**
     * @brief Subscribe to mark price updates  
     */
    bool subscribeMarkPrice(const std::string& symbol) override;
    
    /**
     * @brief Unsubscribe from order book updates
     */
    bool unsubscribeOrderBook(const std::string& symbol) override;
    
    /**
     * @brief Unsubscribe from trade updates
     */
    bool unsubscribeTrades(const std::string& symbol) override;

private:
    // WebSocket++ client types
    using client_type = websocketpp::client<websocketpp::config::asio_tls_client>;
    using connection_ptr = client_type::connection_ptr;
    using message_ptr = websocketpp::config::asio_tls_client::message_type::ptr;
    
    /**
     * @brief Parse incoming WebSocket message from Binance
     */
    void parseMessage(const std::string& message) override;
    
    /**
     * @brief Send subscription message to Binance
     */
    bool sendSubscriptionMessage(const std::string& message) override;
    
    /**
     * @brief WebSocket event handlers
     */
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);
    
    /**
     * @brief Message parsers for different data types
     */
    void parseTickerMessage(const nlohmann::json& data);
    void parseOrderBookMessage(const nlohmann::json& data);
    void parseTradeMessage(const nlohmann::json& data);
    void parseFundingRateMessage(const nlohmann::json& data);
    void parseMarkPriceMessage(const nlohmann::json& data);
    
    /**
     * @brief Connection management
     */
    void runEventLoop();
    bool reconnect();
    std::string formatSymbolForBinance(const std::string& symbol) const;
    
    // WebSocket client and connection
    std::unique_ptr<client_type> ws_client_;
    connection_ptr connection_;
    websocketpp::connection_hdl connection_hdl_;
    
    // Connection state
    std::mutex connection_mutex_;
    bool is_connected_{false};
    bool is_connecting_{false};
    
    // Subscriptions tracking
    std::mutex subscriptions_mutex_;
    std::set<std::string> active_subscriptions_;
    
    // Event loop thread
    std::thread event_loop_thread_;
    std::atomic<bool> event_loop_running_{false};
    
    // Binance API endpoints
    static constexpr const char* BINANCE_WS_BASE = "wss://stream.binance.com:9443/ws/";
    static constexpr const char* BINANCE_FUTURES_WS_BASE = "wss://fstream.binance.com/ws/";
};

} // namespace data
} // namespace arbitrage
