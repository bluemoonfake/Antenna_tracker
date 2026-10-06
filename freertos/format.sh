#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODE=${1:-write}

case "${MODE}" in
  write|--check) ;;
  *)
    echo "Usage: $0 [write|--check]" >&2
    exit 2
    ;;
esac

mapfile -d '' FILES < <(
  find "${SCRIPT_DIR}" -type f \( -name '*.c' -o -name '*.h' \) -print0
)

if [[ "${MODE}" == "--check" ]]; then
  clang-format --style=file --dry-run --Werror "${FILES[@]}"
else
  clang-format --style=file -i "${FILES[@]}"
fi
