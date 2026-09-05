import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const tcc = process.env.TCC_WASM_TCC ?? path.join(root, 'wasm32-emscripten-tcc');
const cases = [
  ['long long arithmetic', 'int f(int a, int b) { long long x = (long long)a*b; return (int)(x+2); }', 'long long lowering is not implemented'],
  ['long long parameter', 'int f(long long a) { return (int)a; }', 'long long lowering is not implemented'],
  ['long long return', 'long long f(int a) { return a; }', 'long long lowering is not implemented'],
  ['float to long long', 'int f(double a) { return (int)(long long)a; }', 'long long lowering is not implemented'],
  ['DuckDB i64 result', '#include <ducktinycc_host.h>\nint f(duckdb_data_chunk p) { return (int)duckdb_data_chunk_get_size(p); }', 'long long lowering is not implemented'],
  ['thread-local storage', '__thread int x; int f(void) { return x; }', 'thread-local storage'],
  ['computed goto', 'int f(void *p) { goto *p; }', 'computed goto is not implemented'],
];
const params = Array.from({ length: 33 }, (_, i) => `int p${i}`).join(',');
cases.push(
  ['parameter limit', `int f(${params}) { return p0; }`, 'at most 32 parameters'],
  ['argument limit', `int g(${params}); int f(void) { return g(${Array(33).fill(1).join(',')}); }`, 'at most 32 arguments'],
);
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'tcc-wasm-reject-'));
try {
  for (const [name, source, diagnostic] of cases) {
    const cfile = path.join(work, 'case.c');
    const output = path.join(work, 'case.wasm');
    fs.writeFileSync(cfile, source + '\n');
    const result = spawnSync(tcc, ['-B', root, '-nostdlib', '-shared', '-o', output, cfile], {
      encoding: 'utf8', timeout: 30000,
    });
    assert.ifError(result.error);
    assert.equal(result.signal, null, `${name}: compiler crashed`);
    assert.notEqual(result.status, 0, `${name}: unsupported code accepted`);
    assert.ok(result.stderr.includes(diagnostic), `${name}: ${result.stderr}`);
    assert.ok(!fs.existsSync(output), `${name}: emitted an unsupported module`);
    console.log(`ok wasm32 rejection: ${name}`);
  }
} finally {
  fs.rmSync(work, { recursive: true, force: true });
}
console.log(`WASM32_REJECTION_OK ${cases.length} checks`);
