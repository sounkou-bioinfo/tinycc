# wasm32 backend smoke tests

Build a wasm32 TinyCC after applying the series:

```sh
./configure --config-backtrace=no --config-bcheck=no
make cross-wasm32-emscripten ONE_SOURCE=yes
```

Run the wasm32 validation target:

```sh
make test-wasm32-emscripten
```

Or run the smoke entrypoint directly:

```sh
tests/wasm32/run-core-smoke.sh
```

The smoke path validates generated wasm binaries with `WebAssembly.validate`,
parses their section/type shape through `tests/wasm32/wasm-inspect.mjs`, and
keeps a SIMD fixture to prove the validator reports the `simd128` value-type
mnemonic.

Emscripten-only checks run in Docker when host `emcc` is unavailable; set
`TCC_WASM_EMCC_DOCKER_IMAGE=ghcr.io/r-wasm/webr:main` to use the webR image as
the emcc container.

Compile the first straight-line scalar test manually:

```sh
./wasm32-emscripten-tcc -B. -nostdlib -shared -o add.wasm tests/wasm32/add.c
node tests/wasm32/node-smoke.mjs add.wasm
```

Runtime import smoke test.  This intentionally calls malloc/free,
memcpy/memmove/memset/strlen and the optional math imports.  The Node harness
provides an env import object with one shared WebAssembly.Memory:

```sh
./wasm32-emscripten-tcc -B. -nostdlib -shared \
  -o runtime_imports.wasm tests/wasm32/runtime_imports.c
node tests/wasm32/node-smoke.mjs runtime_imports.wasm
```

libtcc1 smoke test.  This deliberately omits `-nostdlib` so the wasm32 runtime
helper source is added to the generated module.  The build also produces a
`wasm32-emscripten-libtcc1.a` release artifact for the next archive/object-loader
iteration:

```sh
./wasm32-emscripten-tcc -B. -shared \
  -o libtcc1_helpers.wasm tests/wasm32/libtcc1_helpers.c
node tests/wasm32/node-smoke.mjs libtcc1_helpers.wasm
```

The expected first green milestone is `add(2, 40) == 42` from a real wasm
binary produced directly by TinyCC's wasm32 backend.

Known v0.1.0-alpha limits:

- i32 and f64 straight-line scalar functions are the validated path.
- wasm32 libtcc1 currently ships validated 32-bit bit-operation helpers only.
- i64 lowering, byte-sized C lvalue loads/stores, data segments, and structured
  control flow are intentionally tracked as follow-up work; `pointer_sum.c` is
  included to drive that next phase.

Golden side-module comparison:

```sh
emcc -O0 -sSIDE_MODULE=2 -shared tests/wasm32/add.c -o emcc-add-golden.wasm
node tests/wasm32/compare-wasm-shape.mjs add.wasm emcc-add-golden.wasm
```

Emscripten dynamic-link harness:

```sh
emcc -O0 tests/wasm32/dlopen-main.c \
  -sMAIN_MODULE=2 \
  -sALLOW_TABLE_GROWTH=1 \
  --preload-file add.wasm@/add.wasm \
  -o dlopen-main.mjs
node dlopen-main.mjs
```

Browser/Playwright execution smoke:

```sh
tests/wasm32/run-browser-smoke.sh
```

Set `TCC_WASM_INSTALL_BROWSER=1` to make the script install the Chromium
browser dependency via Playwright before running the tests.

Integration jobs:

The GitHub workflow includes opt-in `workflow_dispatch` jobs for webR/Rtinycc
and DuckDB-wasm/DuckTinyCC.  They are not enabled on every pull request because
they depend on external project repos/artifacts, but they consume the same
`wasm32-emscripten-tcc`, `wasm32-emscripten-libtcc1.a`, and generated wasm
side modules produced by the core wasm32 job.
