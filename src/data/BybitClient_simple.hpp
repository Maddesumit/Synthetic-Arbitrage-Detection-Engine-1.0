#pragma once

#include "WebSocketClient.hpp"
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

protected:
    void parseMessage(const std::string& message) override;
    bool sendSubscriptionMessage(const std::string& message) override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace data
} // namespace arbitrage
