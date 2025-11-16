#pragma once

#include "order_book_types.h"
#include "websocket_manager.h"
#include <functional>
#include <memory>
#include <string>

namespace orderbook {

class BinanceClient {
public:
    using OrderBookCallback = std::function<void(const OrderBookData&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    explicit BinanceClient(const WebSocketConfig& config);
    ~BinanceClient();
    
    // Connect to Binance WebSocket API
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Subscribe to order book updates
    void subscribeOrderBook(const std::string& symbol, int limit = 100);
    void unsubscribeOrderBook(const std::string& symbol);
    void requestDepthOnce(const std::string& symbol, int limit = 100);
    
    // Set callbacks
    void setOrderBookCallback(OrderBookCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Get current configuration
    const WebSocketConfig& getConfig() const { return config_; }
    
private:
    void onMessage(const std::string& message);
    void onError(const std::string& error);
    void handleOrderBookUpdate(const std::string& jsonData);
    void buildTop(OrderBookData& out);
    void applyPairs(const std::string& src, bool isBid);
    void printDepthResult(const std::string& fullMessage, const std::string& resultObj);
    
    WebSocketConfig config_;
    std::unique_ptr<WebSocketManager> wsManager_;
    OrderBookCallback orderBookCallback_;
    ErrorCallback errorCallback_;
    bool connected_;
    int bookLimit_ = 20;
    std::map<double,double,std::greater<double>> bidMap_;
    std::map<double,double,std::less<double>> askMap_;
};

} // namespace orderbook