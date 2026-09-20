cmake_minimum_required(VERSION 3.20)
project(slipstream)

set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(slipstream main.cpp)
add_executable(market_data_client market_data_client.cpp)
