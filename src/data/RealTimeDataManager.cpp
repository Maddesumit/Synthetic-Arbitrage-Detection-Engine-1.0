#include "RealTimeDataManager.hpp"
#include "utils/Logger.hpp"
#include <algorithm>

namespace arbitrage {
namespace data {

RealTimeDataManager::RealTimeDataManager() {
    LOG_INFO("Initializing Real-Time Data Manager");
    
    // Create exchange clients
    binance_client_ = std::make_unique<BinanceClient>();
    okx_client_ = std::make_unique<OKXClient>();
    bybit_client_ = std::make_unique<BybitClient>();
    
    // Set up callbacks for each client
    binance_client_->setTickerCallback([this](const Ticker& ticker) {
        onTickerUpdate(ticker);
    });
    
    binance_client_->setOrderBookCallback([this](const OrderBook& order_book) {
        onOrderBookUpdate(order_book);
    });
    
    binance_client_->setTradeCallback([this](const Trade& trade) {
        onTradeUpdate(trade);
    });
    
    okx_client_->setTickerCallback([this](const Ticker& ticker) {
        onTickerUpdate(ticker);
    });
    
    okx_client_->setOrderBookCallback([this](const OrderBook& order_book) {
        onOrderBookUpdate(order_book);
    });
    
    okx_client_->setTradeCallback([this](const Trade& trade) {
        onTradeUpdate(trade);
    });
    
    bybit_client_->setTickerCallback([this](const Ticker& ticker) {
        onTickerUpdate(ticker);
    });
    
    bybit_client_->setOrderBookCallback([this](const OrderBook& order_book) {
        onOrderBookUpdate(order_book);
    });
    
    bybit_client_->setTradeCallback([this](const Trade& trade) {
        onTradeUpdate(trade);
    });
}

RealTimeDataManager::~RealTimeDataManager() {
    LOG_INFO("Destroying Real-Time Data Manager");
    stop();
}

bool RealTimeDataManager::initialize() {
    LOG_INFO("Initializing exchange connections");
    
    bool all_connected = true;
    
    // Connect to Binance
    if (!binance_client_->connect()) {
        LOG_ERROR("Failed to connect to Binance");
        all_connected = false;
    }
    
    // Connect to OKX
    if (!okx_client_->connect()) {
        LOG_ERROR("Failed to connect to OKX");
        all_connected = false;
    }
    
    // Connect to Bybit
    if (!bybit_client_->connect()) {
        LOG_ERROR("Failed to connect to Bybit");
        all_connected = false;
    }
    
    if (all_connected) {
        LOG_INFO("All exchange connections established successfully");
    } else {
        LOG_WARN("Some exchange connections failed");
    }
    
    return all_connected;
}

bool RealTimeDataManager::start() {
    if (running_.load()) {
        LOG_WARN("Real-Time Data Manager already running");
        return true;
    }
    
    LOG_INFO("Starting Real-Time Data Manager");
    
    running_ = true;
    
    // Start connection monitoring thread
    monitor_thread_ = std::thread(&RealTimeDataManager::monitorConnections, this);
    
    return true;
}

void RealTimeDataManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO("Stopping Real-Time Data Manager");
    
    running_ = false;
    
    // Stop monitoring thread
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    // Disconnect all clients
    if (binance_client_) {
        binance_client_->disconnect();
    }
    
    if (okx_client_) {
        okx_client_->disconnect();
    }
    
    if (bybit_client_) {
        bybit_client_->disconnect();
    }
    
    LOG_INFO("Real-Time Data Manager stopped");
}

bool RealTimeDataManager::subscribeToSymbol(const std::string& symbol) {
    LOG_INFO("Subscribing to symbol: " + symbol + " on all exchanges");
    
    bool all_success = true;
    
    // Subscribe on Binance
    if (binance_client_ && binance_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
        if (!binance_client_->subscribeTicker(symbol)) {
            LOG_WARN("Failed to subscribe to " + symbol + " on Binance");
            all_success = false;
        }
    }
    
    // Subscribe on OKX
    if (okx_client_ && okx_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
        if (!okx_client_->subscribeTicker(symbol)) {
            LOG_WARN("Failed to subscribe to " + symbol + " on OKX");
            all_success = false;
        }
    }
    
    // Subscribe on Bybit
    if (bybit_client_ && bybit_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
        if (!bybit_client_->subscribeTicker(symbol)) {
            LOG_WARN("Failed to subscribe to " + symbol + " on Bybit");
            all_success = false;
        }
    }
    
    return all_success;
}

bool RealTimeDataManager::unsubscribeFromSymbol(const std::string& symbol) {
    LOG_INFO("Unsubscribing from symbol: " + symbol + " on all exchanges");
    
    bool all_success = true;
    
    // Note: Unsubscription methods would need to be implemented for ticker streams
    // For now, we'll just log the action
    
    return all_success;
}

bool RealTimeDataManager::subscribeToOrderBook(const std::string& symbol, Exchange exchange) {
    LOG_INFO("Subscribing to order book for " + symbol + " on " + exchangeToString(exchange));
    
    switch (exchange) {
        case Exchange::BINANCE:
            if (binance_client_ && binance_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
                return binance_client_->subscribeOrderBook(symbol);
            }
            break;
        case Exchange::OKX:
            if (okx_client_ && okx_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
                return okx_client_->subscribeOrderBook(symbol);
            }
            break;
        case Exchange::BYBIT:
            if (bybit_client_ && bybit_client_->getConnectionStatus() == ConnectionStatus::CONNECTED) {
                return bybit_client_->subscribeOrderBook(symbol);
            }
            break;
    }
    
    return false;
}

std::optional<MarketDataPoint> RealTimeDataManager::getLatestData(const std::string& symbol, Exchange exchange) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::string key = makeDataKey(symbol, exchange);
    auto it = latest_data_.find(key);
    
    if (it != latest_data_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<MarketDataPoint> RealTimeDataManager::getAllLatestData() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<MarketDataPoint> data;
    data.reserve(latest_data_.size());
    
    for (const auto& [key, market_data] : latest_data_) {
        data.push_back(market_data);
    }
    
    return data;
}

void RealTimeDataManager::setMarketDataCallback(MarketDataCallback callback) {
    market_data_callback_ = std::move(callback);
}

void RealTimeDataManager::setOrderBookCallback(OrderBookCallback callback) {
    order_book_callback_ = std::move(callback);
}

void RealTimeDataManager::setTradeCallback(TradeCallback callback) {
    trade_callback_ = std::move(callback);
}

std::map<Exchange, ConnectionStatus> RealTimeDataManager::getConnectionStatus() const {
    std::map<Exchange, ConnectionStatus> status;
    
    if (binance_client_) {
        status[Exchange::BINANCE] = binance_client_->getConnectionStatus();
    }
    
    if (okx_client_) {
        status[Exchange::OKX] = okx_client_->getConnectionStatus();
    }
    
    if (bybit_client_) {
        status[Exchange::BYBIT] = bybit_client_->getConnectionStatus();
    }
    
    return status;
}

void RealTimeDataManager::onTickerUpdate(const Ticker& ticker) {
    // Convert ticker to MarketDataPoint
    MarketDataPoint market_data = convertTickerToMarketData(ticker);
    
    // Store latest data
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::string key = makeDataKey(ticker.symbol, ticker.exchange);
        latest_data_[key] = market_data;
    }
    
    // Call callback if set
    if (market_data_callback_) {
        market_data_callback_(market_data);
    }
}

void RealTimeDataManager::onOrderBookUpdate(const OrderBook& order_book) {
    // Call callback if set
    if (order_book_callback_) {
        order_book_callback_(order_book);
    }
}

void RealTimeDataManager::onTradeUpdate(const Trade& trade) {
    // Call callback if set
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

void RealTimeDataManager::monitorConnections() {
    LOG_INFO("Starting connection monitoring");
    
    while (running_.load()) {
        // Check connection status and attempt reconnection if needed
        auto status = getConnectionStatus();
        
        for (const auto& [exchange, conn_status] : status) {
            if (conn_status == ConnectionStatus::DISCONNECTED || conn_status == ConnectionStatus::ERROR) {
                LOG_WARN("Exchange " + exchangeToString(exchange) + " disconnected, attempting reconnection");
                
                // Attempt reconnection
                switch (exchange) {
                    case Exchange::BINANCE:
                        if (binance_client_) {
                            binance_client_->connect();
                        }
                        break;
                    case Exchange::OKX:
                        if (okx_client_) {
                            okx_client_->connect();
                        }
                        break;
                    case Exchange::BYBIT:
                        if (bybit_client_) {
                            bybit_client_->connect();
                        }
                        break;
                }
            }
        }
        
        // Sleep for 30 seconds before next check
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
    
    LOG_INFO("Connection monitoring stopped");
}

std::string RealTimeDataManager::makeDataKey(const std::string& symbol, Exchange exchange) const {
    return symbol + "_" + exchangeToString(exchange);
}

MarketDataPoint RealTimeDataManager::convertTickerToMarketData(const Ticker& ticker) const {
    MarketDataPoint market_data;
    
    market_data.symbol = ticker.symbol;
    market_data.exchange = exchangeToString(ticker.exchange);
    market_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        ticker.timestamp.time_since_epoch()
    );
    market_data.bid = ticker.bid;
    market_data.ask = ticker.ask;
    market_data.last = ticker.last;
    market_data.volume = ticker.volume;
    market_data.funding_rate = 0.0; // Not provided in ticker, would need separate subscription
    
    return market_data;
}

} // namespace data
} // namespace arbitrage
