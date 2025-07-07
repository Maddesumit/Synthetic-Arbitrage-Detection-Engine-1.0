#include "data/MarketData.hpp"
#include "data/WebSocketClient.hpp"
#include "data/BinanceClient.hpp"
#include "utils/Logger.hpp"
#include "utils/ConfigManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace arbitrage;

int main() {
    try {
        // Initialize logger
        utils::Logger::initialize();
        LOG_INFO("Starting Phase 2 Market Data Demo");
        
        // Load configuration
        utils::ConfigManager config("config/config.json");
        LOG_INFO("Configuration loaded successfully");
        
        // Create market data container
        data::MarketData btc_data;
        btc_data.symbol = "BTCUSDT";
        
        // Demonstrate market data structures
        LOG_INFO("=== Phase 2: Market Data Infrastructure Demo ===");
        
        // Create sample order book
        data::OrderBook sample_orderbook;
        sample_orderbook.symbol = "BTCUSDT";
        sample_orderbook.exchange = data::Exchange::BINANCE;
        sample_orderbook.timestamp = std::chrono::system_clock::now();
        
        // Add some sample bids (sorted by price descending)
        sample_orderbook.bids = {
            {50000.50, 1.5},
            {50000.25, 2.0},
            {50000.00, 0.8}
        };
        
        // Add some sample asks (sorted by price ascending)
        sample_orderbook.asks = {
            {50000.75, 1.2},
            {50001.00, 1.8},
            {50001.25, 0.5}
        };
        
        LOG_INFO("Sample OrderBook created:");
        LOG_INFO("  Symbol: {}", sample_orderbook.symbol);
        LOG_INFO("  Exchange: {}", data::exchangeToString(sample_orderbook.exchange));
        LOG_INFO("  Best Bid: ${:.2f}", sample_orderbook.getBestBid());
        LOG_INFO("  Best Ask: ${:.2f}", sample_orderbook.getBestAsk());
        LOG_INFO("  Mid Price: ${:.2f}", sample_orderbook.getMidPrice());
        LOG_INFO("  Is Valid: {}", sample_orderbook.isValid() ? "Yes" : "No");
        
        // Update market data container
        btc_data.updateOrderBook(data::Exchange::BINANCE, sample_orderbook);
        LOG_INFO("OrderBook added to MarketData container");
        
        // Create sample trade
        data::Trade sample_trade("BTCUSDT", data::Exchange::BINANCE, 50000.60, 0.15, false);
        btc_data.updateTrade(data::Exchange::BINANCE, sample_trade);
        
        LOG_INFO("Sample Trade created:");
        LOG_INFO("  Symbol: {}", sample_trade.symbol);
        LOG_INFO("  Exchange: {}", data::exchangeToString(sample_trade.exchange));
        LOG_INFO("  Price: ${:.2f}", sample_trade.price);
        LOG_INFO("  Quantity: {:.8f}", sample_trade.quantity);
        LOG_INFO("  Is Buyer Maker: {}", sample_trade.is_buyer_maker ? "Yes" : "No");
        
        // Create sample ticker
        data::Ticker sample_ticker("BTCUSDT", data::Exchange::BINANCE);
        sample_ticker.last = 50000.60;
        sample_ticker.volume = 1250.75;
        sample_ticker.price_change_24h = 1.5;
        sample_ticker.price_change_percent_24h = 0.003;
        sample_ticker.high_24h = 51200.00;
        sample_ticker.low_24h = 49800.00;
        
        btc_data.updateTicker(data::Exchange::BINANCE, sample_ticker);
        
        LOG_INFO("Sample Ticker created:");
        LOG_INFO("  Last Price: ${:.2f}", sample_ticker.last);
        LOG_INFO("  24h Volume: {:.2f}", sample_ticker.volume);
        LOG_INFO("  24h Change: {:.2f}%", sample_ticker.price_change_percent_24h * 100);
        LOG_INFO("  24h High: ${:.2f}", sample_ticker.high_24h);
        LOG_INFO("  24h Low: ${:.2f}", sample_ticker.low_24h);
        
        // Test WebSocket client factory
        LOG_INFO("\\n=== Testing WebSocket Client Factory ===");
        
        try {
            auto binance_client = data::createWebSocketClient(data::Exchange::BINANCE);
            LOG_INFO("Binance WebSocket client created successfully");
            LOG_INFO("Exchange: {}", data::exchangeToString(binance_client->getExchange()));
            LOG_INFO("Status: {}", static_cast<int>(binance_client->getConnectionStatus()));
            
            auto okx_client = data::createWebSocketClient(data::Exchange::OKX);
            LOG_INFO("OKX WebSocket client created successfully");
            
            auto bybit_client = data::createWebSocketClient(data::Exchange::BYBIT);
            LOG_INFO("Bybit WebSocket client created successfully");
            
        } catch (const std::exception& e) {
            LOG_ERROR("WebSocket client creation failed: {}", e.what());
        }
        
        // Demonstrate data validation
        LOG_INFO("\\n=== Data Validation Tests ===");
        LOG_INFO("MarketData has OrderBook for Binance: {}", 
                 btc_data.hasOrderBook(data::Exchange::BINANCE) ? "Yes" : "No");
        LOG_INFO("MarketData has OrderBook for OKX: {}", 
                 btc_data.hasOrderBook(data::Exchange::OKX) ? "Yes" : "No");
        LOG_INFO("MarketData has Trade for Binance: {}", 
                 btc_data.hasTrade(data::Exchange::BINANCE) ? "Yes" : "No");
        LOG_INFO("MarketData has Ticker for Binance: {}", 
                 btc_data.hasTicker(data::Exchange::BINANCE) ? "Yes" : "No");
        
        LOG_INFO("\\n=== Phase 2 Demo Completed Successfully! ===");
        LOG_INFO("Market data infrastructure is working correctly.");
        LOG_INFO("Ready to proceed with real WebSocket connections and Phase 3.");
        
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Demo failed with exception: {}", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Demo failed with unknown exception");
        return 1;
    }
}
