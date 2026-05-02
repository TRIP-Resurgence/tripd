cmake_minimum_required(VERSION 3.20)

# cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GitVersionDetect)

# version from tag
if (GITVERSIONDETECT_VERSION EQUAL "0.0.0-unknown")
    set(TRIPD_VERSION "0.1.0-alpha")
else()
    set(TRIPD_VERSION ${GITVERSIONDETECT_VERSION})
endif()

set(TRIPD_VERSION_SEM ${GITVERSIONDETECT_VERSION_MAJOR}.${GITVERSIONDETECT_VERSION_MINOR}.${GITVERSIONDETECT_VERSION_PATCH})

message(STATUS "Version ${TRIPD_VERSION}")

# commands for code server
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
    trip
    VERSION ${TRIPD_VERSION_SEM}
    DESCRIPTION "TRIP Resurgence Routing Daemon"
    HOMEPAGE_URL "https://github.com/TRIP-Resurgence/tripd"
    LANGUAGES C
)

# gnu dirs
include(GNUInstallDirs)


# source
include_directories("src/")

file(GLOB PROTOCOL_SRC "src/protocol/*.c")
file(GLOB FUNCTIONS_SRC "src/functions/*.c")
file(GLOB DB_SRC "src/db/*.c")
file(GLOB COMMAND_SRC "src/command/*.c")
file(GLOB LOGGING_SRC "src/logging/*.c")
file(GLOB UTIL_SRC "src/util/*.c")
file(GLOB TRIPD_SRC "src/tripd/*.c")

# flags
add_compile_options(-Wall -Wextra -Wno-pedantic -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -coverage)
add_link_options(-coverage)

add_compile_definitions(TRIPD_VERSION="${TRIPD_VERSION}")

if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(CONFIG_PATH "/etc")
else()
    set(CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/etc")
endif()
add_compile_definitions(TRIPD_CONFIG="${CONFIG_PATH}/tripd.conf")

# components

add_library(logging STATIC ${LOGGING_SRC})

add_library(util STATIC ${UTIL_SRC})

add_library(protocol STATIC ${PROTOCOL_SRC})
target_link_libraries(protocol logging)

add_library(db STATIC ${DB_SRC})
target_link_libraries(db logging)

add_library(functions STATIC ${FUNCTIONS_SRC})
target_link_libraries(functions db protocol logging util)

add_library(command STATIC ${COMMAND_SRC})
target_link_libraries(command db functions logging)

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

add_executable(test_serialize_update "src/tests/serialize_update.c")
target_link_libraries(test_serialize_update protocol)
add_test(NAME test_serialize_update COMMAND test_serialize_update)

add_executable(test_parse_open "src/tests/parse_open.c")
target_link_libraries(test_parse_open protocol)
add_test(NAME test_parse_open COMMAND test_parse_open)

add_executable(test_parse_notif "src/tests/parse_notif.c")
target_link_libraries(test_parse_notif protocol)
add_test(NAME test_parse_notif COMMAND test_parse_notif)

add_executable(test_collision "src/tests/collision.c")
target_link_libraries(test_collision logging command)
add_test(NAME test_collision COMMAND test_collision)

add_executable(test_route_selection "src/tests/route_selection.c")
target_link_libraries(test_route_selection logging command)
add_test(NAME test_route_selection COMMAND test_route_selection)

add_executable(test_pattern "src/tests/pattern.c")
target_link_libraries(test_pattern logging command)
add_test(NAME test_pattern COMMAND test_pattern)

add_executable(test_policy "src/tests/policy.c")
target_link_libraries(test_policy logging command)
add_test(NAME test_policy COMMAND test_policy)

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


# installation


install(TARGETS tripd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# config
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    install(FILES tripd.conf DESTINATION "/etc")
else()
    install(FILES tripd.conf DESTINATION "etc")
endif()


# packaging

set(CPACK_GENERATOR "TGZ;DEB")

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_PACKAGE_NAME tripd)
set(CPACK_PACKAGE_VERSION ${TRIPD_VERSION})
set(CPACK_SET_DESTDIR YES)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "arf20 <arf20@arf20.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
set(CPACK_DEBIAN_PACKAGE_NAME "tripd")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")

include(CPack)

