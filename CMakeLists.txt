cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")

project(trip)

include_directories("src/")

file(GLOB PROTOCOL_SRC "src/protocol/*.c")
file(GLOB FUNCTIONS_SRC "src/functions/*.c")
file(GLOB TRIPD_SRC "src/tripd/*.c")


add_library(protocol STATIC ${PROTOCOL_SRC})

add_library(functions STATIC ${FUNCTIONS_SRC})
target_link_libraries(functions protocol)

add_executable(tripd ${TRIPD_SRC})
target_link_libraries(tripd functions protocol)

