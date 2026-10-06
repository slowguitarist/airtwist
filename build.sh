#!/usr/bin/env bash
set -euo pipefail

# This script expects to find CMake, West, arduino-cli, and probe-rs.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"

case "${1:-}" in
	nucleo)
		PLATFORM="zephyr"
		BOARD="nucleo_u545re_q"
		CHIP="STM32U545RETx"
		ELF="$BUILD_DIR/zephyr/zephyr.elf"
		;;
	nano)
		PLATFORM="arduino"
		BOARD="arduino:samd:nano_33_iot"
		CHIP="ATSAMD21G18A"
		ELF="$BUILD_DIR/arduino/noisegenerator.ino.elf"
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

ARDUINO_ARGS=(
	--build-property
	compiler.c.elf.extra_flags=-latomic
)

CMAKE_ARGS+=("$@")
cd "$ROOT"

case "$ACTION" in
	init)
		if [[ "$PLATFORM" == "arduino" ]]; then
			arduino-cli core install arduino:samd
			arduino-cli lib install Arduino_LSM6DS3
			arduino-cli lib install "SAMD_TimerInterrupt"
		else
			echo "use 'conf' for $PLATFORM."
		fi
		;;

	conf)
		if [[ "$PLATFORM" == "zephyr" ]]; then
			west build \
				-p always \
				--cmake-only \
				-b "$BOARD" \
				. \
				-d "$BUILD_DIR" \
				-- "${CMAKE_ARGS[@]}"
		else
			echo "use 'init' for $PLATFORM."
			exit 1
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
				--build-path "$BUILD_DIR/arduino" \
				"$ROOT/noise-generator"

            arduino-cli compile \
                --only-compilation-database \
                --fqbn "$BOARD" \
                --build-path "$BUILD_DIR/arduino-clangd" \
                "${ARDUINO_ARGS[@]}" \
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
		elif [[ "$PLATFORM" == "arduino" ]]; then
			arduino-cli compile \
				--fqbn "$BOARD" \
				--build-path "$BUILD_DIR/arduino" \
                "${ARDUINO_ARGS[@]}" \
				"$ROOT/noise-generator"
		fi

		probe-rs download \
			--chip "$CHIP" \
			"$ELF"
		;;

	*)
		echo "$ACTION is unrecognized -- please add it to build.sh."
		exit 2
		;;
esac
