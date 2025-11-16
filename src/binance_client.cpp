#include "binance_client.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <regex>

namespace orderbook {

BinanceClient::BinanceClient(const WebSocketConfig& config) 
    : config_(config), connected_(false) {
    wsManager_ = std::make_unique<WebSocketManager>(config_);
    bookLimit_ = config_.limit > 0 ? config_.limit : 20;
    
    // Set up callbacks
    wsManager_->setMessageCallback([this](const std::string& message) {
        this->onMessage(message);
    });
    
    wsManager_->setErrorCallback([this](const std::string& error) {
        this->onError(error);
    });

    wsManager_->setConnectionCallback([this](bool ok) {
        if (!ok) return;
        try {
            std::string uri = config_.uri;
            if (uri.find("/ws-fapi/") != std::string::npos || uri.find("/ws-api/") != std::string::npos) {
                this->requestDepthOnce(config_.symbol, config_.limit);
            } else {
                this->subscribeOrderBook(config_.symbol, config_.limit);
            }
        } catch (...) {}
    });
}

BinanceClient::~BinanceClient() = default;

bool BinanceClient::connect() {
    if (wsManager_->connect()) {
        connected_ = true;
        return true;
    }
    return false;
}

void BinanceClient::disconnect() {
    wsManager_->disconnect();
    connected_ = false;
}

bool BinanceClient::isConnected() const {
    return wsManager_->isConnected() && connected_;
}

void BinanceClient::subscribeOrderBook(const std::string& symbol, int limit) {
    if (!isConnected()) {
        if (errorCallback_) {
            errorCallback_("Cannot subscribe: not connected");
        }
        return;
    }
    
    std::string s = symbol;
    for (auto& c : s) c = (char)std::tolower(c);
    std::string stream;
    if (limit == 5 || limit == 10 || limit == 20) {
        stream = s + "@depth" + std::to_string(limit) + "@100ms";
    } else {
        stream = s + "@depth@100ms";
    }
    long long id = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream request;
    request << "{\"method\":\"SUBSCRIBE\",\"params\":[\"" << stream << "\"],\"id\":" << id << "}";
    wsManager_->send(request.str());
}

void BinanceClient::unsubscribeOrderBook(const std::string& symbol) {
    if (!isConnected()) {
        return;
    }
    
    std::string s = symbol;
    for (auto& c : s) c = (char)std::tolower(c);
    std::string stream = s + "@depth@100ms";
    long long id = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream request;
    request << "{\"method\":\"UNSUBSCRIBE\",\"params\":[\"" << stream << "\"],\"id\":" << id << "}";
    wsManager_->send(request.str());
}

void BinanceClient::requestDepthOnce(const std::string& symbol, int limit) {
    if (!isConnected()) {
        if (errorCallback_) { errorCallback_("Cannot request depth: not connected"); }
        return;
    }
    auto genHex = [](){ std::stringstream s; s<<std::hex<<std::setw(8)<<std::setfill('0')<< (uint32_t)std::rand(); return s.str(); };
    std::string uuid = genHex()+"-"+genHex()+"-"+genHex()+"-"+genHex()+"-"+genHex()+genHex();
    std::string up = symbol;
    for (auto &c : up) c = (char)std::toupper(c);
    std::ostringstream req;
    req << "{\"id\":\"" << uuid << "\",\"method\":\"depth\",\"params\":{\"symbol\":\"" << up << "\"}}";
    std::string payload = req.str();
    std::cout << "Sending request: " << payload << std::endl;
    wsManager_->send(payload);
}

void BinanceClient::setOrderBookCallback(OrderBookCallback callback) {
    orderBookCallback_ = callback;
}

void BinanceClient::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void BinanceClient::onMessage(const std::string& message) {
    try {
        std::cout << "Received message: " << message << std::endl;
        std::string payload = message;
        if (payload.find("\"stream\"") != std::string::npos && payload.find("\"data\"") != std::string::npos) {
            size_t pos = payload.find("\"data\"");
            if (pos != std::string::npos) {
                size_t brace = payload.find('{', pos);
                if (brace != std::string::npos) {
                    int depth = 0; size_t i = brace;
                    for (; i < payload.size(); ++i) {
                        if (payload[i] == '{') depth++;
                        else if (payload[i] == '}') { depth--; if (depth == 0) { ++i; break; } }
                    }
                    if (i > brace) {
                        payload = payload.substr(brace, i - brace);
                    }
                }
            }
        }
        if (payload.find("\"bids\"") != std::string::npos && payload.find("\"asks\"") != std::string::npos) {
            handleOrderBookUpdate(payload);
        } else if (message.find("\"result\"") != std::string::npos && message.find("\"status\"") != std::string::npos) {
            size_t pos = message.find("\"result\"");
            if (pos != std::string::npos) {
                size_t brace = message.find('{', pos);
                if (brace != std::string::npos) {
                    int depth = 0; size_t i = brace;
                    for (; i < message.size(); ++i) {
                        if (message[i] == '{') depth++;
                        else if (message[i] == '}') { depth--; if (depth == 0) { ++i; break; } }
                    }
                    if (i > brace) {
                        std::string resultObj = message.substr(brace, i - brace);
                        printDepthResult(message, resultObj);
                        return;
                    }
                }
            }
            std::cout << "Depth result response: " << message << std::endl;
        } else if (message.find("\"status\"") != std::string::npos) {
            std::cout << "Subscription response: " << message << std::endl;
        } else if (message.find("\"result\"") != std::string::npos && message.find("SUBSCRIBE") == std::string::npos) {
            std::cout << "Subscription ack: " << message << std::endl;
        } else {
            std::cout << "Unknown message type: " << message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
        std::cerr << "Message: " << message << std::endl;
    }
}

void BinanceClient::printDepthResult(const std::string& fullMessage, const std::string& resultObj) {
    std::smatch m;
    std::regex idStrRegex(R"REGEX("id"\s*:\s*"([^"]+)")REGEX");
    std::regex idNumRegex(R"REGEX("id"\s*:\s*(\d+))REGEX");
    std::regex statusRegex(R"REGEX("status"\s*:\s*(\d+))REGEX");
    std::string idVal;
    if (std::regex_search(fullMessage, m, idStrRegex)) {
        idVal = m[1].str();
    } else if (std::regex_search(fullMessage, m, idNumRegex)) {
        idVal = m[1].str();
    }
    int statusVal = 0;
    if (std::regex_search(fullMessage, m, statusRegex)) {
        statusVal = std::stoi(m[1].str());
    }
    std::smatch match;
    long long lastUpdateId = 0;
    long long eVal = 0;
    long long tVal = 0;
    std::regex lastUpdateIdRegex(R"REGEX("lastUpdateId"\s*:\s*(\d+))REGEX");
    std::regex eRegex(R"REGEX("E"\s*:\s*(\d+))REGEX");
    std::regex tRegex(R"REGEX("T"\s*:\s*(\d+))REGEX");
    if (std::regex_search(resultObj, match, lastUpdateIdRegex)) lastUpdateId = std::stoll(match[1].str());
    if (std::regex_search(resultObj, match, eRegex)) eVal = std::stoll(match[1].str());
    if (std::regex_search(resultObj, match, tRegex)) tVal = std::stoll(match[1].str());
    std::smatch mlist;
    std::string bidsList;
    std::string asksList;
    std::regex listRegexBids(R"REGEX("bids"\s*:\s*\[(.*)\])REGEX");
    std::regex listRegexAsks(R"REGEX("asks"\s*:\s*\[(.*)\])REGEX");
    if (std::regex_search(resultObj, mlist, listRegexBids)) bidsList = mlist[1].str();
    if (std::regex_search(resultObj, mlist, listRegexAsks)) asksList = mlist[1].str();
    std::regex pairRegex(R"REGEX(\[\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\])REGEX");
    std::vector<std::pair<std::string,std::string>> bidPairs;
    std::vector<std::pair<std::string,std::string>> askPairs;
    if (!bidsList.empty()) {
        auto b = std::sregex_iterator(bidsList.begin(), bidsList.end(), pairRegex);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            const auto& mm = *it; bidPairs.emplace_back(mm[1].str(), mm[2].str());
            if (bidPairs.size() >= (size_t)bookLimit_) break;
        }
    }
    if (!asksList.empty()) {
        auto b = std::sregex_iterator(asksList.begin(), asksList.end(), pairRegex);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            const auto& mm = *it; askPairs.emplace_back(mm[1].str(), mm[2].str());
            if (askPairs.size() >= (size_t)bookLimit_) break;
        }
    }
    std::cout << "{" << std::endl;
    std::cout << "  \"id\": \"" << idVal << "\"," << std::endl;
    std::cout << "  \"status\": " << statusVal << "," << std::endl;
    std::cout << "  \"result\": {" << std::endl;
    std::cout << "    \"lastUpdateId\": " << lastUpdateId << "," << std::endl;
    std::cout << "    \"E\": " << eVal << "," << std::endl;
    std::cout << "    \"T\": " << tVal << "," << std::endl;
    std::cout << "    \"bids\": [" << std::endl;
    for (size_t i = 0; i < bidPairs.size(); ++i) {
        std::cout << "      [\"" << bidPairs[i].first << "\", \"" << bidPairs[i].second << "\"]";
        if (i + 1 < bidPairs.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "    ]," << std::endl;
    std::cout << "    \"asks\": [" << std::endl;
    for (size_t i = 0; i < askPairs.size(); ++i) {
        std::cout << "      [\"" << askPairs[i].first << "\", \"" << askPairs[i].second << "\"]";
        if (i + 1 < askPairs.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "    ]" << std::endl;
    std::cout << "  }" << std::endl;
    std::cout << "}" << std::endl;
}

void BinanceClient::onError(const std::string& error) {
    std::cerr << "BinanceClient error: " << error << std::endl;
    if (errorCallback_) {
        errorCallback_(error);
    }
}

void BinanceClient::handleOrderBookUpdate(const std::string& jsonData) {
    try {
        OrderBookData orderBook;
        orderBook.receiveTime = std::chrono::system_clock::now();
        orderBook.symbol = config_.symbol;
        
        std::regex lastUpdateIdRegex(R"REGEX("lastUpdateId":(\d+))REGEX");
        std::smatch match;
        if (std::regex_search(jsonData, match, lastUpdateIdRegex)) {
            orderBook.lastUpdateId = std::stoll(match[1].str());
        }
        
        std::regex eRegex(R"REGEX("E":(\d+))REGEX");
        if (std::regex_search(jsonData, match, eRegex)) {
            orderBook.messageTime = std::stoll(match[1].str());
        }
        
        std::regex tRegex(R"REGEX("T":(\d+))REGEX");
        if (std::regex_search(jsonData, match, tRegex)) {
            orderBook.transactionTime = std::stoll(match[1].str());
        }
        
        std::smatch mlist;
        std::regex listRegexBids(R"REGEX("bids"\s*:\s*\[(.*)\])REGEX");
        std::regex listRegexAsks(R"REGEX("asks"\s*:\s*\[(.*)\])REGEX");
        std::regex listRegexB(R"REGEX("b"\s*:\s*\[(.*)\])REGEX");
        std::regex listRegexA(R"REGEX("a"\s*:\s*\[(.*)\])REGEX");
        std::string bidsList;
        std::string asksList;
        if (std::regex_search(jsonData, mlist, listRegexBids)) bidsList = mlist[1].str();
        if (std::regex_search(jsonData, mlist, listRegexAsks)) asksList = mlist[1].str();
        if (bidsList.empty() && std::regex_search(jsonData, mlist, listRegexB)) bidsList = mlist[1].str();
        if (asksList.empty() && std::regex_search(jsonData, mlist, listRegexA)) asksList = mlist[1].str();
        applyPairs(bidsList, true);
        applyPairs(asksList, false);
        buildTop(orderBook);
        
        // Calculate and display latency
        auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        double latencyMs = currentTime - orderBook.messageTime;
        
        std::cout << "OrderBook Update - Symbol: " << config_.symbol 
                  << ", Bids: " << orderBook.bids.size() 
                  << ", Asks: " << orderBook.asks.size()
                  << ", Latency: " << std::fixed << std::setprecision(2) << latencyMs << "ms"
                  << std::endl;
        
        if (orderBookCallback_) {
            orderBookCallback_(orderBook);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling order book update: " << e.what() << std::endl;
    }
}

void BinanceClient::applyPairs(const std::string& src, bool isBid) {
    if (src.empty()) return;
    std::regex pairRegex(R"REGEX(\[\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\])REGEX");
    auto begin = std::sregex_iterator(src.begin(), src.end(), pairRegex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto& mm = *it;
        double p = 0.0, q = 0.0;
        try { p = std::stod(mm[1].str()); q = std::stod(mm[2].str()); } catch (...) { continue; }
        if (isBid) {
            if (q == 0.0) bidMap_.erase(p); else bidMap_[p] = q;
        } else {
            if (q == 0.0) askMap_.erase(p); else askMap_[p] = q;
        }
    }
}

void BinanceClient::buildTop(OrderBookData& out) {
    out.bids.clear();
    out.asks.clear();
    size_t bc = 0;
    for (auto it = bidMap_.begin(); it != bidMap_.end() && bc < (size_t)bookLimit_; ++it) {
        out.bids.emplace_back(it->first, it->second);
        ++bc;
    }
    size_t ac = 0;
    for (auto it = askMap_.begin(); it != askMap_.end() && ac < (size_t)bookLimit_; ++it) {
        out.asks.emplace_back(it->first, it->second);
        ++ac;
    }
}

} // namespace orderbook
