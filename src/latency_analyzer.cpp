#include "latency_analyzer.h"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace orderbook {

LatencyAnalyzer::LatencyAnalyzer() : needsSorting_(false) {
    latencies_.reserve(10000); // Reserve space for better performance
}

LatencyAnalyzer::~LatencyAnalyzer() = default;

void LatencyAnalyzer::recordLatency(double latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    latencies_.push_back(latencyMs);
    needsSorting_ = true;
    
    // Keep only recent data to prevent memory issues
    if (latencies_.size() > 100000) {
        latencies_.erase(latencies_.begin(), latencies_.begin() + 10000);
    }
}

LatencyMetrics LatencyAnalyzer::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (latencies_.empty()) {
        return LatencyMetrics();
    }
    
    updateSortedLatencies();
    
    LatencyMetrics metrics;
    metrics.totalSamples = latencies_.size();
    metrics.minLatency = sortedLatencies_.front();
    metrics.maxLatency = sortedLatencies_.back();
    metrics.avgLatency = std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
    
    // Calculate percentiles
    metrics.p50Latency = calculatePercentile(0.5);
    metrics.p95Latency = calculatePercentile(0.95);
    metrics.p99Latency = calculatePercentile(0.99);
    
    return metrics;
}

std::map<double, size_t> LatencyAnalyzer::getLatencyDistribution(double binSizeMs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<double, size_t> distribution;
    
    if (latencies_.empty()) {
        return distribution;
    }
    
    for (double latency : latencies_) {
        double bin = std::floor(latency / binSizeMs) * binSizeMs;
        distribution[bin]++;
    }
    
    return distribution;
}

std::vector<double> LatencyAnalyzer::getRecentLatencies(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (latencies_.size() <= count) {
        return latencies_;
    }
    
    return std::vector<double>(latencies_.end() - count, latencies_.end());
}

void LatencyAnalyzer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_.clear();
    sortedLatencies_.clear();
    needsSorting_ = false;
}

double LatencyAnalyzer::calculateLatencyMs(const OrderBookData& data) {
    auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return static_cast<double>(currentTime - data.messageTime);
}

void LatencyAnalyzer::printReport() const {
    auto metrics = getMetrics();
    auto distribution = getLatencyDistribution(5.0); // 5ms bins
    
    std::cout << "\n=== Latency Analysis Report ===" << std::endl;
    std::cout << "Total Samples: " << metrics.totalSamples << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Min Latency: " << metrics.minLatency << " ms" << std::endl;
    std::cout << "Max Latency: " << metrics.maxLatency << " ms" << std::endl;
    std::cout << "Average Latency: " << metrics.avgLatency << " ms" << std::endl;
    std::cout << "P50 Latency: " << metrics.p50Latency << " ms" << std::endl;
    std::cout << "P95 Latency: " << metrics.p95Latency << " ms" << std::endl;
    std::cout << "P99 Latency: " << metrics.p99Latency << " ms" << std::endl;
    
    std::cout << "\nLatency Distribution (5ms bins):" << std::endl;
    for (const auto& [bin, count] : distribution) {
        std::cout << "  " << std::setw(4) << bin << "-" << std::setw(4) << (bin + 5) << " ms: " 
                  << std::setw(6) << count << " samples" << std::endl;
    }
    
    std::cout << "=============================" << std::endl;
}

void LatencyAnalyzer::updateSortedLatencies() const {
    if (!needsSorting_) {
        return;
    }
    
    sortedLatencies_ = latencies_; // This creates a copy
    std::sort(sortedLatencies_.begin(), sortedLatencies_.end());
    needsSorting_ = false;
}

double LatencyAnalyzer::calculatePercentile(double percentile) const {
    if (sortedLatencies_.empty()) {
        return 0.0;
    }
    
    size_t index = static_cast<size_t>(std::ceil(percentile * sortedLatencies_.size())) - 1;
    if (index >= sortedLatencies_.size()) {
        index = sortedLatencies_.size() - 1;
    }
    
    return sortedLatencies_[index];
}

} // namespace orderbook