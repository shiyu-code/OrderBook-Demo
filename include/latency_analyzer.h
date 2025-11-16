#pragma once

#include "order_book_types.h"
#include <vector>
#include <chrono>
#include <mutex>
#include <map>
#include <algorithm>
#include <numeric>

namespace orderbook {

class LatencyAnalyzer {
public:
    LatencyAnalyzer();
    ~LatencyAnalyzer();
    
    // Record a latency measurement
    void recordLatency(double latencyMs);
    
    // Get current latency metrics
    LatencyMetrics getMetrics() const;
    
    // Get latency distribution histogram
    std::map<double, size_t> getLatencyDistribution(double binSizeMs = 1.0) const;
    
    // Get recent latencies (last N samples)
    std::vector<double> getRecentLatencies(size_t count = 100) const;
    
    // Reset statistics
    void reset();
    
    // Calculate latency from order book data (E field)
    static double calculateLatencyMs(const OrderBookData& data);
    
    // Print latency report
    void printReport() const;
    
private:
    mutable std::mutex mutex_;
    std::vector<double> latencies_;
    mutable std::vector<double> sortedLatencies_;
    mutable bool needsSorting_;
    
    void updateSortedLatencies() const;
    double calculatePercentile(double percentile) const;
};

} // namespace orderbook