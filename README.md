# AirTwist

Real-time environment for Memsonic.

Motivation is [here](https://github.com/slowguitarist/memsonic/blob/main/README.md#rationale).

## Building

You will need [Memsonic](https://github.com/slowguitarist/memsonic) and
[ZephyrOS](https://github.com/zephyrproject-rtos/zephyr).

Obtain Zephyr and probe-rs IDs for your target, and replace `BOARD` and `CHIP` in the
[build script](./build.sh), respectively. If using QEMU, Zephyr ID is `native_sim` and
probe-rs is not needed.

Build: `./build.sh host build`; build and flash (non-QEMU): `./build.sh host flash`.

## Running simulations

Assuming embedded GDB is used:

1. Visit [tasks.h](./src/tasks.h) and adjust compile-time constants relevant to your simulation.

2. Put you flight plot into [profile.h](./src/profile.h).

3. See [Building](#building).

4. If running on an embedded device, fire a DAP probe:
```
probe-rs gdb --chip {CHIP} --protocol swd
```

4. Run the debugger and reset the target:
```
arm-none-eabi-gdb build/zephyr/zephyr.elf
(gdb) target remote localhost:1337
(gdb) monitor reset halt
```

You can now set breakpoints and send hardware events.