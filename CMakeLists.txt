cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(airtwist)

# Memsonic
set(MEMSONIC_RS_TARGET "thumbv8m.main-none-eabihf")
set(MEMSONIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../memsonic")
set(MEMSONIC_PROFILE "release")
set(MEMSONIC_LIB
    "${MEMSONIC_DIR}/target/${MEMSONIC_RS_TARGET}/${MEMSONIC_PROFILE}/libmemsonic.a")

# This will track upstream source changes
add_custom_command(
    OUTPUT  "${MEMSONIC_LIB}"
    COMMAND cargo build
            --release
            --features ffi
            --target "${MEMSONIC_RUST_TARGET}"
    WORKING_DIRECTORY "${MEMSONIC_DIR}"
    DEPENDS
        "${MEMSONIC_DIR}/Cargo.toml"
        "${MEMSONIC_DIR}/src/lib.rs"
        "${MEMSONIC_DIR}/src/builder.rs"
        "${MEMSONIC_DIR}/src/clocks.rs"
        "${MEMSONIC_DIR}/src/env.rs"
        "${MEMSONIC_DIR}/src/ffi.rs"
        "${MEMSONIC_DIR}/src/filters.rs"
        "${MEMSONIC_DIR}/src/math.rs"
        "${MEMSONIC_DIR}/src/model.rs"
        "${MEMSONIC_DIR}/src/profile.rs"
        "${MEMSONIC_DIR}/src/sensors.rs"
)

add_custom_target(memsonic_rs ALL DEPENDS "${MEMSONIC_LIB}")
add_library(memsonic STATIC IMPORTED GLOBAL)
set_target_properties(memsonic PROPERTIES
    IMPORTED_LOCATION "${MEMSONIC_LIB}"
)
add_dependencies(memsonic memsonic_rs)

target_sources(app PRIVATE src/main.c)

target_include_directories(app PRIVATE include)

target_link_libraries(app PRIVATE memsonic)
target_link_options(app PRIVATE -lm)
