cmake_minimum_required(VERSION 3.19)
project(CPS4042_USART LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)

add_executable(CPS4042_USART
    main_part2.cpp
)

target_include_directories(CPS4042_USART
    PRIVATE
        ../part1_i2c/include
        ${Boost_INCLUDE_DIRS}
)
