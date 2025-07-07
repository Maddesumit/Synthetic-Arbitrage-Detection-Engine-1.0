#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>

namespace arbitrage {
namespace data {

/**
 * @brief Exchange identifier
 */
enum class Exchange {
    BINANCE,
    OKX,
    BYBIT
};

/**
 * @brief Convert exchange enum to string
 */
std::string exchangeToString(Exchange exchange);

/**
 * @brief Convert string to exchange enum
 */
Exchange stringToExchange(const std::string& str);

/**
 * @brief Price level in order book
 */
struct PriceLevel {
    double price;
    double quantity;
    
    PriceLevel() = default;
    PriceLevel(double p, double q) : price(p), quantity(q) {}
};

/**
 * @brief Order book snapshot
 */
struct OrderBook {
    std::string symbol;
    Exchange exchange;
    std::chrono::system_clock::time_point timestamp;
    std::vector<PriceLevel> bids;  // Sorted by price descending
    std::vector<PriceLevel> asks;  // Sorted by price ascending
    
    /**
     * @brief Get best bid price
     */
    double getBestBid() const;
    
    /**
     * @brief Get best ask price
     */
    double getBestAsk() const;
    
    /**
     * @brief Get mid price
     */
    double getMidPrice() const;
    
    /**
     * @brief Check if order book is valid
     */
    bool isValid() const;
};

/**
 * @brief Trade data
 */
struct Trade {
    std::string symbol;
    Exchange exchange;
    std::chrono::system_clock::time_point timestamp;
    double price;
    double quantity;
    bool is_buyer_maker;  // true if buyer is maker, false if seller is maker
    
    Trade() = default;
    Trade(const std::string& sym, Exchange ex, double p, double q, bool buyer_maker)
        : symbol(sym), exchange(ex), price(p), quantity(q), is_buyer_maker(buyer_maker) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * @brief Ticker data (24h statistics)
 */
struct Ticker {
    std::string symbol;
    Exchange exchange;
    std::chrono::system_clock::time_point timestamp;
    double bid;                         // Best bid price
    double ask;                         // Best ask price
    double last;                        // Last trade price
    double volume;                      // 24h volume
    double price_change_24h;
    double price_change_percent_24h;
    double high_24h;
    double low_24h;
    
    Ticker() = default;
    Ticker(const std::string& sym, Exchange ex) : symbol(sym), exchange(ex) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * @brief Funding rate data for perpetual contracts
 */
struct FundingRate {
    std::string symbol;
    Exchange exchange;
    std::chrono::system_clock::time_point timestamp;
    double funding_rate;
    std::chrono::system_clock::time_point funding_time;
    
    FundingRate() = default;
    FundingRate(const std::string& sym, Exchange ex, double rate)
        : symbol(sym), exchange(ex), funding_rate(rate) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * @brief Mark price data
 */
struct MarkPrice {
    std::string symbol;
    Exchange exchange;
    std::chrono::system_clock::time_point timestamp;
    double mark_price;
    double index_price;
    
    MarkPrice() = default;
    MarkPrice(const std::string& sym, Exchange ex, double mark, double index)
        : symbol(sym), exchange(ex), mark_price(mark), index_price(index) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * @brief Market data point for UI compatibility
 */
struct MarketDataPoint {
    std::string symbol;
    std::string exchange;
    std::chrono::milliseconds timestamp;
    double bid = 0.0;
    double ask = 0.0;
    double last = 0.0;
    double volume = 0.0;
    double funding_rate = 0.0;
    
    MarketDataPoint() {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }
    
    MarketDataPoint(const std::string& sym, const std::string& ex)
        : symbol(sym), exchange(ex) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }
};

/**
 * @brief Market data container for a single symbol across all exchanges
 */
struct MarketData {
    std::string symbol;
    std::map<Exchange, OrderBook> orderbooks;
    std::map<Exchange, Trade> latest_trades;
    std::map<Exchange, Ticker> tickers;
    std::map<Exchange, FundingRate> funding_rates;
    std::map<Exchange, MarkPrice> mark_prices;
    
    /**
     * @brief Update order book for specific exchange
     */
    void updateOrderBook(Exchange exchange, const OrderBook& orderbook);
    
    /**
     * @brief Update trade for specific exchange
     */
    void updateTrade(Exchange exchange, const Trade& trade);
    
    /**
     * @brief Update ticker for specific exchange
     */
    void updateTicker(Exchange exchange, const Ticker& ticker);
    
    /**
     * @brief Update funding rate for specific exchange
     */
    void updateFundingRate(Exchange exchange, const FundingRate& funding_rate);
    
    /**
     * @brief Update mark price for specific exchange
     */
    void updateMarkPrice(Exchange exchange, const MarkPrice& mark_price);
    
    /**
     * @brief Check if data is available for specific exchange
     */
    bool hasOrderBook(Exchange exchange) const;
    bool hasTrade(Exchange exchange) const;
    bool hasTicker(Exchange exchange) const;
    bool hasFundingRate(Exchange exchange) const;
    bool hasMarkPrice(Exchange exchange) const;
};

} // namespace data
} // namespace arbitrage
