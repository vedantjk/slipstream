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

add_library(slipstream_codec STATIC lib/codec.cpp)
target_include_directories(slipstream_codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(slipstream_codec PUBLIC cxx_std_23)

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

add_executable(wire_tests tests/wire_tests.cpp lib/wire.h)

target_include_directories(
        wire_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        wire_tests
        PRIVATE GTest::gtest_main
)

gtest_discover_tests(wire_tests)

add_executable(codec_tests
        tests/codec_tests.cpp
        tests/reassembler_tests.cpp
        lib/codec.h
        lib/reassembler.h)

target_include_directories(
        codec_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        codec_tests
        PRIVATE slipstream_codec GTest::gtest_main
)

gtest_discover_tests(codec_tests)

add_executable(replay_tests tests/replay_tests.cpp lib/replay.h)
target_include_directories(replay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(replay_tests PRIVATE GTest::gtest_main)
gtest_discover_tests(replay_tests)
