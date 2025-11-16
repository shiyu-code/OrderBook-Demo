#include "websocket_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace orderbook {

WebSocketManager::WebSocketManager(const WebSocketConfig& config) 
    : config_(config), connected_(false), shouldStop_(false), pingPongEnabled_(false), pingIntervalMs_(30000) {
    
    // Initialize WebSocket++ client
    wsClient_.clear_access_channels(websocketpp::log::alevel::all);
    wsClient_.set_access_channels(websocketpp::log::alevel::connect);
    wsClient_.set_access_channels(websocketpp::log::alevel::disconnect);
    wsClient_.set_access_channels(websocketpp::log::alevel::fail);
    
    // Initialize ASIO
    wsClient_.init_asio();
    wsClient_.start_perpetual();
    wsClient_.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
        namespace asio = websocketpp::lib::asio;
        websocketpp::lib::shared_ptr<asio::ssl::context> ctx(new asio::ssl::context(asio::ssl::context::tls_client));
        ctx->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);
        ctx->set_verify_mode(asio::ssl::verify_none);
        SSL_CTX_set_cipher_list(ctx->native_handle(), "DEFAULT:@SECLEVEL=1");
        SSL_CTX_set_min_proto_version(ctx->native_handle(), TLS1_2_VERSION);
        SSL_CTX_set_default_verify_paths(ctx->native_handle());
        return ctx;
    });

    wsClient_.set_socket_init_handler([this](websocketpp::connection_hdl hdl, client::connection_type::socket_type& s) {
        try {
            client::connection_ptr con = wsClient_.get_con_from_hdl(hdl);
            std::string host = config_.overrideHost;
            if (host.empty()) {
                auto uri = con->get_uri();
                host = uri ? uri->get_host() : "";
            }
            if (!host.empty()) {
                SSL* ssl = s.native_handle();
                if (ssl) {
                    SSL_set_tlsext_host_name(ssl, host.c_str());
                }
            }
        } catch (...) {}
    });
    
    // Set handlers
    wsClient_.set_open_handler(websocketpp::lib::bind(&WebSocketManager::onOpen, this, websocketpp::lib::placeholders::_1));
    wsClient_.set_close_handler(websocketpp::lib::bind(&WebSocketManager::onClose, this, websocketpp::lib::placeholders::_1));
    wsClient_.set_message_handler(websocketpp::lib::bind(&WebSocketManager::onMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
    wsClient_.set_fail_handler(websocketpp::lib::bind(&WebSocketManager::onFail, this, websocketpp::lib::placeholders::_1));
}

WebSocketManager::~WebSocketManager() {
    disconnect();
    wsClient_.stop_perpetual();
    
    if (ioThread_.joinable()) {
        ioThread_.join();
    }
    if (pingThread_.joinable()) {
        shouldStop_ = true;
        cv_.notify_all();
        pingThread_.join();
    }
}

bool WebSocketManager::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        return true;
    }
    
    websocketpp::lib::error_code ec;
    client::connection_ptr con = wsClient_.get_connection(config_.uri, ec);
    
    if (ec) {
        std::string error = "Connection initialization error: " + ec.message();
        if (errorCallback_) {
            errorCallback_(error);
        }
        return false;
    }
    if (!config_.overrideHost.empty()) {
        con->replace_header("Host", config_.overrideHost);
    }
    
    // Apply proxy if provided
    if (!config_.proxyUri.empty()) {
        websocketpp::lib::error_code pec;
        con->set_proxy(config_.proxyUri, pec);
        if (pec) {
            std::cerr << "Proxy set error: " << pec.message() << std::endl;
        } else {
            std::cout << "Using proxy: " << config_.proxyUri << std::endl;
        }
    }

    // Apply optimizations
    if (config_.enableCompression) {
        con->add_subprotocol("permessage-deflate");
    }
    
    connectionHandle_ = con->get_handle();
    wsClient_.connect(con);
    
    // Start IO thread
    ioThread_ = std::thread([this]() {
        wsClient_.run();
    });
    
    // Wait for connection
    std::unique_lock<std::mutex> cvLock(mutex_);
    cv_.wait_for(cvLock, std::chrono::seconds(10), [this] { return connected_ || shouldStop_; });
    
    return connected_;
}

void WebSocketManager::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return;
    }
    
    shouldStop_ = true;
    cv_.notify_all();
    
    websocketpp::lib::error_code ec;
    wsClient_.close(connectionHandle_, websocketpp::close::status::going_away, "Client disconnecting", ec);
    
    if (ec) {
        std::cerr << "Error closing connection: " << ec.message() << std::endl;
    }
    
    connected_ = false;
}

bool WebSocketManager::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

void WebSocketManager::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        if (errorCallback_) {
            errorCallback_("Cannot send message: not connected");
        }
        return;
    }
    
    websocketpp::lib::error_code ec;
    wsClient_.send(connectionHandle_, message, websocketpp::frame::opcode::text, ec);
    
    if (ec) {
        std::string error = "Send error: " + ec.message();
        if (errorCallback_) {
            errorCallback_(error);
        }
    }
}

void WebSocketManager::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void WebSocketManager::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void WebSocketManager::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

void WebSocketManager::enableCompression(bool enable) {
    config_.enableCompression = enable;
}

void WebSocketManager::enablePingPong(bool enable, int intervalMs) {
    pingPongEnabled_ = enable;
    pingIntervalMs_ = intervalMs;
    
    if (enable && connected_ && !pingThread_.joinable()) {
        pingThread_ = std::thread(&WebSocketManager::pingPongLoop, this);
    }
}

void WebSocketManager::setBufferSize(int size) {
    config_.bufferSize = size;
}

void WebSocketManager::onOpen(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = true;
    
    std::cout << "WebSocket connection opened" << std::endl;
    
    if (connectionCallback_) {
        connectionCallback_(true);
    }
    
    cv_.notify_all();
    
    // Start ping-pong if enabled
    if (pingPongEnabled_ && !pingThread_.joinable()) {
        pingThread_ = std::thread(&WebSocketManager::pingPongLoop, this);
    }
}

void WebSocketManager::onClose(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    
    std::cout << "WebSocket connection closed" << std::endl;
    
    if (connectionCallback_) {
        connectionCallback_(false);
    }
}

void WebSocketManager::onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    if (messageCallback_) {
        messageCallback_(msg->get_payload());
    }
}

void WebSocketManager::onFail(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    
    client::connection_ptr con = wsClient_.get_con_from_hdl(hdl);
    std::ostringstream oss;
    oss << "WebSocket connection failed: ec='" << (con ? con->get_ec().message() : "unknown") << "'";
    if (con) {
        oss << ", code=" << con->get_response_code() << ", msg='" << con->get_response_msg() << "'";
        if (con->get_uri()) {
            oss << ", uri='" << con->get_uri()->str() << "'";
        }
    }
    std::string error = oss.str();
    std::cout << error << std::endl;
    if (errorCallback_) { errorCallback_(error); }
    
    cv_.notify_all();
}

void WebSocketManager::pingPongLoop() {
    while (!shouldStop_ && connected_) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (cv_.wait_for(lock, std::chrono::milliseconds(pingIntervalMs_), 
                         [this] { return shouldStop_ || !connected_; })) {
            break;
        }
        
        if (connected_ && !shouldStop_) {
            try {
                websocketpp::lib::error_code ec;
                wsClient_.ping(connectionHandle_, "ping");
                
                if (ec) {
                    std::cerr << "Ping error: " << ec.message() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Ping exception: " << e.what() << std::endl;
            }
        }
    }
}

} // namespace orderbook