#pragma once

#include "order_book_types.h"
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace orderbook {

class SimpleWebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ConnectionCallback = std::function<void(bool)>;
    
    explicit SimpleWebSocketClient(const WebSocketConfig& config);
    ~SimpleWebSocketClient();
    
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
    void ioThread();
    void messageThread();
    void pingThread();
    
    bool performWebSocketHandshake();
    std::string createWebSocketFrame(const std::string& data, int opcode = 1);
    std::string base64Encode(const std::string& input);
    std::string generateWebSocketKey();
    bool initTLS();
    void cleanupTLS();
    
    WebSocketConfig config_;
    std::string host_;
    int port_;
    std::string path_;
    
    int socket_;
    std::thread ioThread_;
    std::thread messageThread_;
    std::thread pingThread_;
    
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    ConnectionCallback connectionCallback_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::queue<std::string> sendQueue_;
    std::queue<std::string> receiveQueue_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> pingPongEnabled_;
    int pingIntervalMs_;
    bool useTLS_ = false;
    void* sslCtx_ = nullptr;
    void* ssl_ = nullptr;
};

} // namespace orderbook
