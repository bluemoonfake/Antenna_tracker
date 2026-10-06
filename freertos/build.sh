#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
PROFILE=${1:-boot}
case "${PROFILE}" in
  boot|bridge|recovered) ;;
  *) echo "Unsupported profile: ${PROFILE}. Use boot, bridge or recovered." >&2; exit 2 ;;
esac
BUILD_DIR=${BUILD_DIR:-"${REPO_ROOT}/build/freertos-h743-${PROFILE}"}
TARGET_SYSTEM_ID=${TRACKER_TARGET_SYSTEM_ID:-1}
SERVO_OUTPUT=${TRACKER_ENABLE_SERVO_OUTPUT:-OFF}

"${SCRIPT_DIR}/fetch_dependencies.sh"

cmake --fresh \
  -S "${SCRIPT_DIR}" \
  -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/arm-none-eabi.cmake" \
  -DTRACKER_PROFILE="${PROFILE}" \
  -DTRACKER_FC_BAUD_RATE="${FC_BAUD_RATE:-57600}" \
  -DTRACKER_TARGET_SYSTEM_ID="${TARGET_SYSTEM_ID}" \
  -DTRACKER_ENABLE_SERVO_OUTPUT="${SERVO_OUTPUT}"
cmake --build "${BUILD_DIR}"

echo ">>> ELF: ${BUILD_DIR}/microair_h743_tracker_freertos.elf"
echo ">>> BIN: ${BUILD_DIR}/microair_h743_tracker_freertos.bin"
