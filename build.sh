#!/usr/bin/env bash
set -euo pipefail

# This script expects to find CMake, West, and probe-rs.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build/"
ELF="$BUILD_DIR/zephyr/zephyr.elf"

case "${1:-}" in
    nucleo)
        BOARD="nucleo_u545re_q"
        CHIP="STM32U545RETx"
        ;;
    *)
        echo "Unrecognized board -- please add it to build.sh."
        exit 2
        ;;
esac

ACTION="${2:-build}"
shift 2 || true

if [[ "${1:-}" == "--" ]]; then
    shift
fi

CMAKE_ARGS=(
    "-DCONF_FILE=zephyr-kconfig"
    "-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb"
    "-DGNUARMEMB_TOOLCHAIN_PATH=/usr"
)

CMAKE_ARGS+=("$@")
cd "$ROOT"

case "$ACTION" in
    conf)
        west build \
            -p always \
            --cmake-only \
            -b "$BOARD" \
            . \
            -d "$BUILD_DIR" \
            -- "${CMAKE_ARGS[@]}"
        ;;

    build)
        west build \
            -p always \
            -b "$BOARD" \
            . \
            -d "$BUILD_DIR" \
            -- "${CMAKE_ARGS[@]}"
        ;;

    flash)
        west build \
            -p always \
            -b "$BOARD" \
            . \
            -d "$BUILD_DIR" \
            -- "${CMAKE_ARGS[@]}"

        probe-rs download \
            --chip "$CHIP" \
            "$ELF"
        ;;

    *)
        echo "$ACTION is unrecognized -- please add it to build.sh."
        exit 2
        ;;
esac
