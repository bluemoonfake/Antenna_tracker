# FreeRTOS Antenna Tracker — staged bring-up

This directory was recovered on 2026-09-27 from unreachable Git tree
`b2dedb3dfd3ecc815d35d55ac22d3c79d2612687`. All 28 original files were restored
with matching Git blob hashes and executable modes. The untouched restored
source built a 92,668-byte binary identical to the existing historical binary:

```text
SHA-256: 687953ce8779312110b8943fcb9c7575c6ad4b58ebc0d467360891b93c349e11
```

`recovery-manifest.json` records each original file, pinned dependencies and
the archive containing the untouched source. It describes the recovered input;
subsequent milestone changes are separate from those original hashes.

## Build profiles

From the repository root, with `arm-none-eabi-gcc`, CMake and Ninja installed:

```sh
./freertos/build.sh boot
```

`boot` is the default profile. Output:

```text
build/freertos-h743-boot/microair_h743_tracker_freertos.elf
build/freertos-h743-boot/microair_h743_tracker_freertos.bin
```

It uses the recovered STM32H743 clock setup, relocated vector table and
FreeRTOS kernel. The `boot_diag` task starts hardware PWM once after the
scheduler starts: TIM15 CH2 on PE6 (AF4), 1 kHz with 50% LED-on duty. The LED
is active low, so PWM uses low polarity. With APB2 = HCLK / 2, TIM15 runs
at 240 MHz; PSC = 239, ARR = 999 and CCR2 = 500. No timer IRQ or DMA is used.
`LED_PWM_FREQUENCY_HZ` and `LED_PWM_DUTY_PERCENT` in `src/boot_board.c`
set frequency and LED-on duty. Pin mapping is documented in the
[STM32H743 datasheet, Table 10](https://www.st.com/resource/en/datasheet/stm32h743vi.pdf).

The task then updates diagnostics every 500 ms without toggling GPIO.
Idle and timer service tasks are also created by FreeRTOS. Debug
information is enabled; `g_boot_status` exposes task iterations, RTOS ticks,
HAL milliseconds, free/minimum heap and task stack high-water mark.

`g_boot_status` is a variable in MCU RAM, not an Ubuntu shell command. Read it
through a connected SWD debugger using the ELF symbols. Visible green blinking
can belong to the PX4 bootloader; the application should leave green PWM on.
Clock setup now explicitly enables SYSCFG before selecting VOS0, as required
by STM32H743 HAL. Startup MSP now uses the reserved 4 KiB AXI RAM stack,
because the existing board bootloader disables DTCM before jumping to the app.
Both fixes change both profiles from the recovered baseline;
the byte-identical result above refers to the untouched recovery build.

Boot LEDs now indicate progress through application startup:

- Blue only: `main` has started; HAL/clock initialization is in progress.
- Blue plus green: clock configuration returned; task/PWM startup is pending.
- Green only: the boot task started hardware PWM successfully.
- Red only: the application entered `board_fatal`.
- All LEDs off after bootloader blinking: no persistent application stage
  was observed; inspect startup before `main` with SWD and verify flash content.

`g_boot_status.boot_stage` records main entry (1), HAL ready (2), clock ready
(3), task created (4), task running (5), PWM running (6). The pre-scheduler LED
markers are GPIO levels; the final green indication is hardware PWM. These
markers locate progress, not proof that the task continues running.

Boot configures PE13/PE14 as GPIO low. It does not initialize/start TIM1,
UART, USB, SPI or I2C peripherals, or start bridge/sensor/control tasks.
The clock configuration still selects peripheral clock sources. GPIO and
kernel setup plus LED PWM are the active bring-up functionality. A fatal hook
resets/disables TIM15, restores PE6 to GPIO high (green off), turns on the
red LED on PE5 and halts with interrupts disabled. A steady green LED alone
does not prove task health: hardware PWM continues while the task is blocked
or stalled, so inspect advancing `g_boot_status` counters as well.

The M2 communication profile is built separately:

```sh
./freertos/build.sh bridge
# Override the physical FC UART baud if required:
FC_BAUD_RATE=115200 ./freertos/build.sh bridge
```

Outputs are in `build/freertos-h743-bridge/`. It starts only `mav_bridge`
(priority 5, 1536 stack words) and `boot_diag` (priority 1, 256 stack words),
in addition to the kernel's Idle/timer tasks. It initializes USART1 on
PA9/TX1 and PA10/RX1, USB CDC, and the M1 green LED PWM. PE13/PE14 remain
GPIO low; GPS, SPI, I2C, sensor/control tasks and servo TIM1 are not started.
UART defaults to 57600 baud, 8N1, without hardware flow control. USB line
coding does not change that physical UART baud. See [M2 bring-up](docs/m2-bridge.md).

The recovered prototype remains available as a reference build:

```sh
./freertos/build.sh recovered
```

Its output is in `build/freertos-h743-recovered/`. Servo output remains
disabled by `TRACKER_ENABLE_SERVO_OUTPUT=0`. This prototype starts all three
old application tasks; it has not been approved by the later milestone gates.

Set `BUILD_DIR` to select a different build directory. Dependencies are pinned
in `fetch_dependencies.sh`; the existing five dependency repositories matched
those revisions during recovery.

### RTOS configuration policy

`include/FreeRTOSConfig.h` is the single kernel configuration header. The
current policy uses a 1 kHz preemptive scheduler, 32-bit ticks, priority levels
0–7, priority-inheritance mutexes, stream buffers, stack-overflow checking and
the malloc-failed hook. Application objects currently use `heap_4` and are
created before `vTaskStartScheduler()`; static allocation is intentionally not
enabled until the application resource budget is stable.

Application task stack sizes and priorities are centralized in
`include/tracker_config.h`:

| Task | Priority | Stack (words) | Period or wait |
| --- | ---: | ---: | --- |
| `boot_diag` | 1 | 256 | 500 ms |
| `mav_bridge` | 5 | 1536 | 1 tick polling loop |
| `sensors` | 4 | 768 | GPS wait up to 10 ms; compass 20 ms |
| `control` | 6 | 768 | 50 Hz |

Build-time safety and link settings are exposed by `freertos/build.sh`:

```sh
TRACKER_TARGET_SYSTEM_ID=1 ./freertos/build.sh bridge
TRACKER_ENABLE_SERVO_OUTPUT=OFF ./freertos/build.sh recovered
FC_BAUD_RATE=115200 ./freertos/build.sh bridge
```

Servo output remains `OFF` by default. Enabling it is only appropriate after
the M4/M5 hardware gates in this document have been completed. Run
`./freertos/format.sh --check` in CI or before review; use
`./freertos/format.sh` to format the FreeRTOS-owned C/H files.

## Inspect boot with ST-Link V2

Power the board through its USB connector and connect the probe's SWDIO,
SWCLK and GND to the board pads with those labels. If the probe has a VTref
input, connect it to the target's 3.3 V reference. A clone's 3.3 V power
output is not a VTref input; check its pin labels before connecting power.
NRST is optional for this attach configuration. Do not hold BOOT.

The workspace configuration `FreeRTOS boot — ST-Link V2 attach` in
`../.vscode/launch.json` uses Cortex-Debug, OpenOCD and `gdb-multiarch`.
It attaches to the existing firmware and loads ELF symbols without flashing.
The ELF must match the image on the board. The OpenOCD configuration uses
the direct DAP driver, requiring ST-Link V2 firmware V2J24 or newer.

Select the configuration in Run and Debug and start it. While halted, add
`g_boot_status` to Watch, or enter this in Cortex-Debug's Debug Console:

```text
p g_boot_status
```

Record one sample, Continue for at least two seconds, Pause and record
another. `boot_stage` should be 6 (`BOOT_STAGE_PWM_RUNNING`); iterations,
RTOS ticks and HAL milliseconds should increase between samples. Free heap
should settle and stack high-water mark should remain positive. Halting the
CPU prevents task counters from advancing; resume between samples.

For a terminal session, start the server in one terminal:

```sh
openocd -f freertos/debug/stlink-h743.cfg
```

In a second terminal:

```sh
gdb-multiarch build/freertos-h743-boot/microair_h743_tracker_freertos.elf
```

Then use `target extended-remote localhost:3333`, `monitor halt`, and
`p g_boot_status`. Use `continue`, wait, Ctrl+C, and `p g_boot_status` for
the second sample. `info threads` can show FreeRTOS tasks when OpenOCD's
RTOS detection succeeds. See the
[OpenOCD GDB guide](https://openocd.org/doc/html/GDB-and-OpenOCD.html).

## Milestone sequence and acceptance

### M0 — Recover the old source: PASS

- Restore 28 original files and verify every Git blob hash.
- Build in a fresh directory without overwriting the historical artifact.
- Compare the resulting BIN byte for byte with the old BIN.
- Archive original source, dependency revisions, build log and hashes.

### M1 — Boot/task: build and task runtime PASS; full hardware gate pending

- Build the `boot` profile and inspect its vector table and linked symbols.
- Application flash starts at `0x08020000`; MSP uses AXI RAM, with bounds
  `_sstack` and `_estack` in the ELF. The current `boot` image reserves
  `0x24019268..0x2401A268` (4 KiB), with initial MSP `0x2401A268`.
- Use the `boot` ELF for a debug session on the MicoAir H743.
- With servo power/load disconnected, check three power cycles and at least
  60 seconds of operation. Green LED should appear steady. Measure PE6:
  1 kHz, 500 us low (LED on) and 500 us high (LED off) per cycle;
  red fatal LED should remain off; no reset/hard fault should occur.
- Halt the MCU to read a coherent `g_boot_status` snapshot. Between samples,
  iterations/ticks/HAL milliseconds must advance; stack high-water mark must
  remain positive and free heap must settle after startup.
- Verify the fatal path stops green PWM and leaves the red LED on.
- Measure PE13/PE14 to confirm no servo PWM at boot or during diagnostics.
- Save console/debugger records, firmware hash and pin measurements before
  declaring hardware PASS.

No board flash or runtime validation was performed during source recovery.
On 2026-09-27, the user manually flashed the 27,600-byte SYSCFG fix, then the
27,676-byte stage-LED diagnostic image at `0x08020000`. Both power cycles
ended with bootloader blinking followed by all LEDs off. The latter image's
DFU readback matches the BIN byte for byte, SHA-256
`048373a086131815a45fbc9d3f623a6e46899d1e74c9b9925068d76b5cd1c39a`.
Flash content is verified; application/task execution remains unconfirmed.

Startup inspection found a mismatch: the board's bootloader readback matches
the PX4 MicoAir H743 reference bootloader. Its instructions at `0x0800035E`
through `0x08000366` clear bit 0 of `DTCMCR` (`0xE000EF94`), disabling DTCM.
The old application initial MSP was `0x20020000` in DTCM. The CMSIS startup
does not re-enable DTCM before calling `SystemInit`, whose first instruction
pushes onto that stack. See the EN bit definition in
[ST PM0253, section 4.9.1](https://www.st.com/resource/en/programming_manual/pm0253-stm32f7-series-and-stm32h7-series-cortexm7-processor-programming-manual-stmicroelectronics.pdf).
This is a likely failure before `main`, awaiting hardware confirmation.

The linker now places MSP in the existing AXI RAM stack reservation. CMake
tracks linker-script changes so rebuilding relinks the image. Both profiles
build successfully. The corrected `boot` BIN is still 27,676 bytes, but its
SHA-256 is now
`c56303ce51eacb8b78aed464e4032ce18ed11e96a7d759b9fbeb374ffcce8c57`.
Only eight BIN bytes changed: the initial MSP vector and its startup literal.
The user then reported that the LED remained on after initial blinking
with the corrected image. This is preliminary runtime evidence; the LED
color, advancing task counters and pin measurements had not yet been recorded
at that point. Later on 2026-09-27, the corrected SWD wiring allowed OpenOCD
to detect Cortex-M7 and DPIDR `0x6ba02477`. The user supplied halted debugger
snapshots with `BOOT_STAGE_PWM_RUNNING`: iterations 32 -> 39, RTOS ticks and
HAL milliseconds 15500 -> 19000, free/minimum heap 93704 bytes in both
samples, and stack high-water mark 192 words in both samples. Seven iterations
in 3500 ms match the 500 ms diagnostic period. Task runtime is confirmed.
Three power-cycle/60-second evidence, physical pin measurements and the
fatal-path check have not been supplied; full M1 hardware acceptance remains
pending.
The user continued with M2 deployment and communication tests while the
remaining M1 checks above are still outstanding for full board acceptance.

### M2 — UART/USB bridge: software PASS; hardware communication partially PASS

- The user requested M2 implementation after confirming M1 task runtime.
  The dedicated `bridge` profile has since been deployed by the user.
- Validate UART baud/pin mapping and USB CDC enumeration first.
- Stream buffers and both application tasks are created before the scheduler.
  The bridge task starts USB and UART RX after the scheduler is running.
- UART RX uses one-byte interrupts; UART TX uses an interrupt and an owned
  128-byte buffer. USB owns its 64-byte transfer buffer. ISR callbacks only
  move bytes, rearm reception and update driver counters; MAVLink parsing is
  task-side. DMA is not enabled, so no DMA/cache maintenance is required here.
- Keep raw MAVLink traffic intact and inject tracker heartbeat only at a
  complete FC wire-frame boundary, only toward USB/GCS. Wire framing includes
  v1/v2 lengths and v2's optional signature, independent of CRC/dialect
  acceptance. A 250 ms RX gap or UART loss resets framing and suppresses
  injection until a subsequent complete frame. Heartbeats are attempted once
  per second at a boundary with queue capacity, including under continuous RX.
- USB reset/configuration/suspend changes purge pending queues and old USB RX
  bytes. An already accepted UART transfer can still finish; discarded bytes
  and aborted USB transfers are counted. Buffers are bounded at 4096 bytes.
- `g_bridge_status` and `g_bridge_io_status` expose task health, valid/bad
  frames, target updates, RX/TX/drop/error counts and link changes.
- Verify both directions, target message parsing, intentional fragmented/bad
  frames, link recovery and sustained load. Record drop/error counters.
- Native tests run the production protocol and bridge task with simulated
  I/O; address/undefined-behavior sanitizers are enabled.
- User-provided hardware evidence: CDC `1b8c:0036` on `/dev/ttyACM0`,
  3000/3000 exact UART loopback echoes over 60 seconds with 60 tracker
  heartbeats, then 1500/1500 fragmented echoes over 30 seconds including
  150 deliberately bad-CRC frames preserved and 30 tracker heartbeats.
  Active-server memory reads subsequently confirmed 1350 valid frames,
  150 bad frames and 1350 target updates on the latest run, with correct
  target coordinates, unchanged heap and zero sampled error/drop counters.
  Hardware system-ID filtering, controlled power-cycle/reconnect and
  external-FC checks remain pending.
  See [recorded M2 hardware evidence](docs/m2-bridge.md#recorded-hardware-evidence--2026-09-27).

```sh
./freertos/tests/run.sh
```

### M3 — Sensor and tracker state: pending

- Validate each bus/device separately, then timestamps and sample freshness.
- Port BMI088 roll/pitch estimation and calibrated QMC5883L heading with their
  actual mounting frames. The recovered sensor task only reads GPS and a raw
  compass heading; it does not implement IMU attitude estimation.
- Verify tracker GPS lock, heading calibration/orientation/declination and
  invalid/stale sensor behavior before using them for pointing.

### M4 — PWM: pending

- Add explicit, bounded single-axis test commands with auto-stop.
- Measure frequency, pulse widths, clamp, startup and stop at the physical
  pins before validating servo direction/range and mechanical limits.
- Specify the servo type and what neutral/STOP means for each axis.

### M5 — Tracking and timeout: pending

- Validate bearing/elevation and reachable mechanical sectors against known
  positions; check altitude reference, invalid coordinates and date-line math.
- Define the state machine, feedback controller, target timeout, sensor timeout,
  STOP/park and recovery policy for the actual mechanisms.
- Start with static pointing, then bounded moving targets and deliberate
  target/sensor loss. Require evidence before enabling unrestricted output.

## Limitations of the recovered prototype

- The original archived recovery inserted heartbeat into both streams and
  started RTOS-aware UART/USB I/O before the scheduler. The current shared
  bridge code fixes both behaviors; the recovery manifest still describes the
  untouched archived input, not this modified source.
- Compass heading uses raw `atan2(y, x)` without installation calibration.
- Control currently maps angles linearly to pulses; it is not a validated PID
  loop. Freshness failure stops PWM, rather than moving to an agreed park pose.
- Geometry lacks complete coordinate/range checking; signed coordinate and
  altitude subtraction needs review before M5.
- Linker warnings from newlib's unimplemented system calls remain; the recovered
  profile also has warnings in generated MAVLink headers. No project-source
  warning was observed in the milestone builds.

A host test or a build of another profile does not verify the recovered
profile's hardware behavior. The `.bin` is a raw application image; no FreeRTOS
`.px4` package or bootloader upload flow has been validated yet.
