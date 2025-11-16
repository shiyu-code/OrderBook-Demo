#include "simple_websocket_client.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace orderbook {

SimpleWebSocketClient::SimpleWebSocketClient(const WebSocketConfig& config)
    : config_(config), socket_(-1), connected_(false), shouldStop_(false), 
      pingPongEnabled_(false), pingIntervalMs_(30000) {
    
    // Parse URI
    std::string uri = config.uri;
    if (uri.find("wss://") == 0) {
        uri = uri.substr(6);
        host_ = uri.substr(0, uri.find('/'));
        path_ = uri.substr(uri.find('/'));
        if (path_.empty()) path_ = "/";
        port_ = 443;
    } else if (uri.find("ws://") == 0) {
        uri = uri.substr(5);
        host_ = uri.substr(0, uri.find('/'));
        path_ = uri.substr(uri.find('/'));
        if (path_.empty()) path_ = "/";
        port_ = 80;
    } else {
        host_ = uri.substr(0, uri.find('/'));
        path_ = uri.substr(uri.find('/'));
        if (path_.empty()) path_ = "/";
        port_ = 443;
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

SimpleWebSocketClient::~SimpleWebSocketClient() {
    disconnect();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool SimpleWebSocketClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        return true;
    }
    
    

    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        if (errorCallback_) {
            errorCallback_("Failed to create socket");
        }
        return false;
    }
    
    // Resolve hostname
    struct hostent* host = gethostbyname(host_.c_str());
    if (!host) {
        if (errorCallback_) {
            errorCallback_("Failed to resolve hostname: " + host_);
        }
        return false;
    }
    
    // Connect
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    memcpy(&serverAddr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (::connect(socket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        if (errorCallback_) {
            errorCallback_("Failed to connect to server");
        }
        return false;
    }
    
    useTLS_ = (port_ == 443);
    if (useTLS_) {
        if (!initTLS()) {
            if (errorCallback_) errorCallback_("Failed to initialize TLS");
            return false;
        }
    }
    if (!performWebSocketHandshake()) {
        return false;
    }
    
    connected_ = true;
    
    // Start threads
    ioThread_ = std::thread(&SimpleWebSocketClient::ioThread, this);
    messageThread_ = std::thread(&SimpleWebSocketClient::messageThread, this);
    
    if (connectionCallback_) {
        connectionCallback_(true);
    }
    
    return true;
}

void SimpleWebSocketClient::disconnect() {
    shouldStop_ = true;
    cv_.notify_all();
    
    if (socket_ >= 0) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = -1;
    }
    
    if (ioThread_.joinable()) {
        ioThread_.join();
    }
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
    if (pingThread_.joinable()) {
        pingThread_.join();
    }
    cleanupTLS();
    

    connected_ = false;
    
    if (connectionCallback_) {
        connectionCallback_(false);
    }
}

bool SimpleWebSocketClient::isConnected() const {
    return connected_;
}

void SimpleWebSocketClient::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    sendQueue_.push(message);
    cv_.notify_all();
}

void SimpleWebSocketClient::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void SimpleWebSocketClient::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void SimpleWebSocketClient::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

void SimpleWebSocketClient::enableCompression(bool enable) {
    config_.enableCompression = enable;
}

void SimpleWebSocketClient::enablePingPong(bool enable, int intervalMs) {
    pingPongEnabled_ = enable;
    pingIntervalMs_ = intervalMs;
    
    if (enable && connected_ && !pingThread_.joinable()) {
        pingThread_ = std::thread(&SimpleWebSocketClient::pingThread, this);
    }
}

void SimpleWebSocketClient::setBufferSize(int size) {
    config_.bufferSize = size;
}

bool SimpleWebSocketClient::performWebSocketHandshake() {
    std::string key = generateWebSocketKey();
    
    std::ostringstream request;
    request << "GET " << path_ << " HTTP/1.1\r\n";
    request << "Host: " << host_ << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << key << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    request << "\r\n";
    
    std::string requestStr = request.str();
    int sendRes;
    if (useTLS_) {
        sendRes = SSL_write((SSL*)ssl_, requestStr.c_str(), (int)requestStr.length());
    } else {
        sendRes = ::send(socket_, requestStr.c_str(), requestStr.length(), 0);
    }
    if (sendRes <= 0) {
        if (errorCallback_) {
            errorCallback_("Failed to send handshake request");
        }
        return false;
    }
    
    // Read response
    char buffer[2048];
    int bytesRead;
    if (useTLS_) {
        bytesRead = SSL_read((SSL*)ssl_, buffer, sizeof(buffer) - 1);
    } else {
        bytesRead = ::recv(socket_, buffer, sizeof(buffer) - 1, 0);
    }
    if (bytesRead <= 0) {
        return false;
    }
    
    buffer[bytesRead] = '\0';
    std::string response(buffer);
    
    // Check response
    if (response.find("101 Switching Protocols") == std::string::npos) {
        return false;
    }
    
    return true;
}

std::string SimpleWebSocketClient::generateWebSocketKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string key;
    for (int i = 0; i < 16; ++i) {
        key += static_cast<char>(dis(gen));
    }
    
    return base64Encode(key);
}

std::string SimpleWebSocketClient::base64Encode(const std::string& input) {
    // Simple base64 encoding for demo purposes
    const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (size_t idx = 0; idx < input.length(); ++idx) {
        char_array_3[i++] = input[idx];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++) {
                encoded += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++) {
            encoded += base64_chars[char_array_4[j]];
        }
        
        while(i++ < 3) {
            encoded += '=';
        }
    }
    
    return encoded;
}

void SimpleWebSocketClient::ioThread() {
    std::vector<unsigned char> rbuf(65536);
    while (!shouldStop_ && connected_) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!sendQueue_.empty()) {
                std::string message = sendQueue_.front();
                sendQueue_.pop();
                std::string frame = createWebSocketFrame(message, 0x1);
                if (useTLS_) {
                    SSL_write((SSL*)ssl_, frame.data(), (int)frame.size());
                } else {
                    ::send(socket_, frame.data(), (int)frame.size(), 0);
                }
            }
        }
        int n = 0;
        if (useTLS_) {
            n = SSL_read((SSL*)ssl_, rbuf.data(), (int)rbuf.size());
        } else {
            n = ::recv(socket_, (char*)rbuf.data(), (int)rbuf.size(), 0);
        }
        if (n > 0) {
            const unsigned char* p = rbuf.data();
            size_t len = (size_t)n;
            if (len >= 2) {
                bool fin = (p[0] & 0x80) != 0;
                unsigned char opcode = (p[0] & 0x0F);
                bool masked = (p[1] & 0x80) != 0;
                uint64_t payload_len = (p[1] & 0x7F);
                size_t hdr = 2;
                if (payload_len == 126 && len >= 4) {
                    payload_len = (p[2] << 8) | p[3];
                    hdr = 4;
                } else if (payload_len == 127 && len >= 10) {
                    payload_len = 0;
                    for (int i = 0; i < 8; ++i) payload_len = (payload_len << 8) | p[2 + i];
                    hdr = 10;
                }
                unsigned char mask[4] = {0,0,0,0};
                if (masked && len >= hdr + 4) {
                    mask[0] = p[hdr+0]; mask[1] = p[hdr+1]; mask[2] = p[hdr+2]; mask[3] = p[hdr+3];
                    hdr += 4;
                }
                if (len >= hdr + payload_len) {
                    std::string msg;
                    msg.resize((size_t)payload_len);
                    for (size_t i = 0; i < payload_len; ++i) {
                        unsigned char c = p[hdr + i];
                        if (masked) c = c ^ mask[i % 4];
                        msg[i] = (char)c;
                    }
                    if (opcode == 0x1 && messageCallback_) {
                        messageCallback_(msg);
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void SimpleWebSocketClient::messageThread() {
    // This thread would normally process incoming messages
    // For demo purposes, we'll just keep it alive
    while (!shouldStop_ && connected_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SimpleWebSocketClient::pingThread() {
    while (!shouldStop_ && connected_ && pingPongEnabled_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pingIntervalMs_));
        
        if (!shouldStop_ && connected_) {
            std::cout << "[WebSocket] Sending ping" << std::endl;
            // In a real implementation, we would send a WebSocket ping frame here
        }
    }
}

 
static uint32_t rand32() {
    static std::mt19937 rng((uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return rng();
}

std::string SimpleWebSocketClient::createWebSocketFrame(const std::string& data, int opcode) {
    std::string out;
    unsigned char b1 = 0x80 | (opcode & 0x0F);
    out.push_back((char)b1);
    size_t len = data.size();
    unsigned char b2 = 0x80;
    if (len <= 125) {
        b2 |= (unsigned char)len;
        out.push_back((char)b2);
    } else if (len <= 65535) {
        b2 |= 126;
        out.push_back((char)b2);
        out.push_back((char)((len >> 8) & 0xFF));
        out.push_back((char)(len & 0xFF));
    } else {
        b2 |= 127;
        out.push_back((char)b2);
        for (int i = 7; i >= 0; --i) out.push_back((char)((len >> (8*i)) & 0xFF));
    }
    uint32_t m = rand32();
    unsigned char mask[4] = { (unsigned char)(m>>24), (unsigned char)(m>>16), (unsigned char)(m>>8), (unsigned char)m };
    out.append((char*)mask, 4);
    for (size_t i = 0; i < len; ++i) {
        out.push_back((char)(data[i] ^ mask[i % 4]));
    }
    return out;
}

bool SimpleWebSocketClient::initTLS() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    const SSL_METHOD* method = TLS_client_method();
    sslCtx_ = SSL_CTX_new(method);
    if (!sslCtx_) return false;
    SSL_CTX_set_verify((SSL_CTX*)sslCtx_, SSL_VERIFY_NONE, NULL);
    ssl_ = SSL_new((SSL_CTX*)sslCtx_);
    if (!ssl_) return false;
    SSL_set_tlsext_host_name((SSL*)ssl_, host_.c_str());
    SSL_set_fd((SSL*)ssl_, socket_);
    if (SSL_connect((SSL*)ssl_) != 1) {
        unsigned long e = ERR_get_error();
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        if (errorCallback_) errorCallback_(std::string("TLS connect failed: ") + buf);
        return false;
    }
    return true;
}

void SimpleWebSocketClient::cleanupTLS() {
    if (ssl_) { SSL_shutdown((SSL*)ssl_); SSL_free((SSL*)ssl_); ssl_ = nullptr; }
    if (sslCtx_) { SSL_CTX_free((SSL_CTX*)sslCtx_); sslCtx_ = nullptr; }
    EVP_cleanup();
}

} // namespace orderbook