# MicoAir H743 Antenna Tracker — FreeRTOS

This repository contains the standalone FreeRTOS firmware for the MicoAir H743
antenna tracker. FreeRTOS is the maintained firmware direction in this
repository.

## Build

Initialize the MAVLink headers submodule and fetch the pinned STM32, CMSIS,
USB-device and FreeRTOS dependencies:

```sh
git submodule update --init --recursive
./freertos/fetch_dependencies.sh
```

Build one of the firmware profiles:

```sh
./freertos/build.sh boot
./freertos/build.sh bridge
./freertos/build.sh recovered
```

Build output is written to `build/freertos-h743-<profile>/`. The `recovered`
profile includes the sensor and tracking prototype. Physical servo output is
disabled by default; enabling it requires the hardware safety checks described
in [freertos/README.md](freertos/README.md).

## Source and validation

- [`freertos/`](freertos/) contains the firmware, board support, task code,
  diagnostics, and profile build configuration.
- [`common/attitude/`](common/attitude/) contains the attitude estimator used
  by the recovered profile.
- [`third_party/mavlink/`](third_party/mavlink/) supplies the pinned MAVLink C
  headers.
- [`freertos/README.md`](freertos/README.md) documents the profiles, runtime,
  hardware checks, and remaining validation gates.
- [`freertos/docs/m2-bridge.md`](freertos/docs/m2-bridge.md) documents the USB
  and TELEM1 bridge bring-up.

Host tests are available through `freertos/tests/run.sh`. A host-test pass or a
successful cross-build does not by itself verify USB enumeration, sensor
behavior, GPS fix, telemetry wiring, or physical servo output on the board.
