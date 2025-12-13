cmake_minimum_required(VERSION 3.20)

project(trip)

file(GLOB PROTOCOL_SRC "src/protocol/*.c")

add_library(protocol STATIC ${PROTOCOL_SRC})

