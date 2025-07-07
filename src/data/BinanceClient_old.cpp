#include "BinanceClient.hpp"
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>

namespace arbitrage {
namespace data {

BinanceClient::BinanceClient() : WebSocketClient(Exchange::BINANCE) {
    LOG_INFO("BinanceClient initialized (simplified for Phase 2 demo)");
}

BinanceClient::~BinanceClient() {
    disconnect();
}

bool BinanceClient::connect() {
    try {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (is_connected_) {
            return true;
        }
        
        updateConnectionStatus(ConnectionStatus::CONNECTING);
        LOG_INFO("Simulating Binance WebSocket connection...");
        
        // Simulate connection delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        is_connected_ = true;
        updateConnectionStatus(ConnectionStatus::CONNECTED);
        
        // Start simulation thread
        simulation_running_ = true;
        simulation_thread_ = std::thread(&BinanceClient::simulateMarketData, this);
        
        LOG_INFO("Binance WebSocket connection established (simulated)");
        return true;
        
    } catch (const std::exception& e) {
        handleError("Connection error: " + std::string(e.what()));
        return false;
    }
}

void BinanceClient::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!is_connected_) {
        return;
    }
    
    try {
        // Stop simulation
        simulation_running_ = false;
        if (simulation_thread_.joinable()) {
            simulation_thread_.join();
        }
        
        is_connected_ = false;
        updateConnectionStatus(ConnectionStatus::DISCONNECTED);
        
        LOG_INFO("Binance WebSocket disconnected (simulated)");
        
    } catch (const std::exception& e) {
        handleError("Disconnect error: " + std::string(e.what()));
    }
}

bool BinanceClient::subscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@depth";
    active_subscriptions_.insert(stream);
    LOG_INFO("Subscribed to orderbook for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::subscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@trade";
    active_subscriptions_.insert(stream);
    LOG_INFO("Subscribed to trades for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::subscribeTicker(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@ticker";
    active_subscriptions_.insert(stream);
    LOG_INFO("Subscribed to ticker for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::subscribeFundingRate(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@fundingRate";
    active_subscriptions_.insert(stream);
    LOG_INFO("Subscribed to funding rate for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::subscribeMarkPrice(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@markPrice";
    active_subscriptions_.insert(stream);
    LOG_INFO("Subscribed to mark price for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::unsubscribeOrderBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@depth";
    active_subscriptions_.erase(stream);
    LOG_INFO("Unsubscribed from orderbook for {} (simulated)", symbol);
    return true;
}

bool BinanceClient::unsubscribeTrades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::string stream = symbol + "@trade";
    active_subscriptions_.erase(stream);
    LOG_INFO("Unsubscribed from trades for {} (simulated)", symbol);
    return true;
}

void BinanceClient::parseMessage(const std::string& message) {
    LOG_DEBUG("Parsing simulated message: {}", message);
    // For Phase 2 demo, this is just a placeholder
}

bool BinanceClient::sendSubscriptionMessage(const std::string& message) {
    LOG_DEBUG("Sending simulated subscription message: {}", message);
    return true;
}

void BinanceClient::simulateMarketData() {
    LOG_INFO("Starting market data simulation for Binance");
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (simulation_running_) {
        try {
            // Check if we have any active subscriptions
            std::set<std::string> current_subscriptions;
            {
                std::lock_guard<std::mutex> lock(subscriptions_mutex_);
                current_subscriptions = active_subscriptions_;
            }
            
            for (const auto& subscription : current_subscriptions) {
                if (!simulation_running_) break;
                
                // Extract symbol from subscription
                std::string symbol = subscription.substr(0, subscription.find('@'));
                
                if (subscription.find("@depth") != std::string::npos) {
                    generateSampleOrderBook(symbol);
                } else if (subscription.find("@trade") != std::string::npos) {
                    generateSampleTrade(symbol);
                } else if (subscription.find("@ticker") != std::string::npos) {
                    generateSampleTicker(symbol);
                }
            }
            
            // Sleep for 100ms between updates
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error in market data simulation: {}", e.what());
        }
    }
    
    LOG_INFO("Market data simulation stopped");
}

void BinanceClient::generateSampleOrderBook(const std::string& symbol) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> price_dist(49900.0, 50100.0);
    static std::uniform_real_distribution<> qty_dist(0.1, 5.0);
    
    OrderBook orderbook;
    orderbook.symbol = symbol;
    orderbook.exchange = Exchange::BINANCE;
    orderbook.timestamp = std::chrono::system_clock::now();
    
    double base_price = price_dist(gen);
    
    // Generate 5 bid levels
    for (int i = 0; i < 5; ++i) {
        double price = base_price - (i + 1) * 0.25;
        double quantity = qty_dist(gen);
        orderbook.bids.emplace_back(price, quantity);
    }
    
    // Generate 5 ask levels
    for (int i = 0; i < 5; ++i) {
        double price = base_price + (i + 1) * 0.25;
        double quantity = qty_dist(gen);
        orderbook.asks.emplace_back(price, quantity);
    }
    
    onOrderBookReceived(orderbook);
}

void BinanceClient::generateSampleTrade(const std::string& symbol) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> price_dist(49950.0, 50050.0);
    static std::uniform_real_distribution<> qty_dist(0.01, 1.0);
    static std::uniform_int_distribution<> side_dist(0, 1);
    
    Trade trade;
    trade.symbol = symbol;
    trade.exchange = Exchange::BINANCE;
    trade.price = price_dist(gen);
    trade.quantity = qty_dist(gen);
    trade.is_buyer_maker = side_dist(gen) == 1;
    trade.timestamp = std::chrono::system_clock::now();
    
    onTradeReceived(trade);
}

void BinanceClient::generateSampleTicker(const std::string& symbol) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> price_dist(49900.0, 50100.0);
    static std::uniform_real_distribution<> volume_dist(1000.0, 5000.0);
    static std::uniform_real_distribution<> change_dist(-2.0, 2.0);
    
    Ticker ticker;
    ticker.symbol = symbol;
    ticker.exchange = Exchange::BINANCE;
    ticker.last_price = price_dist(gen);
    ticker.volume_24h = volume_dist(gen);
    ticker.price_change_percent_24h = change_dist(gen) / 100.0;
    ticker.high_24h = ticker.last_price + 500.0;
    ticker.low_24h = ticker.last_price - 500.0;
    ticker.timestamp = std::chrono::system_clock::now();
    
    onTickerReceived(ticker);
}

} // namespace data
} // namespace arbitrage
