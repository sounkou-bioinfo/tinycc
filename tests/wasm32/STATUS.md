# Upstream-refresh staging snapshot

## Source lineage

- Published fork `main` at `1bed0f33`, which is also the commit Rtinycc vendors.
- Upstream `mob` fetched at `0f004a08`. Merge `69c9e88f` keeps the fork history
  and adds 22 upstream commits from 2026-09-06 to 2026-11-18:

  | Area | Commits | Relevance |
  | --- | --- | --- |
  | Type qualifiers | `4666db32` `0cd015b4` `5f1425ef` `1f214e13` `48f54e59` `c9829912` `f4d81ced` `3dc99dbc` | Qualified array types and parameters match GCC; minimal `restrict`. Frontend, shared by wasm32. |
  | `_Optional` | `8b58a07b` `0f004a08` | New qualifier and its documentation. |
  | tccgen fixes | `9db1105c` `2d3eea20` `c638ddfb` | `type_to_str` buffer size, chained designators, pointer-to-float conversions rejected. |
  | Runtime/linker | `e23dabed` `43c7708b` `32d1c0a5` | `stdatomic` store/fences, musl static link, x86_64 long double to int. |
  | arm64 | `70adbb12` `aade7baf` `cadc3387` `fa97aa73` `8c8741e0` `f830a169` | `sp` offsets of 4096 and more; new assembler instructions. |

The batch was merged whole rather than cherry-picked: it applied without
conflicts, and the frontend changes are covered by the native suite and by the
wasm32 differential cases, which compile through the same tccgen. The arm64
and musl changes are not exercised on this host; other platform CI remains
necessary.

The work lives on the local `wasm/upstream-refresh-2` branch. No remote branch
or Rtinycc vendor pin was changed.

## Validated scope

- Clean Linux x86_64 native build and `make test`, including bounds checking:
  `ALL TESTS PASSED` (C90 declaration warnings treated as errors).
- 199 scalar/control-flow/direct-call/pointer-memory comparisons against
  native TinyCC and, independently, GCC. Includes float edge cases, 9-/32-argument
  calls, and `char`/`short`/`int`/`float`/`double` loads and stores through
  pointer arguments with out-of-bounds guards.
- Nine explicit rejection checks, including wide integer operations, DuckDB's
  i64 result ABI, TLS, computed goto and oversized signatures.
- Core module validation, Emscripten side-module comparison and `dlopen`, four
  Chromium execution tests, and one compiler-in-Chromium test covering repeated
  compilation, scalar calls and recovery after rejection.
- ASan/UBSan/LSan: 64 native-hosted libtcc wasm states, alternating successful
  and rejected compilation, with repeated in-memory output. TWIR names are
  state-owned and released by `tcc_delete()`.
- Rtinycc `375fa95` copied into a temporary package tree, with its vendor archive
  replaced by exact local commit `ed5bdbe0`: source tarball build, private-library
  installation, and all **858 native tinytests passed** under R 4.6.0. The actual
  Rtinycc checkout/pin was untouched. This was not a webR or CRAN check.

Local execution used GCC 13.3 and Node 24.14.1; CI retains its Node 24.18.1 pin.
Emscripten Docker and Playwright use the existing immutable image/lockfile pins.
These are local results, not a claim that remote CI or webR integration ran.
Tree-sitter anti-slop cannot parse TinyCC's `ST_FUNC`/`ST_DATA` macro declarations
(before or after this change); the plain C API fixture parses with no findings.
Existing non-C90 compiler warnings remain; this is not a warning-free build.

## Next dependency-ordered work

1. Implement an addressable shadow stack and data/relocation model: `&local`,
   variable-index local arrays, globals and string literals, with aliasing and
   restoration across calls/returns. Pointer-argument loads/stores at every
   scalar width already pass in `differential/cases/memory.c`; extend that
   manifest for each new storage class.
2. Reconcile wasm i64 values/call signatures with the frontend's 32-bit split
   registers. Restore DuckDB `idx_t` calls only with ABI tests, not mock i32
   results. Resolve the long-double ABI discrepancy explicitly.
3. Add indirect/table calls and aggregate/variadic ABI support, then exercise
   real webR/Rtinycc and DuckDB-wasm integrations.

The scalar milestone is not a general C memory model or a working Rtinycc wasm
FFI. The program-counter dispatcher prioritizes correctness over optimized
structured control flow; a later stackification pass can replace it behind the
same tests.
