#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace orderbook {

struct Order {
    double price;
    double quantity;
    
    Order() = default;
    Order(double p, double q) : price(p), quantity(q) {}
};

struct OrderBookData {
    std::string symbol;
    long long lastUpdateId;
    long long messageTime;  // E field - message output time
    long long transactionTime; // T field - transaction time
    std::vector<Order> bids;
    std::vector<Order> asks;
    
    std::chrono::system_clock::time_point receiveTime;
};

struct LatencyMetrics {
    double minLatency;
    double maxLatency;
    double avgLatency;
    double p50Latency;
    double p95Latency;
    double p99Latency;
    size_t totalSamples;
    
    LatencyMetrics() : minLatency(0), maxLatency(0), avgLatency(0), 
                       p50Latency(0), p95Latency(0), p99Latency(0), totalSamples(0) {}
};

enum class OptimizationStrategy {
    BASIC,
    COMPRESSION,
    PING_PONG,
    BUFFER_OPTIMIZATION,
    THREAD_POOL
};

struct WebSocketConfig {
    std::string uri;
    std::string symbol;
    int limit;
    bool enableCompression;
    bool enablePingPong;
    int bufferSize;
    OptimizationStrategy strategy;
    std::string proxyUri;
    std::string overrideHost;
    std::string resolveIp;
    
    WebSocketConfig() : uri("wss://ws-api.binance.com/ws-api/v3"), symbol("BTCUSDT"), 
                       limit(100), enableCompression(true), enablePingPong(true), 
                       bufferSize(8192), strategy(OptimizationStrategy::BASIC), proxyUri(""), 
                       overrideHost(""), resolveIp("") {}
};

} // namespace orderbook