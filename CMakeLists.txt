cmake_minimum_required(VERSION 3.16)
project(RealTimeTradingSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    # AddressSanitizer (optional — comment out if linker issues occur on macOS)
    # add_compile_options(-fsanitize=address)
    # add_link_options(-fsanitize=address)
endif()

# ── Find dependencies ─────────────────────────────────────────────────────────
find_package(CURL REQUIRED)

# JsonCpp — try pkg-config first, then fallback
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(JSONCPP jsoncpp)
endif()

if(NOT JSONCPP_FOUND)
    # Homebrew path on macOS
    find_path(JSONCPP_INCLUDE_DIRS json/json.h
        PATHS /usr/local/include /opt/homebrew/include
    )
    find_library(JSONCPP_LIBRARIES
        NAMES jsoncpp
        PATHS /usr/local/lib /opt/homebrew/lib
    )
    if(JSONCPP_INCLUDE_DIRS AND JSONCPP_LIBRARIES)
        set(JSONCPP_FOUND TRUE)
    else()
        message(FATAL_ERROR
            "JsonCpp not found. Install with:\n"
            "  macOS:  brew install jsoncpp\n"
            "  Ubuntu: sudo apt-get install libjsoncpp-dev")
    endif()
endif()

# Threads (pthreads)
find_package(Threads REQUIRED)

# ── Source files ──────────────────────────────────────────────────────────────
file(GLOB_RECURSE TRADING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Explicitly add API server (glob may miss new subdirs on first configure)
list(APPEND TRADING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/api/ApiServer.cpp"
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(trading_system ${TRADING_SOURCES})

target_include_directories(trading_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/api
    ${JSONCPP_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(trading_system PRIVATE
    CURL::libcurl
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

# httplib auto-detects brotli/OpenSSL/zlib from Homebrew headers — link them all.
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(trading_system PRIVATE
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    /opt/homebrew/lib/libbrotlienc.dylib
    /opt/homebrew/lib/libbrotlidec.dylib
    /opt/homebrew/lib/libbrotlicommon.dylib
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

# ── Print summary ─────────────────────────────────────────────────────────────
message(STATUS "Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "CURL include   : ${CURL_INCLUDE_DIRS}")
message(STATUS "JSONCPP include: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP libs   : ${JSONCPP_LIBRARIES}")
