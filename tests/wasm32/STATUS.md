# Upstream-refresh staging snapshot

## Source lineage

- Published fork fetched at `54062ef0`; Rtinycc currently vendors `76cbba94`.
  The intervening fork patch fixes signed multicharacter-shift overflow.
- Upstream `mob` fetched at `0fb54300`. Merge `2868255c` retains the published
  fork history and adds these five upstream commits:

  | Commit | Relevance |
  | --- | --- |
  | `128e7893` | Unifies ELF NOTE sections. |
  | `64552b3f` | Corrects linker boundary-symbol handling (`end`/`_end`). |
  | `2ba12e83` | Updates compiler documentation to current behavior. |
  | `1bcec040` | Native TLS/ELF layout fixes; does not implement wasm TLS. |
  | `0fb54300` | Faster native bounds checking. |

All five were merged together rather than cherry-picked: this is a small,
coherent upstream increment, and the native suite exercises the relevant TLS,
linker-symbol and bounds-check paths. Other platform CI remains necessary.

The original dirty checkout was not reset or rebased. Its control-flow and
hosted-compiler work was copied into the separate `wasm/upstream-refresh`
worktree and completed in `ed5bdbe0`. No remote branch or Rtinycc vendor pin was
changed.

## Validated scope

- Clean Linux x86_64 native build and `make test`, including bounds checking:
  `ALL TESTS PASSED` (C90 declaration warnings treated as errors).
- 166 scalar/control-flow/direct-call comparisons against native TinyCC and,
  independently, GCC. Includes float edge cases and 9-/32-argument calls.
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

1. Implement an addressable shadow stack and data/relocation model, including
   byte/short memory accesses, aliasing and restoration across calls/returns.
   Extend `pointer_sum.c` into native/wasm memory-model differential tests.
2. Reconcile wasm i64 values/call signatures with the frontend's 32-bit split
   registers. Restore DuckDB `idx_t` calls only with ABI tests, not mock i32
   results. Resolve the long-double ABI discrepancy explicitly.
3. Add indirect/table calls and aggregate/variadic ABI support, then exercise
   real webR/Rtinycc and DuckDB-wasm integrations.

The scalar milestone is not a general C memory model or a working Rtinycc wasm
FFI. The program-counter dispatcher prioritizes correctness over optimized
structured control flow; a later stackification pass can replace it behind the
same tests.
