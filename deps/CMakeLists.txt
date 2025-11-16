# Dependencies CMakeLists.txt

# WebSocket++
include(FetchContent)

FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

# Make available
FetchContent_MakeAvailable(websocketpp json)

# Boost libraries (assuming system installation)
find_package(Boost REQUIRED COMPONENTS system thread chrono)