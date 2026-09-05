import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test, expect } from '@playwright/test';

const here = path.dirname(fileURLToPath(import.meta.url));

const MIME = {
  '.html': 'text/html',
  '.mjs': 'text/javascript',
  '.js': 'text/javascript',
  '.wasm': 'application/wasm',
};

function startServer(rootDir) {
  const server = http.createServer((req, res) => {
    const urlPath = decodeURIComponent(req.url.split('?')[0]);
    const filePath = path.join(rootDir, urlPath === '/' ? 'tcc-in-browser.html' : urlPath);
    if (!filePath.startsWith(rootDir) || !fs.existsSync(filePath)) {
      res.writeHead(404); res.end('not found'); return;
    }
    // Cross-origin isolation headers keep us close to a real wasm deployment.
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    res.setHeader('Content-Type', MIME[path.extname(filePath)] || 'application/octet-stream');
    fs.createReadStream(filePath).pipe(res);
  });
  return new Promise((resolve) => {
    server.listen(0, '127.0.0.1', () => resolve(server));
  });
}

test('TinyCC compiles and runs add(2,40) inside a real browser', async ({ page }) => {
  const artifact = path.join(here, 'tcc-in-wasm.mjs');
  expect(fs.existsSync(artifact), 'run build-emcc-hosted-tcc.sh first').toBe(true);

  const server = await startServer(here);
  const { port } = server.address();
  const consoleLines = [];
  page.on('console', (msg) => consoleLines.push(msg.text()));

  try {
    await page.goto(`http://127.0.0.1:${port}/tcc-in-browser.html`);
    await page.waitForFunction(() => window.__tccDone === true, { timeout: 25_000 });

    const result = await page.evaluate(() => window.__tccResult);
    const err = await page.evaluate(() => window.__tccError);
    const status = await page.textContent('#status');

    expect(err, `tcc error: ${err}\nconsole:\n${consoleLines.join('\n')}`).toBe('');
    expect(result).toBe(42);
    expect(status).toBe('Passed');

    const runs = await page.evaluate(() => {
      const Module = window.__tccModule;
      const cases = [
        ['loop', 'int f(int a,int b){int s=0;while(a--)s+=b;return s;}', 80],
        ['nested calls', 'int g(int x){return x+1;} int f(int a,int b){return g(a)+g(b);}', 44],
        ['casts', 'int f(int a,int b){double d=a;d*=b;return (int)d;}', 80],
        ['float', 'int f(int a,int b){float x=(float)a;return (int)(x+(float)b);}', 42],
        ['unordered', 'int f(int a,int b){double x=0.0/0.0;return !(x<(double)b);}', 1],
        ['float truth', 'int f(int a,int b){double x=a,y=b;return x&&y;}', 1],
        ['short circuit', 'int f(int a,int b){return (a<b)&&(a==2);}', 1],
        ['wide rejection', 'int f(int a,int b){long long x=(long long)a*b;return (int)x;}', -3],
        ['recovery', 'int f(int a,int b){return a+b;}', 42],
      ];
      return cases.map(([name, source, expected]) => ({
        name, expected,
        result: Module.ccall('tcc_wasm_compile_call2', 'number',
          ['string', 'string', 'number', 'number'], [source, 'f', 2, 40]),
        error: Module.ccall('tcc_wasm_last_error', 'string', [], []),
      }));
    });
    for (const run of runs) {
      expect(run.result, `${run.name}: ${run.error}`).toBe(run.expected);
      if (run.name === 'wide rejection')
        expect(run.error).toContain('long long lowering is not implemented');
      else
        expect(run.error, run.name).toBe('');
    }
  } finally {
    server.close();
  }
});
