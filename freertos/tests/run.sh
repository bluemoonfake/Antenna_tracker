#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "${OUT}"' EXIT
PYTHONDONTWRITEBYTECODE=1 python3 "${SCRIPT_DIR}/bridge_loopback_test.py"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -fno-omit-frame-pointer -I"${REPO_ROOT}/freertos/include" \
   "${REPO_ROOT}/freertos/src/gps_ubx.c" \
   "${SCRIPT_DIR}/gps_ubx_test.c" -o "${OUT}/gps-ubx"
"${OUT}/gps-ubx"
c++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${REPO_ROOT}/freertos/compat" \
    -I"${REPO_ROOT}/common/attitude" \
    "${REPO_ROOT}/common/attitude/attitude.cpp" \
    "${SCRIPT_DIR}/attitude_test.cpp" -lm -o "${OUT}/attitude"
"${OUT}/attitude"
SENSOR_FLAGS=(-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined
              -fno-omit-frame-pointer -fno-pie
              -I"${SCRIPT_DIR}/recovered_stubs" -I"${REPO_ROOT}/freertos/include"
              -I"${REPO_ROOT}/freertos/compat" -I"${REPO_ROOT}/common/attitude")
cc "${SENSOR_FLAGS[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections -no-pie \
   "${SCRIPT_DIR}/recovered_board_test.c" -o "${OUT}/recovered-board"
"${OUT}/recovered-board"
for source in sensor_task bmi088_spi app_state gps_ubx; do
  cc "${SENSOR_FLAGS[@]}" -c "${REPO_ROOT}/freertos/src/${source}.c" -o "${OUT}/${source}.o"
done
cc "${SENSOR_FLAGS[@]}" -c "${SCRIPT_DIR}/sensor_runtime_test.c" -o "${OUT}/sensor-test.o"
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
    -I"${REPO_ROOT}/freertos/compat" -I"${REPO_ROOT}/common/attitude" \
    "${REPO_ROOT}/common/attitude/attitude.cpp" "${OUT}/"*.o -lm -o "${OUT}/sensor-runtime"
for scenario in 0 1 2; do "${OUT}/sensor-runtime" "${scenario}"; done
FLAGS=(-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie
       -I"${SCRIPT_DIR}/stubs" -I"${REPO_ROOT}/freertos/include"
       -isystem "${REPO_ROOT}/third_party" -DMAVLINK_COMM_NUM_BUFFERS=2
       -DTRACKER_BRIDGE_ONLY=1)
cc "${FLAGS[@]}" "${REPO_ROOT}/freertos/src/bridge_protocol.c" \
   "${SCRIPT_DIR}/bridge_protocol_test.c" -o "${OUT}/protocol"
python3 - "${REPO_ROOT}" "${OUT}/position.bin" <<'PY'
import runpy, pathlib, sys
tool = runpy.run_path(str(pathlib.Path(sys.argv[1]) / 'freertos/tools/bridge_loopback.py'))
frame = tool['position_frame'](257)
pathlib.Path(sys.argv[2]).write_bytes(frame)
pathlib.Path(sys.argv[2]).with_name('position-sysid2.bin').write_bytes(
    tool['position_frame'](257, sysid=2))
buffer = bytearray(frame[:5])
assert tool['extract_frames'](buffer) == []
buffer.extend(frame[5:] + tool['position_frame'](258, True))
assert tool['extract_frames'](buffer) == [frame, tool['position_frame'](258, True)]
assert not buffer
PY
"${OUT}/protocol" "${OUT}/position.bin"
"${OUT}/protocol" "${OUT}/position-sysid2.bin"
cc "${FLAGS[@]}" "${REPO_ROOT}/freertos/src/bridge_protocol.c" \
   "${REPO_ROOT}/freertos/src/mavlink_bridge_task.c" \
   "${SCRIPT_DIR}/bridge_task_test.c" -o "${OUT}/task"
for scenario in 0 1 2 3; do "${OUT}/task" "${scenario}"; done
