import fs from 'node:fs/promises';
import path from 'node:path';
import { test, expect } from '@playwright/test';

const readBytes = async (p) => Array.from(await fs.readFile(path.resolve(p)));

async function instantiateInBrowser(page, wasmPath) {
  const bytes = await readBytes(wasmPath);
  return page.evaluate(async (arr) => {
    const memory = new WebAssembly.Memory({ initial: 2 });
    const heap = () => new Uint8Array(memory.buffer);
    let bump = 4096;
    const malloc = (n) => { const p = bump; bump = (bump + Number(n) + 7) & -8; return p; };
    const memcpy = (dst, src, n) => { heap().copyWithin(dst, src, src + n); return dst; };
    const memset = (dst, c, n) => { heap().fill(c & 255, dst, dst + n); return dst; };
    const strlen = (ptr) => { const h = heap(); let p = ptr; while (h[p]) ++p; return p - ptr; };
    const imports = { env: {
      memory,
      __indirect_function_table: new WebAssembly.Table({ element: 'anyfunc', initial: 32 }),
      malloc,
      free() {},
      calloc(nmemb, size) { const n = Number(nmemb) * Number(size); const p = malloc(n); heap().fill(0, p, p + n); return p; },
      realloc(_p, n) { return malloc(n); },
      memcpy,
      memmove: memcpy,
      memset,
      memcmp() { return 0; },
      strlen,
      sqrt: Math.sqrt,
      sin: Math.sin,
      cos: Math.cos,
      exp: Math.exp,
      log: Math.log,
      sqrtf: Math.sqrt,
      sinf: Math.sin,
      cosf: Math.cos,
      R_NilValue: () => 0,
      Rf_length: () => 0,
      Rf_protect: (x) => x,
      Rf_unprotect() {},
      Rf_error() { throw new Error('Rf_error'); },
      duckdb_data_chunk_get_size: () => 0,
      duckdb_data_chunk_get_vector: () => 0,
      duckdb_vector_get_data: () => 0,
      duckdb_malloc: malloc,
      duckdb_free() {},
      duckdb_scalar_function_set_error() {},
    }};
    const instance = await WebAssembly.instantiate(new Uint8Array(arr), imports);
    return Object.fromEntries(Object.entries(instance.instance.exports).map(([k, v]) => [k, typeof v]));
  }, bytes);
}

test('generated add module instantiates in Chromium', async ({ page }) => {
  const exports = await instantiateInBrowser(page, process.env.TCC_WASM_ADD ?? '../../../add.wasm');
  expect(exports.add ?? exports._add).toBe('function');
});

test('runtime import module instantiates in Chromium', async ({ page }) => {
  const exports = await instantiateInBrowser(page, process.env.TCC_WASM_RUNTIME ?? '../../../runtime_imports.wasm');
  expect(exports.runtime_imports ?? exports._runtime_imports).toBe('function');
});

test('libtcc1 helper module instantiates in Chromium', async ({ page }) => {
  const exports = await instantiateInBrowser(page, process.env.TCC_WASM_LIBTCC1 ?? '../../../libtcc1_helpers.wasm');
  expect(exports.tcc1_marker ?? exports._tcc1_marker).toBe('function');
});
