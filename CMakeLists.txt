cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")

project(trip)

include_directories("src/")

file(GLOB PROTOCOL_SRC "src/protocol/*.c")
file(GLOB FUNCTIONS_SRC "src/functions/*.c")
file(GLOB COMMAND_SRC "src/command/*.c")
file(GLOB LOGGING_SRC "src/logging/*.c")
file(GLOB UTIL_SRC "src/util/*.c")
file(GLOB TRIPD_SRC "src/tripd/*.c")

add_library(logging STATIC ${LOGGING_SRC})

add_library(util STATIC ${UTIL_SRC})

add_library(protocol STATIC ${PROTOCOL_SRC})
target_link_libraries(protocol logging)

add_library(functions STATIC ${FUNCTIONS_SRC})
target_link_libraries(functions protocol logging util)

add_library(command STATIC ${COMMAND_SRC})
target_link_libraries(command functions logging)


add_executable(tripd ${TRIPD_SRC})
target_link_libraries(tripd command)


find_program(DOXYGEN doxygen)
if (DOXYGEN)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(documentation ALL
        COMMAND ${DOXYGEN} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM)
else (DOXYGEN_FOUND)
    message("Doxygen needed for documentation")
endif()

