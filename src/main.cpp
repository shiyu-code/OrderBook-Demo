#include "binance_client.h"
#include "latency_analyzer.h"
#include "optimization_strategies.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <iomanip>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

void printUsage(const std::string& programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --symbol <symbol>     Trading symbol (default: BTCUSDT)" << std::endl;
    std::cout << "  --limit <limit>       Order book depth limit (default: 100)" << std::endl;
    std::cout << "  --duration <seconds>  Test duration in seconds (default: 60)" << std::endl;
    std::cout << "  --strategy <name>     Specific strategy to test (basic, compression, pingpong, buffer, threadpool)" << std::endl;
    std::cout << "  --compare             Run comparison of all strategies" << std::endl;
    std::cout << "  --endpoint <uri>      Override WebSocket endpoint (e.g., wss://ws-api.binance.com/ws-api/v3)" << std::endl;
    std::cout << "  --proxy <uri>         Use HTTP CONNECT proxy (e.g., http://proxy.host:8080)" << std::endl;
    std::cout << "  --host <name>         Override TLS SNI/Host header (e.g., ws-api.binance.com)" << std::endl;
    std::cout << "  --resolve <ip>        Resolve hostname to specific IP (e.g., 118.193.240.41)" << std::endl;
    std::cout << "  --help                Show this help message" << std::endl;
}

orderbook::OptimizationStrategy parseStrategy(const std::string& strategyName) {
    if (strategyName == "basic") return orderbook::OptimizationStrategy::BASIC;
    if (strategyName == "compression") return orderbook::OptimizationStrategy::COMPRESSION;
    if (strategyName == "pingpong") return orderbook::OptimizationStrategy::PING_PONG;
    if (strategyName == "buffer") return orderbook::OptimizationStrategy::BUFFER_OPTIMIZATION;
    if (strategyName == "threadpool") return orderbook::OptimizationStrategy::THREAD_POOL;
    
    throw std::invalid_argument("Unknown strategy: " + strategyName);
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    std::string symbol = "BTCUSDT";
    int limit = 100;
    int duration = 60;
    std::string strategyName;
    std::string endpointOverride;
    std::string proxyOverride;
    std::string hostOverride;
    std::string resolveIp;
    bool compareAll = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--symbol" && i + 1 < argc) {
            symbol = argv[++i];
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::stoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::stoi(argv[++i]);
        } else if (arg == "--strategy" && i + 1 < argc) {
            strategyName = argv[++i];
        } else if (arg == "--endpoint" && i + 1 < argc) {
            endpointOverride = argv[++i];
        } else if (arg == "--proxy" && i + 1 < argc) {
            proxyOverride = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            hostOverride = argv[++i];
        } else if (arg == "--resolve" && i + 1 < argc) {
            resolveIp = argv[++i];
        } else if (arg == "--compare") {
            compareAll = true;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "=== Binance Order Book Demo ===" << std::endl;
    std::cout << "Symbol: " << symbol << std::endl;
    std::cout << "Limit: " << limit << std::endl;
    std::cout << "Duration: " << duration << " seconds" << std::endl;
    
    if (compareAll) {
        // Run comparison of all strategies
        std::vector<orderbook::OptimizationStrategy> strategies = {
            orderbook::OptimizationStrategy::BASIC,
            orderbook::OptimizationStrategy::COMPRESSION,
            orderbook::OptimizationStrategy::PING_PONG,
            orderbook::OptimizationStrategy::BUFFER_OPTIMIZATION,
            orderbook::OptimizationStrategy::THREAD_POOL
        };
        
        auto results = orderbook::OptimizationStrategies::compareStrategies(strategies, symbol, duration);
        orderbook::OptimizationStrategies::printComparisonReport(results);
        
    } else if (!strategyName.empty()) {
        // Run specific strategy
        try {
            auto strategy = parseStrategy(strategyName);
            std::cout << "Strategy: " << strategyName << std::endl;
            
            // Create client with specific strategy
            orderbook::WebSocketConfig config;
            config.symbol = symbol;
            config.limit = limit;
            config.strategy = strategy;
            if (!endpointOverride.empty()) {
                config.uri = endpointOverride;
            } else {
                config.uri = "wss://ws-api.binance.com/ws-api/v3";
            }
            if (!proxyOverride.empty()) {
                config.proxyUri = proxyOverride;
            }
            if (!hostOverride.empty()) {
                config.overrideHost = hostOverride;
            }
            if (!resolveIp.empty()) {
                // rewrite URI to use the resolved IP while keeping the path
                auto pos = config.uri.find("/", 6); // after scheme
                std::string path = pos != std::string::npos ? config.uri.substr(pos) : "/";
                config.uri = std::string("wss://") + resolveIp + path;
                if (config.overrideHost.empty()) {
                    // default override host is ws-api hostname if not provided
                    config.overrideHost = "ws-api.binance.com";
                }
            }
            
            orderbook::LatencyAnalyzer analyzer;
            orderbook::BinanceClient client(config);
            
            // Set up callbacks
            client.setOrderBookCallback([&analyzer](const orderbook::OrderBookData& data) {
                double latency = orderbook::LatencyAnalyzer::calculateLatencyMs(data);
                analyzer.recordLatency(latency);
                
                std::cout << "OrderBook Update - "
                         << "Bids: " << data.bids.size() 
                         << ", Asks: " << data.asks.size()
                         << ", Latency: " << std::fixed << std::setprecision(2) 
                         << latency << " ms" << std::endl;
            });
            
            client.setErrorCallback([](const std::string& error) {
                std::cerr << "Error: " << error << std::endl;
            });
            
            std::cout << "Endpoint: " << config.uri << std::endl;
            if (!config.proxyUri.empty()) {
                std::cout << "Proxy: " << config.proxyUri << std::endl;
            }
            if (!config.overrideHost.empty()) {
                std::cout << "SNI/Host: " << config.overrideHost << std::endl;
            }
            if (!client.connect()) {
                std::cerr << "Connect failed, switching to testnet" << std::endl;
                orderbook::WebSocketConfig fallback = config;
                fallback.uri = "wss://stream.binancefuture.com/stream";
                orderbook::BinanceClient testClient(fallback);
                testClient.setOrderBookCallback([&analyzer](const orderbook::OrderBookData& data) {
                    double latency = orderbook::LatencyAnalyzer::calculateLatencyMs(data);
                    analyzer.recordLatency(latency);
                    std::cout << "OrderBook Update - "
                             << "Bids: " << data.bids.size() 
                             << ", Asks: " << data.asks.size()
                             << ", Latency: " << std::fixed << std::setprecision(2) 
                             << latency << " ms" << std::endl;
                });
                testClient.setErrorCallback([](const std::string& error) { std::cerr << "Error: " << error << std::endl; });
                std::cout << "Endpoint: " << fallback.uri << std::endl;
                if (!fallback.proxyUri.empty()) { std::cout << "Proxy: " << fallback.proxyUri << std::endl; }
                if (!fallback.overrideHost.empty()) { std::cout << "SNI/Host: " << fallback.overrideHost << std::endl; }
                if (!testClient.connect()) { std::cerr << "Failed to connect to testnet" << std::endl; return 1; }
                std::cout << "Connected to Binance WebSocket API (testnet)" << std::endl;
                if (fallback.uri.find("/ws-fapi/") != std::string::npos || fallback.uri.find("/ws-api/") != std::string::npos) {
                    testClient.requestDepthOnce(symbol, limit);
                } else {
                    testClient.subscribeOrderBook(symbol, limit);
                }
                auto startTime = std::chrono::steady_clock::now();
                while (running && std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count() < duration) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - startTime).count() % 10 == 0) {
                        analyzer.printReport();
                    }
                }
                testClient.disconnect();
                std::cout << "\nFinal report:" << std::endl;
                analyzer.printReport();
            } else {
                std::cout << "Connected to Binance WebSocket API" << std::endl;
                if (config.uri.find("/ws-fapi/") != std::string::npos || config.uri.find("/ws-api/") != std::string::npos) {
                    client.requestDepthOnce(symbol, limit);
                } else {
                    client.subscribeOrderBook(symbol, limit);
                }
                auto startTime = std::chrono::steady_clock::now();
                while (running && std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count() < duration) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - startTime).count() % 10 == 0) {
                        analyzer.printReport();
                    }
                }
                client.disconnect();
                std::cout << "\nFinal report:" << std::endl;
                analyzer.printReport();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
        
    } else {
        // Run with default settings (basic strategy)
        std::cout << "Using default strategy (basic)" << std::endl;
        
        orderbook::WebSocketConfig config;
        config.symbol = symbol;
        config.limit = limit;
        if (!endpointOverride.empty()) {
            config.uri = endpointOverride;
        } else {
            config.uri = "wss://ws-api.binance.com/ws-api/v3";
        }
        if (!proxyOverride.empty()) {
            config.proxyUri = proxyOverride;
        }
        if (!hostOverride.empty()) {
            config.overrideHost = hostOverride;
        }
        if (!resolveIp.empty()) {
            auto pos = config.uri.find("/", 6);
            std::string path = pos != std::string::npos ? config.uri.substr(pos) : "/";
            config.uri = std::string("wss://") + resolveIp + path;
            if (config.overrideHost.empty()) {
                config.overrideHost = "ws-api.binance.com";
            }
        }
        
        orderbook::LatencyAnalyzer analyzer;
        orderbook::BinanceClient client(config);
        
        // Set up callbacks
        client.setOrderBookCallback([&analyzer](const orderbook::OrderBookData& data) {
            double latency = orderbook::LatencyAnalyzer::calculateLatencyMs(data);
            analyzer.recordLatency(latency);
            
            std::cout << "OrderBook Update - "
                     << "Bids: " << data.bids.size() 
                     << ", Asks: " << data.asks.size()
                     << ", Latency: " << std::fixed << std::setprecision(2) 
                     << latency << " ms" << std::endl;
        });
        
        client.setErrorCallback([](const std::string& error) {
            std::cerr << "Error: " << error << std::endl;
        });
        
        std::cout << "Endpoint: " << config.uri << std::endl;
        if (!config.proxyUri.empty()) {
            std::cout << "Proxy: " << config.proxyUri << std::endl;
        }
        if (!config.overrideHost.empty()) {
            std::cout << "SNI/Host: " << config.overrideHost << std::endl;
        }
        if (!client.connect()) {
            std::cerr << "Connect failed, switching to testnet" << std::endl;
            orderbook::WebSocketConfig fallback = config;
            fallback.uri = "wss://stream.binancefuture.com/stream";
            orderbook::BinanceClient testClient(fallback);
            testClient.setOrderBookCallback([&analyzer](const orderbook::OrderBookData& data) {
                double latency = orderbook::LatencyAnalyzer::calculateLatencyMs(data);
                analyzer.recordLatency(latency);
                std::cout << "OrderBook Update - "
                         << "Bids: " << data.bids.size() 
                         << ", Asks: " << data.asks.size()
                         << ", Latency: " << std::fixed << std::setprecision(2) 
                         << latency << " ms" << std::endl;
            });
            testClient.setErrorCallback([](const std::string& error) { std::cerr << "Error: " << error << std::endl; });
            std::cout << "Endpoint: " << fallback.uri << std::endl;
            if (!fallback.proxyUri.empty()) { std::cout << "Proxy: " << fallback.proxyUri << std::endl; }
            if (!fallback.overrideHost.empty()) { std::cout << "SNI/Host: " << fallback.overrideHost << std::endl; }
            if (!testClient.connect()) { std::cerr << "Failed to connect to testnet" << std::endl; return 1; }
            std::cout << "Connected to Binance WebSocket API (testnet)" << std::endl;
            if (fallback.uri.find("/ws-fapi/") != std::string::npos || fallback.uri.find("/ws-api/") != std::string::npos) {
                testClient.requestDepthOnce(symbol, limit);
            } else {
                testClient.subscribeOrderBook(symbol, limit);
            }
            auto startTime = std::chrono::steady_clock::now();
            while (running && std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count() < duration) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count() % 10 == 0) {
                    analyzer.printReport();
                }
            }
            testClient.disconnect();
            std::cout << "\nFinal report:" << std::endl;
            analyzer.printReport();
        } else {
            std::cout << "Connected to Binance WebSocket API" << std::endl;
            if (config.uri.find("/ws-fapi/") != std::string::npos || config.uri.find("/ws-api/") != std::string::npos) {
                client.requestDepthOnce(symbol, limit);
            } else {
                client.subscribeOrderBook(symbol, limit);
            }
            auto startTime = std::chrono::steady_clock::now();
            while (running && std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count() < duration) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count() % 10 == 0) {
                    analyzer.printReport();
                }
            }
            client.disconnect();
            std::cout << "\nFinal report:" << std::endl;
            analyzer.printReport();
        }
    }
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}