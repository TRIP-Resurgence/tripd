cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")

project(trip)

file(GLOB PROTOCOL_SRC "src/protocol/*.c")

add_library(protocol STATIC ${PROTOCOL_SRC})

