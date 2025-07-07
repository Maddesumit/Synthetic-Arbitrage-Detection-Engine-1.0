#pragma once

#include <functional>
#include <string>

namespace arbitrage {
namespace ui {

class WebSocket {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    
    WebSocket() = default;
    virtual ~WebSocket() = default;
    
    void onMessage(MessageHandler handler) {
        message_handler_ = std::move(handler);
    }
    
    void send(const std::string& message) {
        if (message_handler_) {
            message_handler_(message);
        }
    }
    
private:
    MessageHandler message_handler_;
};

} // namespace ui
} // namespace arbitrage 