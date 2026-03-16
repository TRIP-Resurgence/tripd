cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(trip)

include_directories("src/")

file(GLOB PROTOCOL_SRC "src/protocol/*.c")
file(GLOB FUNCTIONS_SRC "src/functions/*.c")
file(GLOB TRIB_SRC "src/trib/*.c")
file(GLOB COMMAND_SRC "src/command/*.c")
file(GLOB LOGGING_SRC "src/logging/*.c")
file(GLOB UTIL_SRC "src/util/*.c")
file(GLOB TRIPD_SRC "src/tripd/*.c")

add_compile_options(-Wall -Wextra -Wno-pedantic -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -coverage)
add_link_options(-coverage)

# components

add_library(logging STATIC ${LOGGING_SRC})

add_library(util STATIC ${UTIL_SRC})

add_library(protocol STATIC ${PROTOCOL_SRC})
target_link_libraries(protocol logging)

add_library(trib STATIC ${TRIB_SRC})
target_link_libraries(trib logging)

add_library(functions STATIC ${FUNCTIONS_SRC})
target_link_libraries(functions trib protocol logging util)

add_library(command STATIC ${COMMAND_SRC})
target_link_libraries(command trib functions trib logging)

# main executable

add_executable(tripd ${TRIPD_SRC})
target_link_libraries(tripd command)


# tests

add_executable(test_protocol_types_sizes "src/tests/protocol_types_sizes.c")
target_link_libraries(test_protocol_types_sizes logging command)
add_test(NAME test_protocol_types_sizes COMMAND test_protocol_types_sizes)

add_executable(test_serialize_open "src/tests/serialize_open.c")
target_link_libraries(test_serialize_open protocol)
add_test(NAME test_serialize_open COMMAND test_serialize_open)

add_executable(test_serialize_notif "src/tests/serialize_notif.c")
target_link_libraries(test_serialize_notif protocol)
add_test(NAME test_serialize_notif COMMAND test_serialize_notif)

add_executable(test_collision "src/tests/collision.c")
target_link_libraries(test_collision logging command)
add_test(NAME test_collision COMMAND test_collision)

add_executable(test_route_selection "src/tests/route_selection.c")
target_link_libraries(test_route_selection logging command)
add_test(NAME test_route_selection COMMAND test_route_selection)

include(CTest)
enable_testing()


# documentation

find_program(DOXYGEN doxygen)
if (DOXYGEN)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(documentation ALL
        COMMAND ${DOXYGEN} ${DOXYGEN_OUT} 2>&1 > ${CMAKE_CURRENT_BINARY_DIR}/doxygen.log
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM)
else (DOXYGEN_FOUND)
    message("Doxygen needed for documentation")
endif()

