cmake_minimum_required(VERSION 3.20)
project(trade_connector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- External dependencies ---

# Boost (headers-only for Beast/Asio)
find_package(Boost REQUIRED COMPONENTS system)

# OpenSSL
find_package(OpenSSL REQUIRED)

# zlib (needed by ixwebsocket)
find_package(ZLIB REQUIRED)

# simdjson
include(FetchContent)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG        v3.12.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(simdjson)

# IXWebSocket
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)
set(USE_ZLIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ixwebsocket)

# --- trade_connector static library ---

add_library(trade_connector STATIC
    # Add .cpp files here as you create them, e.g.:
    src/websocket/client.cpp
    src/rest/client.cpp
    src/utils/signing.cpp
    # src/websocket/client.cpp
)

target_include_directories(trade_connector PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(trade_connector PUBLIC
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    simdjson
    ixwebsocket
)

target_compile_features(trade_connector PUBLIC cxx_std_20)

# --- Example executable (optional) ---

option(TC_BUILD_EXAMPLES "Build example programs" OFF)
if(TC_BUILD_EXAMPLES)
    add_executable(kraken_example examples/kraken_websocket.cpp)
    target_link_libraries(kraken_example PRIVATE trade_connector)
endif()

# --- Benchmarks (optional) ---

option(TC_BUILD_BENCHMARKS "Build benchmarks" OFF)
if(TC_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_sendorder benchmarks/bench_sendorder.cpp)
    target_link_libraries(bench_sendorder PRIVATE trade_connector benchmark::benchmark)
endif()
