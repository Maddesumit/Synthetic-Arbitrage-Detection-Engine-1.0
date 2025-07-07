#pragma once

#include "MarketData.hpp"
#include "BinanceClient.hpp"
#include "OKXClient.hpp"
#include "BybitClient.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace arbitrage {
namespace data {

/**
 * @brief Real-time data manager that coordinates all exchange connections
 */
class RealTimeDataManager {
public:
    /**
     * @brief Data update callbacks
     */
    using MarketDataCallback = std::function<void(const MarketDataPoint&)>;
    using OrderBookCallback = std::function<void(const OrderBook&)>;
    using TradeCallback = std::function<void(const Trade&)>;
    
    /**
     * @brief Constructor
     */
    RealTimeDataManager();
    
    /**
     * @brief Destructor
     */
    ~RealTimeDataManager();
    
    /**
     * @brief Initialize all exchange connections
     */
    bool initialize();
    
    /**
     * @brief Start real-time data collection
     */
    bool start();
    
    /**
     * @brief Stop data collection
     */
    void stop();
    
    /**
     * @brief Subscribe to ticker data for a symbol on all exchanges
     */
    bool subscribeToSymbol(const std::string& symbol);
    
    /**
     * @brief Unsubscribe from ticker data for a symbol
     */
    bool unsubscribeFromSymbol(const std::string& symbol);
    
    /**
     * @brief Subscribe to order book data for a symbol on specific exchange
     */
    bool subscribeToOrderBook(const std::string& symbol, Exchange exchange);
    
    /**
     * @brief Get latest market data for a symbol and exchange
     */
    std::optional<MarketDataPoint> getLatestData(const std::string& symbol, Exchange exchange) const;
    
    /**
     * @brief Get all latest market data
     */
    std::vector<MarketDataPoint> getAllLatestData() const;
    
    /**
     * @brief Set callback for market data updates
     */
    void setMarketDataCallback(MarketDataCallback callback);
    
    /**
     * @brief Set callback for order book updates
     */
    void setOrderBookCallback(OrderBookCallback callback);
    
    /**
     * @brief Set callback for trade updates
     */
    void setTradeCallback(TradeCallback callback);
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Get connection status for all exchanges
     */
    std::map<Exchange, ConnectionStatus> getConnectionStatus() const;

private:
    // Exchange clients
    std::unique_ptr<BinanceClient> binance_client_;
    std::unique_ptr<OKXClient> okx_client_;
    std::unique_ptr<BybitClient> bybit_client_;
    
    // Data storage
    mutable std::mutex data_mutex_;
    std::unordered_map<std::string, MarketDataPoint> latest_data_; // key: symbol_exchange
    
    // Callbacks
    MarketDataCallback market_data_callback_;
    OrderBookCallback order_book_callback_;
    TradeCallback trade_callback_;
    
    // Control
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
    
    // Internal methods
    void onTickerUpdate(const Ticker& ticker);
    void onOrderBookUpdate(const OrderBook& order_book);
    void onTradeUpdate(const Trade& trade);
    void monitorConnections();
    std::string makeDataKey(const std::string& symbol, Exchange exchange) const;
    MarketDataPoint convertTickerToMarketData(const Ticker& ticker) const;
};

} // namespace data
} // namespace arbitrage
