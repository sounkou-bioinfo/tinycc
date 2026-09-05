#!/usr/bin/env bash
set -euo pipefail

# Compiler-in-browser smoke: build libtcc *hosted inside wasm* with emcc (in a
# Docker emscripten container), then drive it in real Chromium via Playwright.
# TinyCC compiles C to a wasm32 side module inside the browser, dlopens it, and
# calls the result -- no native toolchain at runtime.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

DOCKER="${DOCKER:-docker}"
IMAGE="${TCC_WASM_EMCC_DOCKER_IMAGE:-emscripten/emsdk:6.0.5@sha256:76a44fff907397784decc435115d07fcb9587a4f1504977f39f3745e538e3a1e}"

# Rebuild by default: existing artifacts may have come from an older backend.
if command -v emcc >/dev/null 2>&1; then
  tests/wasm32/build-emcc-hosted-tcc.sh
else
  if ! command -v "$DOCKER" >/dev/null 2>&1; then
    echo "error: docker not found; cannot build emcc-hosted tcc" >&2
    exit 1
  fi
  if ! "$DOCKER" image inspect "$IMAGE" >/dev/null 2>&1; then
    "$DOCKER" pull "$IMAGE"
  fi
  echo "building emcc-hosted tcc with image: $IMAGE"
  "$DOCKER" run --rm -v "$ROOT:/src" -w /src "$IMAGE" \
    bash -lc 'tests/wasm32/build-emcc-hosted-tcc.sh'
fi

cd "$ROOT/tests/wasm32/browser"
npm ci
if [[ "${TCC_WASM_INSTALL_BROWSER:-0}" == 1 ]]; then
  npx playwright install --with-deps chromium
fi

npx --no-install playwright test tcc-in-browser.spec.mjs
echo TCC_IN_BROWSER_SMOKE_OK
