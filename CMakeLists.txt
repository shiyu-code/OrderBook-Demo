cmake_minimum_required(VERSION 3.16)
project(BinanceOrderBookDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add external dependencies via FetchContent
include(FetchContent)

# websocketpp (header-only)
FetchContent_Declare(
  websocketpp
  GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
  GIT_TAG        0.8.2
)
FetchContent_MakeAvailable(websocketpp)

# OpenSSL
find_package(OpenSSL REQUIRED)

# Standalone Asio (header-only)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${websocketpp_SOURCE_DIR})
include_directories(${asio_SOURCE_DIR}/asio/include)
add_definitions(-DASIO_STANDALONE)
add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
add_definitions(-DWEBSOCKETPP_USE_ASIO)
add_definitions(-D_WEBSOCKETPP_CPP11_THREAD_)

# Source files
set(SOURCES
    src/main.cpp
    src/binance_client.cpp
    src/simple_websocket_client.cpp
    src/websocket_manager.cpp
    src/latency_analyzer.cpp
    src/optimization_strategies.cpp
)

# Header files
set(HEADERS
    include/binance_client.h
    include/simple_websocket_client.h
    include/latency_analyzer.h
    include/optimization_strategies.h
    include/order_book_types.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compiler specific flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()