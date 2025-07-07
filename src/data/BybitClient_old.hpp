#pragma once

#include "WebSocketClient.hpp"
#include <thread>
#include <mutex>
#include <set>
#include <memory>

namespace arbitrage {
namespace data {

/**
 * @brief Bybit WebSocket client implementation - Real connection to Bybit API
 */
class BybitClient : public WebSocketClient {
public:
    BybitClient();
    ~BybitClient() override;
    
    bool connect() override;
    void disconnect() override;
    
    bool subscribeOrderBook(const std::string& symbol) override;
    bool subscribeTrades(const std::string& symbol) override;
    bool subscribeTicker(const std::string& symbol) override;
    bool subscribeFundingRate(const std::string& symbol) override;
    bool subscribeMarkPrice(const std::string& symbol) override;
    bool unsubscribeOrderBook(const std::string& symbol) override;
    bool unsubscribeTrades(const std::string& symbol) override;

private:
    // WebSocket++ client types
    using client_type = websocketpp::client<websocketpp::config::asio_tls_client>;
    using connection_ptr = client_type::connection_ptr;
    using message_ptr = websocketpp::config::asio_tls_client::message_type::ptr;
    
    void parseMessage(const std::string& message) override;
    bool sendSubscriptionMessage(const std::string& message) override;
    
    // WebSocket event handlers
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);
    
    // Message parsers
    void parseTickerMessage(const nlohmann::json& data);
    void parseOrderBookMessage(const nlohmann::json& data);
    void parseTradeMessage(const nlohmann::json& data);
    
    // Utility methods
    void runEventLoop();
    std::string formatSymbolForBybit(const std::string& symbol) const;
    
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
    
    // Bybit API endpoint
    static constexpr const char* BYBIT_WS_BASE = "wss://stream.bybit.com/v5/public/spot";
};

} // namespace data
} // namespace arbitrage
