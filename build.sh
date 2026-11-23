#!/usr/bin/env bash
set -euo pipefail

# This script expects to find CMake, West, arduino-cli, and probe-rs.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"

case "${1:-}" in
	host)
		PLATFORM="zephyr"
		BOARD="nucleo_u545re_q"
		CHIP="STM32U545RETx"
		ELF="$BUILD_DIR/zephyr/zephyr.elf"
		;;
	nano)
		PLATFORM="arduino"
		BOARD="arduino:samd:nano_33_iot"
		;;
	*)
        echo "Unrecognized board (hint: you can add yours to build.sh)."
		exit 2
		;;
esac

ACTION="${2:-build}"
DEFAULT_PORT="/dev/ttyACM0"
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
	init)
		if [[ "$PLATFORM" == "arduino" ]]; then
			arduino-cli core install arduino:samd
			arduino-cli lib install Arduino_LSM6DS3
			arduino-cli lib install "SAMD_TimerInterrupt"
        elif [[ "$PLATFORM" == "zephyr" ]]; then
            rustup default stable
            rustup target add thumbv8m.main-none-eabihf
			python -m venv .venv
            source .venv/bin/activate
            python -m pip install west jsonschema pyelftools
            west update
		fi
		;;

	build)
		if [[ "$PLATFORM" == "zephyr" ]]; then
			west build \
				-p always \
				-b "$BOARD" \
				. \
				-d "$BUILD_DIR" \
				-- "${CMAKE_ARGS[@]}"
		elif [[ "$PLATFORM" == "arduino" ]]; then
			arduino-cli compile \
				--fqbn "$BOARD" \
				--port "$DEFAULT_PORT" \
				--build-path "$BUILD_DIR/arduino" \
				"$ROOT/noise-generator"

			arduino-cli compile \
				--only-compilation-database \
				--fqbn "$BOARD" \
				--build-path "$BUILD_DIR/arduino-clangd" \
				"$ROOT/noise-generator"
		fi
		;;

	flash)
		if [[ "$PLATFORM" == "zephyr" ]]; then
			west build \
				-p always \
				-b "$BOARD" \
				. \
				-d "$BUILD_DIR" \
				-- "${CMAKE_ARGS[@]}"

			probe-rs download \
				--chip "$CHIP" \
				"$ELF"
		elif [[ "$PLATFORM" == "arduino" ]]; then
			arduino-cli compile \
				--upload \
				--fqbn "$BOARD" \
				--port "$DEFAULT_PORT" \
				--build-path "$BUILD_DIR/arduino" \
				"$ROOT/noise-generator"
		fi
		;;

	*)
        echo "$ACTION is unrecognized (hint: you can add it to build.sh)."
		exit 2
		;;
esac
