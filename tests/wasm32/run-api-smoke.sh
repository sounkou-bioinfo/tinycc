#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/tcc-wasm-api-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
cd "$ROOT"
FLAGS=(-O1 -g -Wall -Werror=declaration-after-statement)
if [[ "${TCC_WASM_SANITIZE:-0}" == 1 ]]; then
  FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
fi
"${CC:-cc}" "${FLAGS[@]}" -I. -DONE_SOURCE=1 \
  -DTCC_TARGET_WASM32 -DTARGETOS_Emscripten \
  libtcc.c tests/wasm32/api-smoke.c -lm -ldl -lpthread -o "$WORK/api-smoke"
"$WORK/api-smoke"
