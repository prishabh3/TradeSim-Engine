# Tests CMakeLists.txt — minimal, no external test framework needed

cmake_minimum_required(VERSION 3.16)

# Helper macro: create a test executable that links against the trading lib
macro(add_trading_test name)
    # Gather all shared sources EXCEPT main.cpp
    file(GLOB_RECURSE SHARED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/common/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/strategy/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/portfolio/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/market/*.cpp"
    )

    add_executable(${name} ${name}.cpp ${SHARED_SOURCES})

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${JSONCPP_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
    )

    target_link_libraries(${name} PRIVATE
        CURL::libcurl
        ${JSONCPP_LIBRARIES}
        Threads::Threads
    )

    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_trading_test(test_thread_safe_queue)
add_trading_test(test_indicators)
add_trading_test(test_order_book)
add_trading_test(test_matching_engine)
