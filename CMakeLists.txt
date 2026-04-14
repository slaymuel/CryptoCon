cmake_minimum_required(VERSION 3.20)
project(trade_connector VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable flto
option(TC_ENABLE_LTO "Enable link-time optimization" OFF)
if(TC_ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

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
FetchContent_GetProperties(ixwebsocket)
if(NOT ixwebsocket_POPULATED)
    FetchContent_Populate(ixwebsocket)
    add_subdirectory(${ixwebsocket_SOURCE_DIR} ${ixwebsocket_BINARY_DIR})
endif()

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
    simdjson
)

target_link_libraries(trade_connector PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ixwebsocket
)

target_compile_features(trade_connector PUBLIC cxx_std_20)

# --- Example executable (optional) ---

option(TC_BUILD_EXAMPLES "Build example programs" OFF)
if(TC_BUILD_EXAMPLES)
    add_executable(binance_trade_feed_mixin examples/binance_trade_feed_mixin.cpp)
    target_link_libraries(binance_trade_feed_mixin PRIVATE trade_connector)
endif()

# --- Benchmarks (optional) ---

option(TC_BUILD_BENCHMARKS "Build benchmarks" OFF)
if(TC_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_sendorder benchmarks/bench_sendorder.cpp)
    target_link_libraries(bench_sendorder PRIVATE trade_connector benchmark::benchmark)
endif()

# --- Installation ---

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS trade_connector
    EXPORT trade_connectorTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers preserving directory structure
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/trade_connector
    FILES_MATCHING PATTERN "*.h"
)

# Install the exported targets file
install(EXPORT trade_connectorTargets
    FILE trade_connectorTargets.cmake
    NAMESPACE trade_connector::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trade_connector
)

# Generate and install the package config files
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/trade_connectorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/trade_connectorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trade_connector
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/trade_connectorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/trade_connectorConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/trade_connectorConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trade_connector
)
