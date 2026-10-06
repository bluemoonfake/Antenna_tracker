#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
DEPS_DIR=${FREERTOS_DEPS_DIR:-"${REPO_ROOT}/.deps"}

fetch_repo()
{
  local name=$1
  local url=$2
  local revision=$3
  local path="${DEPS_DIR}/${name}"

  if [ -d "${path}/.git" ] &&
     [ "$(git -C "${path}" rev-parse HEAD)" = "${revision}" ]; then
    return
  fi

  if [ -e "${path}" ]; then
    echo "Dependency path exists at the wrong revision: ${path}" >&2
    echo "Remove only that directory, then run this script again." >&2
    exit 2
  fi

  echo ">>> Fetching ${name}"
  mkdir -p "${DEPS_DIR}"
  git init -q "${path}"
  git -C "${path}" remote add origin "${url}"
  git -C "${path}" fetch -q --depth 1 origin "${revision}"
  git -C "${path}" checkout -q --detach FETCH_HEAD
}

# STM32CubeH7 v1.13.0 component revisions plus the current upstream FreeRTOS
# kernel.  Commit IDs make firmware builds reproducible even if tags move.
fetch_repo cmsis_5 \
  https://github.com/ARM-software/CMSIS_5.git \
  2b7495b8535bdcb306dac29b9ded4cfb679d7e5c
fetch_repo cmsis_device_h7 \
  https://github.com/STMicroelectronics/cmsis_device_h7.git \
  8f922cdc7cc6de2344e75ddd657889f4ff761790
fetch_repo stm32h7xx_hal_driver \
  https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git \
  a1996eed9172b59887bafaaa0ea1816ea14d48b5
fetch_repo freertos_kernel \
  https://github.com/FreeRTOS/FreeRTOS-Kernel.git \
  9b777ae5c5b8e9e456065a00294d1e5f5f9facf5
fetch_repo stm32_usb_device \
  https://github.com/STMicroelectronics/stm32-mw-usb-device.git \
  0b06460a43c30670a764cb7e58c5a7918e789f6e

echo ">>> Dependencies are ready in ${DEPS_DIR}"
