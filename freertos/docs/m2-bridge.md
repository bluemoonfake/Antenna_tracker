# M2 UART/USB bring-up

Status: software PASS; hardware communication checks partially PASS. The user
has deployed the bridge profile and supplied the loopback results below.
Outstanding M1 pin, fatal-path and power-cycle checks remain listed in the
main README; full M2 hardware acceptance is still pending.

## Build and flash

```sh
./freertos/build.sh bridge
```

The physical USART1 baud is 57600, 8N1, no flow control. To change it, build
with `FC_BAUD_RATE=115200 ./freertos/build.sh bridge`, and set the connected
FC or UART adapter to the same baud. USB CDC line coding does not reconfigure
the UART.

Use the existing DFU procedure, then:

```sh
dfu-util -d 0483:df11 -S 200364500000 -a 0 -s 0x08020000 \
  -D build/freertos-h743-bridge/microair_h743_tracker_freertos.bin
```

For readback, use the actual new BIN size rather than the old boot size:

```sh
dfu-util -d 0483:df11 -S 200364500000 -a 0 \
  -s "0x08020000:$(stat -c %s build/freertos-h743-bridge/microair_h743_tracker_freertos.bin)" \
  -U /tmp/tracker-m2-readback.bin
cmp build/freertos-h743-bridge/microair_h743_tracker_freertos.bin \
  /tmp/tracker-m2-readback.bin
```

Power-cycle without BOOT. Green LED PWM/diagnostics remain available. The
application should enumerate persistently as `1b8c:0036`, product
`MicoAir H743 FreeRTOS Tracker`, and `/dev/ttyACM*`. The bootloader's temporary
`3162:004b MicoAir743` appearance is a different USB device.

## SWD diagnostics

Use `FreeRTOS M2 bridge — ST-Link V2 attach`, which selects the bridge ELF.
Halt and read:

```gdb
p g_boot_status
p g_bridge_status
p g_bridge_io_status
p g_state
```

`g_boot_status` should advance every 500 ms as before. `g_bridge_status`
should show `io_started=true` and `usb_configured=true` while CDC is connected.
Its loop runs with a minimum 1 ms delay; diagnostic time/heap/stack fields
refresh every 500 ms. Each HAL TX acceptance copies into driver-owned memory,
so the task can consume/reuse its queue bytes before the interrupt completes.
Compare accepted/completed UART TX counts after traffic drains. For USB,
accepted minus completed minus aborted is the amount still in flight.

UART RX overflow, UART errors and failed RX restarts are counted separately.
Bad CRC or unsupported MAVLink frames are forwarded raw but do not update
target state. Target heartbeat/position updates are restricted to system ID
1 (`TRACKER_TARGET_SYSTEM_ID`); `GLOBAL_POSITION_INT.alt` is kept in mm, with
coordinates in 1e-7 degrees. A missing/truncated frame cannot cause heartbeat
injection until a complete later wire frame restores a boundary.
Signature bytes are preserved for signed v2 frames, but M2 does not configure
MAVLink signing keys or authenticate signatures. Other messages are forwarded;
local command handling is outside this milestone.

## UART loopback check

Disconnect the external FC and, with power removed, connect UART1 **TX1/PA9
to RX1/PA10** on the tracker. Use the pin labels; SWD and I2C four-pin plugs
have different pin orders. This is a temporary UART loopback, not an SWD
connection. The bridge profile never enables the servo timer.

Power the board through USB-C, close serial/GCS programs using its CDC port,
and keep the CPU running during transport measurements. Substitute its actual
TTY path:

```sh
lsusb -d 1b8c:0036
ls /dev/serial/by-id/
python3 freertos/tools/bridge_loopback.py --port /dev/ttyACM0 --seconds 60
python3 freertos/tools/bridge_loopback.py --port /dev/ttyACM0 --seconds 30 \
  --fragment --bad-every 10
```

The checker sends 50 v2 position frames/s (2000 byte/s by default), validates
exact ordered echoes through USB -> UART TX -> UART RX -> USB, and counts
tracker heartbeats between complete echoed frames. Deliberately bad CRC
frames must echo unchanged but must not increment `target_updates`.
Before sending, the checker drains pre-existing bytes until the port is quiet
for 200 ms (at most 3 seconds). Opening CDC can expose an old partial heartbeat;
this startup traffic is reported separately and excluded from measurement.
During measurement every byte remains checked strictly: unexpected bytes or
frames fail with a hex dump and progress counters, without resynchronizing.
Inspect `g_state.target` after a valid test: lat=110000000, lon=1060000000,
altitude_mm=23700. Run below UART capacity for loss-free acceptance; 57600
8N1 carries at most 5760 byte/s in each direction. Oversubscription may drop
bytes, but queues must remain bounded and drop counters must report it.

Repeat after a power cycle and a USB reconnect. To test USB reconnection
without resetting the tracker, use an independently verified board power
input; changing USB power alone also reboots the board and tests a different
case. A USB disconnect/reset discards queued old commands; at most the current
128-byte accepted UART transfer can finish. Data already accepted by the
64-byte USB transfer can remain in flight over suspend/resume. Stop and
restart the checker after a disconnect; it intentionally fails on link loss.

Finally remove the loopback and connect tracker TX1 to the external FC RX,
tracker RX1 to FC TX, and common GND using compatible 3.3 V UART signaling.
Confirm unchanged MAVLink in both directions, updates only from the selected
target system, and tracker heartbeat only toward USB/GCS. Save firmware SHA,
readback comparison, CDC identity, debugger snapshots, load/reconnect logs
and drop/error counts before marking M2 hardware PASS.

## Hardware target-system filtering check

This check is prepared but has not been recorded as hardware PASS. Use the
`bridge` firmware with `TRACKER_TARGET_SYSTEM_ID=1`, disconnect the external
FC, join TX1/PA9 to RX1/PA10 with power removed, then power the board and close
other CDC clients. The host checker now accepts `--sysid`, `--latitude-e7`,
`--longitude-e7` and `--altitude-mm`; no firmware change is needed for these
host options. Do not use `--bad-every` for the filtering check.

Attach with the matching M2 ELF. Halt the MCU and save a baseline:

```gdb
p g_bridge_status
p g_bridge_io_status
p g_state.target
```

Resume the CPU before running the checker; keep it running for the full serial
test. First send valid system-1 position frames:

```sh
python3 freertos/tools/bridge_loopback.py --port /dev/ttyACM0 \
  --seconds 10 --rate 10 --sysid 1
```

After the checker exits, halt and save another snapshot. If it reports `N/N`
exact echoes, `fc_valid_frames` and `target_updates` must each increase by `N`,
`fc_bad_frames` must not increase, and target must be valid at
latitude_e7=110000000, longitude_e7=1060000000, altitude_mm=23700.

Resume, then send valid system-2 frames with different coordinates:

```sh
python3 freertos/tools/bridge_loopback.py --port /dev/ttyACM0 \
  --seconds 10 --rate 10 --sysid 2 \
  --latitude-e7 120000000 --longitude-e7 1070000000 --altitude-mm 45000
```

After exit, halt and compare with the preceding snapshot. For `N/N` echoes,
`fc_valid_frames` must increase by `N`, but `target_updates` must not increase.
The entire target, including `timestamp_ms`, must remain unchanged, and no bad
frames or RX/drop/error counters should be added. The raw echoes must remain
byte-for-byte identical despite filtering. Use the checker's actual `N`, not
an assumed count based on elapsed time.

Finally resume and repeat system 1 with the second set of coordinates. Its
`N` valid frames must again produce `N` target updates and set the new target,
showing that acceptance still works after the rejected system. Keep power and
firmware unchanged between snapshots; a reset invalidates counter deltas.
The script's `PASS` confirms transport only. Declare filtering PASS only after
both the echoes and debugger comparisons match. Save the three checker logs
and baseline/after snapshots before updating the hardware status.

## Recorded hardware evidence — 2026-09-27

The following results were supplied by the user from the MicoAir H743 with
UART1 TX1 joined to RX1 and the checker running on Ubuntu:

- USB enumerated as `1b8c:0036 MicoAir H743 FreeRTOS Tracker`, with
  `/dev/ttyACM0`. GDB initially used the boot ELF; selecting the bridge ELF
  made M2 symbols and diagnostic values available.
- Before loopback, snapshots showed `io_started=true`,
  `usb_configured=true`, free/minimum heap 79088 bytes, and bridge stack
  high-water mark 1312 words. USB TX completed 1281 bytes; four USB RX bytes
  were accepted and completed by UART TX. UART RX was still zero then.
- The first checker attempt failed on an unexpected byte. Its raw input was
  not captured, so the exact original cause remains unconfirmed. The checker
  was updated to drain pre-test traffic and report strict measurement errors
  with hex bytes and progress counters; pseudo-terminal regression checks PASS.

Subsequent hardware results:

```text
--seconds 10
Startup: drained 4136 pre-test bytes; strict loopback measurement starts now
PASS: 500/500 exact echoes, 0 bad-CRC frames preserved, 10 tracker heartbeats

--seconds 60
Startup: drained 1224 pre-test bytes; strict loopback measurement starts now
PASS: 3000/3000 exact echoes, 0 bad-CRC frames preserved, 60 tracker heartbeats

--seconds 30 --fragment --bad-every 10
Startup: drained 363 pre-test bytes; strict loopback measurement starts now
PASS: 1500/1500 exact echoes, 150 bad-CRC frames preserved, 30 tracker heartbeats
```

This confirms exact ordered round-trip traffic and heartbeat placement in
these measured intervals, including preservation of deliberately bad CRC.
Pre-test drained bytes are excluded from the echo measurement.

After a later USB/SWD reconnect, GDB displayed zero diagnostics from an old
session. Two GDB/OpenOCD pairs were still present: the old server reported
target state `unknown` and failed every memory read; the new server read the
running board successfully. The disconnected pair was closed. Read-only
queries through the existing active server found:

- Application vectors `0x2401D1A0`, `0x08020331`, matching the bridge build.
- `fc_valid_frames=1350`, `fc_bad_frames=150`, `target_updates=1350`, matching
  the latest 1500-frame run with 150 intentionally bad frames on this boot.
- Target lat=110000000, lon=1060000000, altitude_mm=23700 and valid=true.
- Free/minimum heap 79088 bytes; bridge stack high-water 1200 words, boot
  stack high-water 192 words; sampled UART/USB error/drop counters all zero.

These queries were made while the CPU was running. Individual counters are
useful evidence; changing fields across separate reads are not a coherent
halted snapshot. The target state and parser totals support bad-CRC rejection
by the application for this run.

A subsequent missing-symbol report came from the remaining GDB loading the
boot ELF, confirmed through its process mappings. A console `file` command
only changes the current GDB session; a new launch uses `launch.json` again.
The M2 configuration is now first in that file; the separate boot configuration
is named `FreeRTOS M1 boot — ST-Link V2 attach`. Restart the debug session and
select M2 explicitly, then confirm `info files` points to the bridge ELF.

Later, zero diagnostics recurred with the correct ELF while three debug
server pairs remained open after probe reconnection. Only the newest server
could read the board. With the CPU halted, it read nonzero diagnostics:
4350 valid frames, 150 bad frames and 4350 target updates; target coordinates
remained correct, heap remained 79088 bytes and sampled error/drop counts were
zero. The two disconnected pairs were closed. This incident does not establish
a D-cache visibility fault; the active server could read the halted RAM values.

Both VS Code profiles now run `freertos/debug/check_session.sh` before
starting OpenOCD. It refuses a second server using this repo's SWD config.
Use Continue/Pause in the existing session; press Stop before starting a new
one or unplugging the probe. If the check fails, resolve the existing session
instead of choosing Debug Anyway. An already-running debugger is not stopped
by the check itself.

Still outstanding: hardware target-system filtering, controlled power-cycle
and USB-session recovery checks, and external-FC integration. The reconnect
above exposed stale debugger sessions; no controlled traffic/purge recovery
test has yet been recorded.
For an exact parser-count check, save before/after snapshots around a new
`--seconds 10 --fragment --bad-every 10` run: 500 echoes imply 450 valid
position frames, 50 bad frames and 450 target updates, assuming no other
traffic. Compare counter deltas rather than absolute cumulative totals.
USB reconnect without an MCU reset requires independently verified board
power; a USB-powered unplug/replug instead exercises a power cycle.

## Local verification

```sh
./freertos/tests/run.sh
```

Tests cover production task routing and target filtering, raw bad-CRC traffic,
fragmented input, heartbeat placement under continuous RX, queue wrap/overflow,
USB session purge/reconnect, v1/v2/signed wire framing and timestamp wrap.
They use simulated hardware services and ASan/UBSan; HAL interrupt timing,
electrical UART behavior and actual USB enumeration require the board checks.

Verified on 2026-09-27 at the default 57600 baud:

- `boot`, `bridge` and `recovered` build successfully. Only the existing
  newlib/no-syscall link warnings remain; no project compilation warnings.
- All native protocol checks and four production-task scenarios pass with
  ASan/UBSan. The 20,000-iteration load case preserves FC-to-USB frames and
  heartbeat boundaries; intentional USB-to-UART overload reports 524640
  discarded bytes, with exact sent/pending/discarded byte accounting.
- Cppcheck checks all 11 bridge-profile project translation units without
  diagnostics after excluding two `comparePointers` reports at CMSIS
  `cmsis_gcc.h:151` and `:157` (linker-defined startup table boundaries).
- Bridge BIN: 85072 bytes; SHA-256
  `00f317868e06c5879e993b37db4f349da3c86f1544689fee11fdb3ac19bfe6b2`.
  Initial MSP is `0x2401D1A0` in AXI RAM; reset vector is `0x08020331`.
  Linked symbols exclude sensor/control tasks and servo/I2C/SPI initialization.
- The 27676-byte boot BIN retains SHA-256
  `c56303ce51eacb8b78aed464e4032ce18ed11e96a7d759b9fbeb374ffcce8c57`,
  matching the accepted task-runtime image.
- Software checks PASS. The user later deployed M2 and supplied the hardware
  results above; full hardware acceptance remains pending.
