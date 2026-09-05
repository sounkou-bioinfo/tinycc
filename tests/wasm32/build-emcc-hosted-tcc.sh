#!/usr/bin/env bash
set -euo pipefail

# Build libtcc *hosted inside wasm* with emcc: the resulting module is the
# TinyCC compiler itself running under Emscripten, able to compile C to a
# wasm32 side module at runtime and dlopen it.  Produces tcc-in-wasm.mjs
# (+ .wasm), runnable in Node and in the browser (MAIN_MODULE so it can
# dlopen the side modules tcc emits).
#
# Run inside an emscripten/emsdk container with the repo mounted at /src.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

OUT="${1:-tests/wasm32/browser/tcc-in-wasm.mjs}"
mkdir -p "$(dirname "$OUT")"

# TinyCC compiled with emcc: __wasm32__ auto-selects the wasm32 backend, but we
# set it explicitly so config.h does not fall back to x86_64.  ONE_SOURCE pulls
# the whole compiler in through libtcc.c.
emcc \
  -O2 \
  -DTCC_TARGET_WASM32 \
  -DONE_SOURCE=1 \
  -DCONFIG_TCC_PREDEFS=1 \
  -DCONFIG_TCCDIR="\"/tcc\"" \
  -I. \
  libtcc.c tests/wasm32/tcc_wasm_driver.c \
  -sMAIN_MODULE=2 \
  -sALLOW_TABLE_GROWTH=1 \
  -sINITIAL_MEMORY=134217728 \
  -sEXPORTED_FUNCTIONS='["_main","_tcc_wasm_compile_call2","_tcc_wasm_last_error","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToNewUTF8"]' \
  -sEXPORT_ES6=1 \
  -sMODULARIZE=1 \
  -sENVIRONMENT=node,web \
  -o "$OUT"

echo "EMCC_HOSTED_TCC_BUILD_OK -> $OUT"
