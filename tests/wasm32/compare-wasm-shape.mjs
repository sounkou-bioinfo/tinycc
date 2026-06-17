import fs from 'node:fs/promises';

const [generatedPath, goldenPath] = process.argv.slice(2);
if (!generatedPath || !goldenPath) {
  throw new Error('usage: node compare-wasm-shape.mjs generated.wasm golden.wasm');
}

const sectionNames = new Map([
  [0, 'custom'], [1, 'type'], [2, 'import'], [3, 'function'], [4, 'table'],
  [5, 'memory'], [6, 'global'], [7, 'export'], [8, 'start'], [9, 'element'],
  [10, 'code'], [11, 'data'], [12, 'data_count'],
]);

function readULEB(bytes, pos) {
  let result = 0, shift = 0, p = pos;
  while (true) {
    const b = bytes[p++];
    result |= (b & 0x7f) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  return [result, p];
}

function shape(bytes) {
  if (bytes[0] !== 0 || bytes[1] !== 0x61 || bytes[2] !== 0x73 || bytes[3] !== 0x6d) {
    throw new Error('bad wasm magic');
  }
  const out = [];
  let p = 8;
  while (p < bytes.length) {
    const id = bytes[p++];
    let size; [size, p] = readULEB(bytes, p);
    out.push(sectionNames.get(id) ?? `section-${id}`);
    p += size;
  }
  return out;
}

const generated = new Uint8Array(await fs.readFile(generatedPath));
const golden = new Uint8Array(await fs.readFile(goldenPath));
if (!WebAssembly.validate(generated)) throw new Error(`${generatedPath} failed WebAssembly.validate`);
if (!WebAssembly.validate(golden)) throw new Error(`${goldenPath} failed WebAssembly.validate`);

const generatedShape = shape(generated);
const required = ['type', 'import', 'function', 'export', 'code'];
for (const name of required) {
  if (!generatedShape.includes(name)) {
    throw new Error(`${generatedPath} missing required ${name} section; shape=${generatedShape.join(',')}`);
  }
}
console.log(`generated=${generatedShape.join(',')}`);
console.log(`golden=${shape(golden).join(',')}`);
