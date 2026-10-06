#!/usr/bin/env python3
"""Check M2 USB -> UART1 -> USB with TX1 physically joined to RX1."""
import argparse
from collections import deque
import os
import select
import struct
import termios
import time


def crc_x25(data, extra):
    crc = 0xFFFF
    for byte in data + bytes([extra]):
        tmp = byte ^ (crc & 0xFF)
        tmp ^= (tmp << 4) & 0xFF
        crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    return crc


def position_frame(index, bad_crc=False, *, sysid=1, latitude_e7=110000000,
                   longitude_e7=1060000000, altitude_mm=23700):
    # GLOBAL_POSITION_INT: coordinates in 1e-7 degrees, altitude in mm.
    payload = struct.pack("<IiiiihhhH", index & 0xFFFFFFFF, latitude_e7,
                          longitude_e7, altitude_mm, 0, 0, 0, 0, 0)
    header = bytes([len(payload), 0, 0, index & 0xFF, sysid, 1, 33, 0, 0])
    crc = crc_x25(header + payload, 104) ^ int(bad_crc)
    return b"\xfd" + header + payload + struct.pack("<H", crc)


def extract_frames(buffer):
    frames = []
    while buffer:
        if buffer[0] not in (0xFD, 0xFE):
            raise RuntimeError(
                f"Unexpected byte 0x{buffer[0]:02x} at a frame boundary; "
                f"RX prefix: {buffer[:64].hex(' ')}")
        if len(buffer) < 3:
            break
        length = buffer[1] + (12 if buffer[0] == 0xFD else 8)
        if buffer[0] == 0xFD and buffer[2] & 1:
            length += 13
        if len(buffer) < length:
            break
        frames.append(bytes(buffer[:length]))
        del buffer[:length]
    return frames


def drain_startup(fd, quiet_seconds=0.2, maximum_seconds=3.0):
    # Opening a CDC port can resume queued USB transfers at an arbitrary
    # byte position. Drain old traffic before sending any test frame.
    deadline = time.monotonic() + maximum_seconds
    discarded = 0
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("No quiet startup interval; disconnect the external "
                               "FC and close other serial programs")
        if not select.select([fd], [], [], min(quiet_seconds, remaining))[0]:
            if remaining < quiet_seconds:
                raise RuntimeError("Startup drain timed out")
            return discarded
        try:
            data = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not data:
            raise RuntimeError("USB disconnected during startup drain")
        discarded += len(data)


def write_all(fd, data):
    while data:
        if not select.select([], [fd], [], 2.0)[1]:
            raise RuntimeError("USB write timed out")
        try:
            count = os.write(fd, data)
        except BlockingIOError:
            continue
        data = data[count:]


def run(args):
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    original = termios.tcgetattr(fd)
    expected, buffer = deque(), bytearray()
    sent = received = heartbeats = bad_frames = 0
    try:
        raw = termios.tcgetattr(fd)
        raw[:4] = [0, 0, termios.CS8 | termios.CREAD | termios.CLOCAL, 0]
        raw[4:6] = [termios.B57600, termios.B57600]
        raw[6][termios.VMIN] = raw[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, raw)
        discarded = drain_startup(fd)
        print(f"Startup: drained {discarded} pre-test bytes; "
              "strict loopback measurement starts now", flush=True)
        start = time.monotonic()
        next_send, end = start, start + args.seconds
        while time.monotonic() < end + 3.0:
            now = time.monotonic()
            if now < end and now >= next_send:
                bad = args.bad_every > 0 and (sent + 1) % args.bad_every == 0
                frame = position_frame(sent, bad, sysid=args.sysid,
                                       latitude_e7=args.latitude_e7,
                                       longitude_e7=args.longitude_e7,
                                       altitude_mm=args.altitude_mm)
                expected.append(frame)
                if args.fragment:
                    write_all(fd, frame[:5])
                    time.sleep(0.005)
                    write_all(fd, frame[5:])
                else:
                    write_all(fd, frame)
                sent += 1
                bad_frames += int(bad)
                next_send += 1.0 / args.rate
            if select.select([fd], [], [], 0.005)[0]:
                try:
                    data = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                if not data:
                    raise RuntimeError("USB disconnected")
                buffer.extend(data)
                for frame in extract_frames(buffer):
                    if expected and frame == expected[0]:
                        expected.popleft()
                        received += 1
                    elif (frame[0] == 0xFD and frame[5] == 1 and frame[6] == 191
                          and frame[7:10] == b"\0\0\0"
                          and crc_x25(frame[1:-2], 50)
                          == int.from_bytes(frame[-2:], "little")):
                        heartbeats += 1
                    else:
                        wanted = expected[0].hex(' ') if expected else "none"
                        raise RuntimeError(
                            f"Unexpected frame: {frame.hex(' ')}; "
                            f"next expected echo: {wanted}")
            if now >= end and not expected and not buffer:
                break
        if expected or buffer or received != sent:
            raise RuntimeError(f"Missing echo: sent={sent}, received={received}, pending={len(expected)}")
        if args.seconds >= 3.0 and heartbeats == 0:
            raise RuntimeError("No tracker heartbeat received")
        print(f"PASS: {received}/{sent} exact echoes, {bad_frames} bad-CRC frames "
              f"preserved, {heartbeats} tracker heartbeats")
    except RuntimeError as error:
        raise RuntimeError(
            f"{error}; sent={sent}, echoed={received}, "
            f"heartbeats={heartbeats}, pending_echoes={len(expected)}") from error
    finally:
        try:
            termios.tcsetattr(fd, termios.TCSANOW, original)
        finally:
            os.close(fd)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--rate", type=float, default=50.0)
    parser.add_argument("--fragment", action="store_true")
    parser.add_argument("--bad-every", type=int, default=0)
    parser.add_argument("--sysid", type=int, default=1,
                        help="MAVLink sender system ID (1..255; default: 1)")
    parser.add_argument("--latitude-e7", type=int, default=110000000,
                        help="Latitude in 1e-7 degrees (default: 110000000)")
    parser.add_argument("--longitude-e7", type=int, default=1060000000,
                        help="Longitude in 1e-7 degrees (default: 1060000000)")
    parser.add_argument("--altitude-mm", type=int, default=23700,
                        help="MSL altitude in mm (default: 23700)")
    options = parser.parse_args()
    if options.seconds <= 0 or options.rate <= 0 or options.bad_every < 0:
        parser.error("seconds/rate must be positive and bad-every nonnegative")
    if not 1 <= options.sysid <= 255:
        parser.error("sysid must be between 1 and 255")
    if not -900000000 <= options.latitude_e7 <= 900000000:
        parser.error("latitude-e7 must be between -900000000 and 900000000")
    if not -1800000000 <= options.longitude_e7 <= 1800000000:
        parser.error("longitude-e7 must be between -1800000000 and 1800000000")
    if not -(1 << 31) <= options.altitude_mm < (1 << 31):
        parser.error("altitude-mm must fit in a signed 32-bit integer")
    try:
        run(options)
    except (RuntimeError, OSError) as error:
        parser.exit(1, f"FAIL: {error}\n")
