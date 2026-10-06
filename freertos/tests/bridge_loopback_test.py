"""Exercise the host checker on a pseudo-terminal, without a board."""
from contextlib import redirect_stdout
import io
import os
from pathlib import Path
import pty
import runpy
import select
import struct
import threading
import tty
from types import SimpleNamespace
from unittest.mock import patch

tool = runpy.run_path(str(Path(__file__).resolve().parents[1] /
                          "tools/bridge_loopback.py"))


def check_serial_session(inject_error, sysid=1):
    master, slave = pty.openpty()
    tty.setraw(slave)
    os.set_blocking(master, False)
    stop = threading.Event()
    errors = []

    def echo():
        transmitted = bytearray()
        try:
            while not stop.is_set():
                if select.select([master], [], [], 0.01)[0]:
                    data = os.read(master, 4096)
                    transmitted.extend(data)
                    for frame in tool["extract_frames"](transmitted):
                        assert frame[5] == sysid
                        assert struct.unpack_from("<iii", frame, 14) == (
                            120000000, 1070000000, 45000)
                    os.write(master, (b"\x00" if inject_error else b"") + data)
        except Exception as error:
            errors.append(error)

    # Reproduce a read starting inside a frame left by an earlier session.
    old_tail = tool["position_frame"](999)[5:]
    os.write(master, old_tail)
    worker = threading.Thread(target=echo)
    worker.start()
    output = io.StringIO()
    args = SimpleNamespace(port=os.ttyname(slave), seconds=0.12, rate=25,
                           fragment=True, bad_every=2, sysid=sysid,
                           latitude_e7=120000000, longitude_e7=1070000000,
                           altitude_mm=45000)
    try:
        with redirect_stdout(output):
            if inject_error:
                try:
                    tool["run"](args)
                except RuntimeError as error:
                    assert "Unexpected byte 0x00" in str(error), error
                    assert "RX prefix:" in str(error), error
                    assert "sent=1, echoed=0" in str(error), error
                else:
                    raise AssertionError("Measurement noise must fail")
            else:
                tool["run"](args)
                assert "PASS:" in output.getvalue(), output.getvalue()
        assert f"drained {len(old_tail)} pre-test bytes" in output.getvalue()
    finally:
        stop.set()
        worker.join(timeout=1)
        os.close(master)
        os.close(slave)
    assert not worker.is_alive()
    assert not errors, errors


check_serial_session(False)
check_serial_session(False, sysid=2)
check_serial_session(True)

frame = tool["position_frame"](257, sysid=2, latitude_e7=120000000,
                               longitude_e7=1070000000, altitude_mm=45000)
assert frame[5] == 2
assert struct.unpack_from("<iii", frame, 14) == (120000000, 1070000000, 45000)

# Continuous startup traffic must not cause an unbounded wait.
with patch("time.monotonic", side_effect=[0.0, 0.0, 3.1]), \
     patch("select.select", return_value=([123], [], [])), \
     patch("os.read", return_value=b"noise"):
    try:
        tool["drain_startup"](123)
    except RuntimeError as error:
        assert "No quiet startup interval" in str(error), error
    else:
        raise AssertionError("Startup timeout was not enforced")

print("PASS: system IDs/custom coordinates, partial startup drain, "
      "fragmented/bad-CRC echoes, "
      "strict measurement failure and bounded startup wait")
