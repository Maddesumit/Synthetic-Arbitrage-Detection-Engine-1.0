#include "WebSocketClient.hpp"
#include "BinanceClient.hpp"
#include "OKXClient.hpp"
#include "BybitClient.hpp"
#include <chrono>
#include <thread>

namespace arbitrage {
namespace data {

WebSocketClient::WebSocketClient(Exchange exchange) 
    : exchange_(exchange), current_reconnect_delay_(reconnect_initial_delay_) {
}

WebSocketClient::~WebSocketClient() {
    // Signal shutdown
    shutdown_requested_ = true;
    reconnection_active_ = false;
    
    // Give reconnection thread time to exit gracefully
    if (reconnection_thread_.joinable()) {
        try {
            // Wait up to 3 seconds for thread to finish
            auto start = std::chrono::steady_clock::now();
            while (reconnection_thread_.joinable() && 
                   std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            if (reconnection_thread_.joinable()) {
                // Force detach if join takes too long to prevent blocking destructor
                try {
                    reconnection_thread_.join();
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception joining reconnection thread in destructor: {}", e.what());
                    reconnection_thread_.detach();
                } catch (...) {
                    LOG_ERROR("Unknown exception joining reconnection thread in destructor");
                    reconnection_thread_.detach();
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during WebSocketClient destructor: {}", e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception during WebSocketClient destructor");
        }
    }
}

void WebSocketClient::setReconnectParameters(int initial_delay, int max_delay, double backoff_multiplier) {
    reconnect_initial_delay_ = initial_delay;
    reconnect_max_delay_ = max_delay;
    reconnect_backoff_multiplier_ = backoff_multiplier;
    current_reconnect_delay_ = initial_delay;
}

void WebSocketClient::updateConnectionStatus(ConnectionStatus status) {
    ConnectionStatus old_status = connection_status_.load();
    connection_status_ = status;
    
    LOG_INFO("WebSocket client for {} status changed from {} to {}", 
             exchangeToString(exchange_), 
             static_cast<int>(old_status), 
             static_cast<int>(status));
    
    if (connection_callback_) {
        connection_callback_(status);
    }
    
    // Start reconnection if connection lost and auto-reconnect is enabled
    if (status == ConnectionStatus::ERROR || status == ConnectionStatus::DISCONNECTED) {
        if (auto_reconnect_ && !reconnection_active_) {
            startReconnection();
        }
    } else if (status == ConnectionStatus::CONNECTED) {
        // Reset reconnection delay on successful connection
        current_reconnect_delay_ = reconnect_initial_delay_;
        reconnect_attempts_ = 0;  // Reset attempts on success
        reconnection_active_ = false;
    }
}

void WebSocketClient::handleError(const std::string& error_message) {
    LOG_ERROR("WebSocket client for {} error: {}", exchangeToString(exchange_), error_message);
    
    if (error_callback_) {
        error_callback_(error_message);
    }
    
    updateConnectionStatus(ConnectionStatus::ERROR);
}

void WebSocketClient::onOrderBookReceived(const OrderBook& orderbook) {
    if (orderbook_callback_) {
        orderbook_callback_(orderbook);
    }
}

void WebSocketClient::onTradeReceived(const Trade& trade) {
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

void WebSocketClient::onTickerReceived(const Ticker& ticker) {
    if (ticker_callback_) {
        ticker_callback_(ticker);
    }
}

void WebSocketClient::onFundingRateReceived(const FundingRate& funding_rate) {
    if (funding_rate_callback_) {
        funding_rate_callback_(funding_rate);
    }
}

void WebSocketClient::onMarkPriceReceived(const MarkPrice& mark_price) {
    if (mark_price_callback_) {
        mark_price_callback_(mark_price);
    }
}

void WebSocketClient::startReconnection() {
    // Prevent multiple reconnection attempts
    std::lock_guard<std::mutex> lock(reconnection_mutex_);
    
    if (reconnection_active_ || shutdown_requested_) {
        return;  // Already reconnecting or shutting down
    }
    
    reconnection_active_ = true;
    updateConnectionStatus(ConnectionStatus::RECONNECTING);
    
    // Safely start reconnection in a separate thread
    if (reconnection_thread_.joinable()) {
        try {
            reconnection_thread_.join();  // Wait for previous thread to finish
        } catch (const std::exception& e) {
            LOG_ERROR("Exception joining previous reconnection thread: {}", e.what());
        }
    }
    
    try {
        reconnection_thread_ = std::thread(&WebSocketClient::reconnectionLoop, this);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start reconnection thread for {}: {}", exchangeToString(exchange_), e.what());
        reconnection_active_ = false;
        updateConnectionStatus(ConnectionStatus::ERROR);
    }
}

void WebSocketClient::reconnectionLoop() {
    LOG_INFO("Starting reconnection loop for {}", exchangeToString(exchange_));
    
    while (reconnection_active_ && !shutdown_requested_ && connection_status_ != ConnectionStatus::CONNECTED) {
        // Check if we've exceeded maximum attempts
        if (reconnect_attempts_ >= max_reconnect_attempts_) {
            LOG_ERROR("Maximum reconnection attempts ({}) reached for {}, giving up", 
                     max_reconnect_attempts_, exchangeToString(exchange_));
            reconnection_active_ = false;
            updateConnectionStatus(ConnectionStatus::ERROR);
            break;
        }
        
        reconnect_attempts_++;
        LOG_INFO("Attempting to reconnect to {} (attempt {}/{}) in {}ms", 
                 exchangeToString(exchange_), reconnect_attempts_.load(), max_reconnect_attempts_, current_reconnect_delay_);
        
        // Wait before attempting reconnection (with early exit on shutdown)
        auto start_time = std::chrono::steady_clock::now();
        auto delay_ms = std::chrono::milliseconds(current_reconnect_delay_);
        
        while (std::chrono::steady_clock::now() - start_time < delay_ms) {
            if (!reconnection_active_ || shutdown_requested_) {
                LOG_INFO("Reconnection loop for {} interrupted by shutdown", exchangeToString(exchange_));
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!reconnection_active_ || shutdown_requested_) {
            break;
        }
        
        // Attempt to reconnect
        try {
            // Check for shutdown before attempting connection
            if (shutdown_requested_) {
                LOG_INFO("Reconnection cancelled due to shutdown request for {}", exchangeToString(exchange_));
                break;
            }
            
            if (connect()) {
                LOG_INFO("Reconnection to {} successful", exchangeToString(exchange_));
                reconnection_active_ = false;
                reconnect_attempts_ = 0;  // Reset attempts on success
                break;
            } else {
                LOG_WARN("Reconnection to {} failed, will retry", exchangeToString(exchange_));
                
                // Increase delay with backoff
                current_reconnect_delay_ = std::min(
                    static_cast<int>(current_reconnect_delay_ * reconnect_backoff_multiplier_),
                    reconnect_max_delay_
                );
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during reconnection to {}: {}", exchangeToString(exchange_), e.what());
            // Continue the loop instead of breaking on exception
        } catch (...) {
            LOG_ERROR("Unknown exception during reconnection to {}", exchangeToString(exchange_));
            // Continue the loop instead of breaking on exception
        }
    }
    
    LOG_INFO("Reconnection loop for {} ended", exchangeToString(exchange_));
}

std::unique_ptr<WebSocketClient> createWebSocketClient(Exchange exchange) {
    switch (exchange) {
        case Exchange::BINANCE:
            return std::make_unique<BinanceClient>();
        case Exchange::OKX:
            return std::make_unique<OKXClient>();
        case Exchange::BYBIT:
            return std::make_unique<BybitClient>();
        default:
            throw std::invalid_argument("Unsupported exchange");
    }
}

} // namespace data
} // namespace arbitrage
