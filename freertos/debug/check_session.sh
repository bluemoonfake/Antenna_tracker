#!/usr/bin/env bash
set -euo pipefail

debug_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
debug_config="${debug_dir}/stlink-h743.cfg"

for debug_pid in $(pgrep -x openocd || true); do
  if rg --text --fixed-strings --quiet -- "${debug_config}" \
      "/proc/${debug_pid}/cmdline" 2>/dev/null; then
    printf 'An SWD debug server for this repo already exists (PID %s).\n' \
      "${debug_pid}" >&2
    printf 'Use that session, or press Stop before starting another.\n' >&2
    exit 1
  fi
done
