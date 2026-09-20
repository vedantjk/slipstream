cmake_minimum_required(VERSION 3.20)
project(slipstream)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
        GIT_SHALLOW TRUE
)

# Mainly needed on Windows; harmless elsewhere.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(slipstream main.cpp)
add_executable(market_data_client market_data_client.cpp
        lib/parser.h)

enable_testing()
add_executable(parser_tests tests/parser_tests.cpp lib/parser.h)

target_include_directories(
        parser_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        parser_tests
        PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(parser_tests)