#pragma once

#include "order_book_types.h"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace orderbook {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

class WebSocketManager {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ConnectionCallback = std::function<void(bool)>;
    
    explicit WebSocketManager(const WebSocketConfig& config);
    ~WebSocketManager();
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    void send(const std::string& message);
    
    void setMessageCallback(MessageCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    
    // Optimization methods
    void enableCompression(bool enable);
    void enablePingPong(bool enable, int intervalMs = 30000);
    void setBufferSize(int size);
    
private:
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);
    
    void pingPongLoop();
    
    WebSocketConfig config_;
    client wsClient_;
    websocketpp::connection_hdl connectionHandle_;
    
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    ConnectionCallback connectionCallback_;
    
    std::thread ioThread_;
    std::thread pingThread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    bool connected_;
    bool shouldStop_;
    bool pingPongEnabled_;
    int pingIntervalMs_;
};

} // namespace orderbook