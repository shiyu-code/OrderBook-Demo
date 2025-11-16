#pragma once

#include "order_book_types.h"
#include "simple_websocket_client.h"
#include <memory>
#include <vector>
#include <string>

namespace orderbook {

class OptimizationStrategies {
public:
    struct StrategyResult {
        OptimizationStrategy strategy;
        std::string description;
        double avgLatencyImprovement;
        double stabilityScore;
        size_t successfulConnections;
        size_t totalConnections;
    };
    
    OptimizationStrategies();
    ~OptimizationStrategies();
    
    // Apply different optimization strategies
    static void applyBasicOptimization(SimpleWebSocketClient& manager);
    static void applyCompressionOptimization(SimpleWebSocketClient& manager);
    static void applyPingPongOptimization(SimpleWebSocketClient& manager, int intervalMs = 30000);
    static void applyBufferOptimization(SimpleWebSocketClient& manager, int bufferSize = 16384);
    static void applyThreadPoolOptimization(SimpleWebSocketClient& manager);
    
    // Compare strategies and return results
    static std::vector<StrategyResult> compareStrategies(
        const std::vector<OptimizationStrategy>& strategies,
        const std::string& testSymbol = "BTCUSDT",
        size_t testDurationSeconds = 60
    );
    
    // Get strategy name
    static std::string getStrategyName(OptimizationStrategy strategy);
    
    // Print strategy comparison report
    static void printComparisonReport(const std::vector<StrategyResult>& results);
    
private:
    static StrategyResult testStrategy(
        OptimizationStrategy strategy,
        const std::string& symbol,
        size_t durationSeconds
    );
};

} // namespace orderbook