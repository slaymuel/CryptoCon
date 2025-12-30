cmake_minimum_required(VERSION 3.16)
project(trade_connector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(DEBUG_BUILD)

    add_compile_options(
      -g 
      -O0
      -fsanitize=address
      -fno-omit-frame-pointer
      -fsanitize=undefined
      -fsanitize=leak
    )
    add_link_options(
      -fsanitize=address
      -fsanitize=undefined
      -fsanitize=leak
    )
    message(STATUS "Debug build - all logs enabled")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_definitions(NDEBUG)
    add_compile_definitions(QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_WARNING)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    # set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    # -march=native               # CPU-specific optimizations
    # -flto                       # Link-time optimization
    # -ffast-math                 # Aggressive math optimizations
    message(STATUS "Release build - only warnings and above")
endif()

# Find Boost (Beast is header-only, but System is required)
find_package(Boost REQUIRED COMPONENTS system)

# Find OpenSSL for HTTPS
find_package(OpenSSL REQUIRED)
find_package(simdjson REQUIRED)

# Fetch uWebSockets and its dependencies
include(FetchContent)

# Fetch ixwebsocket
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)

# Enable SSL/TLS support for ixwebsocket
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ixwebsocket)

add_executable(trade_connector
    src/main.cpp
)

target_include_directories(trade_connector PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${uwebsockets_SOURCE_DIR}/src
)

target_link_libraries(trade_connector PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    simdjson::simdjson
    ixwebsocket
)