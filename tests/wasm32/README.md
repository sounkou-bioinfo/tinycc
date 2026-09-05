# wasm32 backend smoke tests

See [STATUS.md](STATUS.md) for the upstream-refresh lineage, gate evidence and
remaining integration work.

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
parses their section/type shape through `tests/wasm32/wasm-inspect.mjs`, keeps a
SIMD fixture to prove the validator reports the `simd128` value-type mnemonic,
and runs `tests/wasm32/run-differential-smoke.mjs` to compare curated wasm32
exports against the same C compiled by native TinyCC.

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

Current staging scope (beyond v0.1.0-alpha):

- i32, f32 and f64 scalar arithmetic, comparisons, negation and conversions are
  exercised against native TinyCC and GCC. Floating comparisons include NaNs,
  infinities, signed zero and negated branches. Float-to-integer tests use only
  values whose truncated result is representable (out-of-range C casts are UB).
- Direct local function calls (including chained/nested, e.g. `g(a)+g(b)`) are
  validated, including forward definitions and mixed-type calls. Calls use
  scratch registers without clobbering pending arguments; the current signature
  limit is 32 parameters/arguments, with larger signatures rejected.
- Structured control flow (`if`/`else`, `for`, `while`, `switch`, recursion) is
  validated. Comparisons defer through `VT_CMP` (like riscv64), `gjmp`/
  `gjmp_cond` honour `nocode_wanted`, and `wasm32-emit.c` lowers the TWIR jump
  graph to wasm via a `br_table` program-counter dispatch loop. See
  `differential/cases/control.c`.
- wasm32 libtcc1 currently ships validated 32-bit bit-operation helpers only.
- Runtime `long long` lowering is rejected: the frontend still uses split
  registers on 32-bit targets, so merely emitting wasm i64 instructions is not
  sufficient. This includes DuckDB `idx_t` results: their old generic smoke
  call is now a rejection test, not a supposed working import. TLS and computed
  goto are also rejected with diagnostics.
- Still follow-up: addressable locals / general pointers / globals / string
  literals (need a shadow stack, data segments and relocations). A passing
  constant-index local-array example is not evidence of a C memory model.
  `pointer_sum.c` drives that phase.
- Aggregate, variadic and indirect-call ABIs are not validated. The initial
  backend's `long double` model is an eight-byte alias, not Emscripten's full
  long-double ABI. This is not yet a general webR/Rtinycc FFI implementation.

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

Native TinyCC vs wasm32 differential smoke:

```sh
make test-wasm32-differential
```

The differential manifest lives in `tests/wasm32/differential/cases.json`.
It covers the supported scalar/control-flow subset, not the full TinyCC suite.
To also use GCC as the native oracle:

```sh
TCC_NATIVE_TCC=gcc node tests/wasm32/run-differential-smoke.mjs
node tests/wasm32/reject-unsupported.mjs
```

The runner preserves C `printf` spellings for infinities/NaNs and signed zero.
The core gate also runs `tests/wasm32/run-api-smoke.sh`: 64 success/error states
and repeated memory output. Use `TCC_WASM_SANITIZE=1` to enable ASan/UBSan (and
LSan on Linux); CI enables this. TWIR names live until `tcc_delete()`, so repeated
emission can reuse names and failed compilations release their allocations.
Unsupported-feature rejection tests are separate from positive differential
checks; neither failed compilation nor a runtime trap counts as a match.

Browser/Playwright execution smoke:

```sh
tests/wasm32/run-browser-smoke.sh
```

Set `TCC_WASM_INSTALL_BROWSER=1` to make the script install the Chromium
browser dependency via Playwright before running the tests. The smoke uses the
committed npm lockfile and installs dependencies with `npm ci`.

Compiler hosted inside wasm:

```sh
make test-wasm32-hosted
```

This rebuilds libtcc as an Emscripten MAIN_MODULE, then compiles C, emits a side
module, `dlopen`s it and calls it entirely inside Chromium. It checks scalar
casts, comparisons, control flow, repeated states and recovery after an
unsupported compilation. The build uses local `emcc` when available, otherwise
an immutable Emscripten Docker image; Playwright uses `npm ci` and the lockfile.
The normal wasm CI job runs both native-hosted and browser-hosted compiler gates.

Integration jobs:

The GitHub workflow includes opt-in `workflow_dispatch` jobs for webR/Rtinycc
and DuckDB-wasm/DuckTinyCC.  They are not enabled on every pull request because
they depend on external project repos/artifacts, but they consume the same
`wasm32-emscripten-tcc`, `wasm32-emscripten-libtcc1.a`, and generated wasm
side modules produced by the core wasm32 job.
