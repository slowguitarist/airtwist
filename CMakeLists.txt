cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(airtwist)

include(ExternalProject)

find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(MEMSONIC_GIT_TAG "main" CACHE STRING "Memsonic branch, tag, or commit")

set(MEMSONIC_RS_TARGET "thumbv8m.main-none-eabihf")
set(MEMSONIC_CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/_deps/memsonic-target")
set(MEMSONIC_LIB "${MEMSONIC_CARGO_TARGET_DIR}/${MEMSONIC_RS_TARGET}/debug/libmemsonic.a")

ExternalProject_Add(memsonic_git
    PREFIX "${CMAKE_BINARY_DIR}/_deps/memsonic"
    GIT_REPOSITORY "https://github.com/slowguitarist/memsonic.git"
    GIT_TAG "${MEMSONIC_GIT_TAG}"

    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        "${CMAKE_COMMAND}" -E env
        "CARGO_TARGET_DIR=${MEMSONIC_CARGO_TARGET_DIR}"
        "${CARGO_EXECUTABLE}" build
        --manifest-path <SOURCE_DIR>/Cargo.toml
        --features ffi
        --target "${MEMSONIC_RS_TARGET}"
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS "${MEMSONIC_LIB}"
    INSTALL_COMMAND ""
)

add_library(memsonic STATIC IMPORTED GLOBAL)
set_target_properties(memsonic PROPERTIES
    IMPORTED_LOCATION "${MEMSONIC_LIB}"
)

target_sources(app PRIVATE
    src/main.c
    src/injector.c
    src/sensor.c
    src/simulant.c
)

target_include_directories(app PRIVATE include)

target_link_libraries(app PRIVATE memsonic)
target_link_options(app PRIVATE -lm)

add_dependencies(app memsonic_git)
