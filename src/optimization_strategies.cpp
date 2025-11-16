#include "optimization_strategies.h"
#include "binance_client.h"
#include "latency_analyzer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>

namespace orderbook {

OptimizationStrategies::OptimizationStrategies() = default;
OptimizationStrategies::~OptimizationStrategies() = default;

void OptimizationStrategies::applyBasicOptimization(SimpleWebSocketClient& manager) {
    // Basic optimization: standard settings
    manager.enableCompression(false);
    manager.enablePingPong(false);
    manager.setBufferSize(4096);
}

void OptimizationStrategies::applyCompressionOptimization(SimpleWebSocketClient& manager) {
    // Enable WebSocket compression
    manager.enableCompression(true);
    manager.enablePingPong(true, 30000); // 30 second ping interval
    manager.setBufferSize(8192);
}

void OptimizationStrategies::applyPingPongOptimization(SimpleWebSocketClient& manager, int intervalMs) {
    // Aggressive ping-pong to keep connection alive
    manager.enableCompression(true);
    manager.enablePingPong(true, intervalMs);
    manager.setBufferSize(16384);
}

void OptimizationStrategies::applyBufferOptimization(SimpleWebSocketClient& manager, int bufferSize) {
    // Large buffer for better throughput
    manager.enableCompression(true);
    manager.enablePingPong(true, 30000);
    manager.setBufferSize(bufferSize);
}

void OptimizationStrategies::applyThreadPoolOptimization(SimpleWebSocketClient& manager) {
    // Multi-threaded processing (simulated)
    manager.enableCompression(true);
    manager.enablePingPong(true, 20000); // More frequent pings
    manager.setBufferSize(32768); // Very large buffer
}

std::string OptimizationStrategies::getStrategyName(OptimizationStrategy strategy) {
    switch (strategy) {
        case OptimizationStrategy::BASIC:
            return "Basic";
        case OptimizationStrategy::COMPRESSION:
            return "Compression";
        case OptimizationStrategy::PING_PONG:
            return "Ping-Pong";
        case OptimizationStrategy::BUFFER_OPTIMIZATION:
            return "Buffer Optimization";
        case OptimizationStrategy::THREAD_POOL:
            return "Thread Pool";
        default:
            return "Unknown";
    }
}

std::vector<OptimizationStrategies::StrategyResult> OptimizationStrategies::compareStrategies(
    const std::vector<OptimizationStrategy>& strategies,
    const std::string& testSymbol,
    size_t testDurationSeconds) {
    
    std::vector<StrategyResult> results;
    
    std::cout << "\n=== Starting Strategy Comparison ===" << std::endl;
    std::cout << "Symbol: " << testSymbol << std::endl;
    std::cout << "Duration: " << testDurationSeconds << " seconds per strategy" << std::endl;
    std::cout << "Strategies to test: " << strategies.size() << std::endl;
    
    for (size_t i = 0; i < strategies.size(); ++i) {
        auto strategy = strategies[i];
        std::cout << "\nTesting strategy " << (i + 1) << "/" << strategies.size() 
                  << ": " << getStrategyName(strategy) << std::endl;
        
        auto result = testStrategy(strategy, testSymbol, testDurationSeconds);
        results.push_back(result);
        
        std::cout << "  Average latency: " << std::fixed << std::setprecision(2) 
                  << result.avgLatencyImprovement << " ms" << std::endl;
        std::cout << "  Stability score: " << std::fixed << std::setprecision(2) 
                  << result.stabilityScore << std::endl;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * result.successfulConnections / result.totalConnections) << "%" << std::endl;
    }
    
    return results;
}

void OptimizationStrategies::printComparisonReport(const std::vector<StrategyResult>& results) {
    if (results.empty()) {
        std::cout << "No strategy results to display." << std::endl;
        return;
    }
    
    std::cout << "\n=== Strategy Comparison Report ===" << std::endl;
    std::cout << std::setw(20) << "Strategy" 
              << std::setw(15) << "Avg Latency (ms)"
              << std::setw(15) << "Stability"
              << std::setw(12) << "Success Rate"
              << std::endl;
    std::cout << std::string(62, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::setw(20) << getStrategyName(result.strategy)
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.avgLatencyImprovement
                  << std::setw(15) << std::fixed << std::setprecision(3) << result.stabilityScore
                  << std::setw(11) << std::fixed << std::setprecision(1) 
                  << (100.0 * result.successfulConnections / result.totalConnections) << "%"
                  << std::endl;
    }
    
    // Find best strategy
    auto bestStrategy = std::min_element(results.begin(), results.end(),
        [](const StrategyResult& a, const StrategyResult& b) {
            return a.avgLatencyImprovement < b.avgLatencyImprovement;
        });
    
    if (bestStrategy != results.end()) {
        std::cout << "\nBest strategy: " << getStrategyName(bestStrategy->strategy) 
                  << " (Average latency: " << std::fixed << std::setprecision(2) 
                  << bestStrategy->avgLatencyImprovement << " ms)" << std::endl;
    }
    
    std::cout << "=======================================" << std::endl;
}

OptimizationStrategies::StrategyResult OptimizationStrategies::testStrategy(
    OptimizationStrategy strategy,
    const std::string& symbol,
    size_t durationSeconds) {
    
    StrategyResult result;
    result.strategy = strategy;
    result.description = getStrategyName(strategy);
    result.successfulConnections = 0;
    result.totalConnections = 1;
    
    WebSocketConfig config;
    config.symbol = symbol;
    config.limit = 100;
    config.strategy = strategy;
    
    // Apply strategy-specific settings
    switch (strategy) {
        case OptimizationStrategy::BASIC:
            config.enableCompression = false;
            config.enablePingPong = false;
            config.bufferSize = 4096;
            break;
        case OptimizationStrategy::COMPRESSION:
            config.enableCompression = true;
            config.enablePingPong = true;
            config.bufferSize = 8192;
            break;
        case OptimizationStrategy::PING_PONG:
            config.enableCompression = true;
            config.enablePingPong = true;
            config.bufferSize = 16384;
            break;
        case OptimizationStrategy::BUFFER_OPTIMIZATION:
            config.enableCompression = true;
            config.enablePingPong = true;
            config.bufferSize = 32768;
            break;
        case OptimizationStrategy::THREAD_POOL:
            config.enableCompression = true;
            config.enablePingPong = true;
            config.bufferSize = 65536;
            break;
    }
    
    LatencyAnalyzer analyzer;
    BinanceClient client(config);
    
    // Set up callback to record latencies
    client.setOrderBookCallback([&analyzer](const OrderBookData& data) {
        double latency = LatencyAnalyzer::calculateLatencyMs(data);
        analyzer.recordLatency(latency);
    });
    
    client.setErrorCallback([&result](const std::string& error) {
        std::cerr << "Error during strategy test: " << error << std::endl;
    });
    
    // Connect and start test
    if (client.connect()) {
        result.successfulConnections = 1;
        client.subscribeOrderBook(symbol, 100);
        
        // Run test for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        
        client.disconnect();
    }
    
    // Calculate metrics
    auto metrics = analyzer.getMetrics();
    result.avgLatencyImprovement = metrics.avgLatency;
    
    // Calculate stability score (lower variance = higher stability)
    if (metrics.totalSamples > 0) {
        double variance = (metrics.p95Latency - metrics.p50Latency) / metrics.p50Latency;
        result.stabilityScore = 1.0 / (1.0 + variance); // Higher score = more stable
    } else {
        result.stabilityScore = 0.0;
    }
    
    return result;
}

} // namespace orderbook