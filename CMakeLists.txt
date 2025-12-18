cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")

project(trip)

include_directories("src/")

file(GLOB PROTOCOL_SRC "src/protocol/*.c")
file(GLOB LSFUNCTIONS_SRC "src/lsfunctions/*.c")


add_library(protocol STATIC ${PROTOCOL_SRC})

add_library(lsfunctions STATIC ${LSFUNCTIONS_SRC})
target_link_libraries(lsfunctions protocol)

