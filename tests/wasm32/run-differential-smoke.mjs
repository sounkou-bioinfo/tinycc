import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createWasmHost } from './wasm-host-runtime.mjs';
import { inspectWasm } from './wasm-inspect.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '../..');
const manifestPath = process.env.TCC_WASM_DIFF_MANIFEST ?? path.join(here, 'differential/cases.json');
const nativeTcc = process.env.TCC_NATIVE_TCC ?? path.join(root, 'tcc');
const wasmTcc = process.env.TCC_WASM_TCC ?? path.join(root, 'wasm32-emscripten-tcc');
const keep = process.env.TCC_WASM_DIFF_KEEP === '1';

function run(argv, options = {}) {
  const [cmd, ...args] = argv;
  const res = spawnSync(cmd, args, {
    cwd: root,
    timeout: 30000,
    encoding: 'utf8',
    stdio: options.capture ? ['ignore', 'pipe', 'pipe'] : 'inherit',
  });
  if (res.status !== 0) {
    const detail = options.capture ? `\nstdout:\n${res.stdout}\nstderr:\n${res.stderr}` : '';
    throw new Error(`${argv.join(' ')} failed with status ${res.status}${detail}`);
  }
  return res.stdout ?? '';
}

function cString(s) {
  return JSON.stringify(s);
}

/* Pointer arguments are spelled "<elem>*" in the manifest and take an array
   of element values.  Each case gets its own buffer; the harness copies it
   into native and wasm memory, makes the call, then compares both the result
   and every buffer element, so stray or narrow stores are caught too. */
const ELEMS = {
  i8: { c: 'signed char', fmt: '%d', view: Int8Array },
  u8: { c: 'unsigned char', fmt: '%d', view: Uint8Array },
  i16: { c: 'short', fmt: '%d', view: Int16Array },
  u16: { c: 'unsigned short', fmt: '%d', view: Uint16Array },
  i32: { c: 'int', fmt: '%d', view: Int32Array },
  u32: { c: 'unsigned', fmt: '%u', view: Uint32Array },
  f32: { c: 'float', fmt: '%.17g', view: Float32Array },
  f64: { c: 'double', fmt: '%.17g', view: Float64Array },
};

function pointee(type) {
  if (!type.endsWith('*')) return null;
  const elem = ELEMS[type.slice(0, -1)];
  if (!elem) throw new Error(`unsupported pointer argument type ${type}`);
  return elem;
}

function elemValueType(elem) {
  return elem.view === Float32Array || elem.view === Float64Array ? 'f64' : 'i32';
}

function cNumberLiteral(value, type) {
  if (type === 'f64') {
    if (value === 'NaN') return 'NAN';
    if (value === 'Infinity') return 'INFINITY';
    if (value === '-Infinity') return '(-INFINITY)';
    if (value === '-0') return '(-0.0)';
    if (!Number.isFinite(value)) throw new Error(`unsupported f64 literal: ${value}`);
    return Number(value).toPrecision(17);
  }
  return String(Number(value) | 0);
}

function renderNativeRunner(sourcePath, calls) {
  const lines = [
    '#include <stdio.h>',
    '#include <math.h>',
    `#include ${cString(sourcePath)}`,
    'int main(void) {',
  ];
  for (const call of calls) {
    call.cases.forEach((args, index) => {
      const label = `${call.name}:${index}`;
      const buffers = [];
      const renderedArgs = args.map((arg, i) => {
        const elem = pointee(call.args[i]);
        if (!elem) return cNumberLiteral(arg, call.args[i]);
        const name = `buf_${call.name}_${index}_${i}`;
        const init = arg.map((x) => cNumberLiteral(x, elemValueType(elem))).join(', ');
        lines.push(`  static ${elem.c} ${name}[] = { ${init} };`);
        buffers.push({ name, i, elem, n: arg.length });
        return name;
      }).join(', ');
      if (call.ret === 'f64') {
        lines.push(`  printf(${cString(label + '=%.17g\n')}, ${call.name}(${renderedArgs}));`);
      } else if (call.ret === 'i32') {
        lines.push(`  printf(${cString(label + '=%d\n')}, ${call.name}(${renderedArgs}));`);
      } else if (call.ret === 'void') {
        lines.push(`  ${call.name}(${renderedArgs});`);
      } else {
        throw new Error(`unsupported return type ${call.ret}`);
      }
      for (const b of buffers) {
        for (let j = 0; j < b.n; ++j) {
          const cast = b.elem.fmt === '%.17g' ? '(double)' : '';
          lines.push(`  printf(${cString(`${label}#${b.i}[${j}]=${b.elem.fmt}\n`)}, ${cast}${b.name}[${j}]);`);
        }
      }
    });
  }
  lines.push('  return 0;', '}');
  return lines.join('\n');
}

function parseNativeOutput(text) {
  const out = new Map();
  for (const line of text.trim().split(/\n+/)) {
    if (!line) continue;
    const eq = line.indexOf('=');
    if (eq < 0) throw new Error(`bad native output line: ${line}`);
    const text = line.slice(eq + 1);
    let value;
    if (text === 'inf') value = Infinity;
    else if (text === '-inf') value = -Infinity;
    else if (text === 'nan' || text === '-nan') value = NaN;
    else {
      value = Number(text);
      if (Number.isNaN(value)) throw new Error(`bad native number: ${text}`);
    }
    out.set(line.slice(0, eq), value);
  }
  return out;
}

function wasmExport(exports, name) {
  const fn = exports[name] ?? exports[`_${name}`];
  if (typeof fn !== 'function') throw new Error(`missing wasm export ${name}`);
  return fn;
}

function equalEnough(type, actual, expected) {
  if (type === 'i32') return Object.is(actual | 0, expected | 0);
  if (Object.is(actual, expected)) return true;
  if (!Number.isFinite(actual) || !Number.isFinite(expected)) return false;
  if (actual === 0 && expected === 0) return false; // preserve the sign of zero
  const scale = Math.max(1, Math.abs(actual), Math.abs(expected));
  return Math.abs(actual - expected) <= 1e-12 * scale;
}

const manifest = JSON.parse(await fs.readFile(manifestPath, 'utf8'));
const work = await fs.mkdtemp(path.join(os.tmpdir(), 'tcc-wasm-diff-'));
let checked = 0;

try {
  for (const group of manifest) {
    const source = path.join(here, 'differential/cases', group.file);
    const stem = path.basename(group.file, '.c');
    const runner = path.join(work, `${stem}-native-runner.c`);
    const nativeExe = path.join(work, `${stem}-native`);
    const wasmPath = path.join(work, `${stem}.wasm`);

    await fs.writeFile(runner, renderNativeRunner(source, group.calls));
    run([nativeTcc, '-B', root, '-I', path.join(root, 'include'), '-I', root, '-o', nativeExe, runner], { capture: true });
    const native = parseNativeOutput(run([nativeExe], { capture: true }));

    run([wasmTcc, '-B.', '-nostdlib', '-shared', '-o', wasmPath, source], { capture: true });
    const bytes = new Uint8Array(await fs.readFile(wasmPath));
    const summary = inspectWasm(bytes);
    if (!summary.valid) throw new Error(`${wasmPath} failed WebAssembly.validate`);
    const host = createWasmHost();
    const { instance } = await WebAssembly.instantiate(bytes, host.imports);

    for (const call of group.calls) {
      const fn = wasmExport(instance.exports, call.name);
      for (let i = 0; i < call.cases.length; ++i) {
        const label = `${call.name}:${i}`;
        const buffers = [];
        const args = call.cases[i].map((arg, k) => {
          const elem = pointee(call.args[k]);
          if (!elem) return Number(arg);
          /* One spare element on each side detects stores outside the buffer. */
          const size = elem.view.BYTES_PER_ELEMENT;
          const base = host.env.malloc((arg.length + 2) * size);
          const guard = new elem.view(host.memory.buffer, base, arg.length + 2);
          guard.fill(0x5a);
          guard.set(arg.map(Number), 1);
          buffers.push({ k, elem, base, n: arg.length, sentinel: guard[0] });
          return base + size;
        });
        const result = fn(...args);
        let matched = true;
        const mismatches = [];
        if (call.ret !== 'void') {
          const expected = native.get(label);
          if (expected === undefined) throw new Error(`native result missing for ${label}`);
          const actual = Number(result);
          if (!equalEnough(call.ret, actual, expected)) {
            matched = false;
            mismatches.push(`result wasm=${actual}, native=${expected}`);
          }
        }
        for (const b of buffers) {
          const view = new b.elem.view(host.memory.buffer, b.base, b.n + 2);
          if (!Object.is(view[0], b.sentinel) || !Object.is(view[b.n + 1], b.sentinel)) {
            matched = false;
            mismatches.push(`argument ${b.k} store outside buffer`);
          }
          for (let j = 0; j < b.n; ++j) {
            const key = `${label}#${b.k}[${j}]`;
            const expected = native.get(key);
            if (expected === undefined) throw new Error(`native buffer value missing for ${key}`);
            if (!equalEnough(elemValueType(b.elem), view[j + 1], expected)) {
              matched = false;
              mismatches.push(`${key} wasm=${view[j + 1]}, native=${expected}`);
            }
          }
        }
        if (call.xfail) {
          if (matched) {
            throw new Error(`${group.file} ${label} unexpectedly matched; remove xfail: ${call.xfail}`);
          }
          console.log(`xfail wasm32 differential: ${group.file} ${label}: ${call.xfail}`);
          continue;
        }
        if (!matched) {
          throw new Error(`${group.file} ${label} mismatch: ${mismatches.join('; ')}`);
        }
        checked++;
      }
    }
    console.log(`ok wasm32 differential: ${group.file}`);
  }
} finally {
  if (keep) {
    console.log(`kept differential workdir: ${work}`);
  } else {
    await fs.rm(work, { recursive: true, force: true });
  }
}

console.log(`WASM32_DIFFERENTIAL_SMOKE_OK ${checked} checks`);
