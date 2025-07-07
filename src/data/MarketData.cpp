#include "MarketData.hpp"
#include <algorithm>
#include <stdexcept>

namespace arbitrage {
namespace data {

std::string exchangeToString(Exchange exchange) {
    switch (exchange) {
        case Exchange::BINANCE: return "binance";
        case Exchange::OKX: return "okx";
        case Exchange::BYBIT: return "bybit";
        default: return "unknown";
    }
}

Exchange stringToExchange(const std::string& str) {
    if (str == "binance") return Exchange::BINANCE;
    if (str == "okx") return Exchange::OKX;
    if (str == "bybit") return Exchange::BYBIT;
    throw std::invalid_argument("Unknown exchange: " + str);
}

// OrderBook implementation
double OrderBook::getBestBid() const {
    return bids.empty() ? 0.0 : bids[0].price;
}

double OrderBook::getBestAsk() const {
    return asks.empty() ? 0.0 : asks[0].price;
}

double OrderBook::getMidPrice() const {
    if (bids.empty() || asks.empty()) {
        return 0.0;
    }
    return (getBestBid() + getBestAsk()) / 2.0;
}

bool OrderBook::isValid() const {
    if (bids.empty() || asks.empty()) {
        return false;
    }
    
    // Check if best bid < best ask
    if (getBestBid() >= getBestAsk()) {
        return false;
    }
    
    // Check if bids are sorted in descending order
    for (size_t i = 1; i < bids.size(); ++i) {
        if (bids[i].price > bids[i-1].price) {
            return false;
        }
    }
    
    // Check if asks are sorted in ascending order
    for (size_t i = 1; i < asks.size(); ++i) {
        if (asks[i].price < asks[i-1].price) {
            return false;
        }
    }
    
    return true;
}

// MarketData implementation
void MarketData::updateOrderBook(Exchange exchange, const OrderBook& orderbook) {
    orderbooks[exchange] = orderbook;
}

void MarketData::updateTrade(Exchange exchange, const Trade& trade) {
    latest_trades[exchange] = trade;
}

void MarketData::updateTicker(Exchange exchange, const Ticker& ticker) {
    tickers[exchange] = ticker;
}

void MarketData::updateFundingRate(Exchange exchange, const FundingRate& funding_rate) {
    funding_rates[exchange] = funding_rate;
}

void MarketData::updateMarkPrice(Exchange exchange, const MarkPrice& mark_price) {
    mark_prices[exchange] = mark_price;
}

bool MarketData::hasOrderBook(Exchange exchange) const {
    return orderbooks.find(exchange) != orderbooks.end();
}

bool MarketData::hasTrade(Exchange exchange) const {
    return latest_trades.find(exchange) != latest_trades.end();
}

bool MarketData::hasTicker(Exchange exchange) const {
    return tickers.find(exchange) != tickers.end();
}

bool MarketData::hasFundingRate(Exchange exchange) const {
    return funding_rates.find(exchange) != funding_rates.end();
}

bool MarketData::hasMarkPrice(Exchange exchange) const {
    return mark_prices.find(exchange) != mark_prices.end();
}

} // namespace data
} // namespace arbitrage
