/* Emscripten MAIN_MODULE driver: hosts libtcc *inside* wasm.
 *
 * Given C source at runtime it drives the full loop entirely in the wasm
 * runtime: tcc compiles the source with its own wasm32 backend, emits an
 * Emscripten side module, we write it to MEMFS, dlopen it, resolve a symbol,
 * and call it.  This is the "compiler in the browser" path -- no native tcc
 * and no emcc involved at runtime.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <emscripten.h>
#include "libtcc.h"

static char last_error[512];

static void capture_error(void *opaque, const char *msg)
{
    (void)opaque;
    snprintf(last_error, sizeof(last_error), "%s", msg);
    fprintf(stderr, "[tcc] %s\n", msg);
}

EMSCRIPTEN_KEEPALIVE
const char *tcc_wasm_last_error(void)
{
    return last_error;
}

/* Compile `csrc`, emit a side module, dlopen it, then call int sym(int,int).
 * Returns the call result, or a negative error code (last error via
 * tcc_wasm_last_error()). */
EMSCRIPTEN_KEEPALIVE
int tcc_wasm_compile_call2(const char *csrc, const char *sym, int a, int b)
{
    TCCState *s;
    static unsigned int seq;               /* unique path per compile: Emscripten
                                              dlopen caches by path, so reusing a
                                              path returns the stale module. */
    char out_path[64];
    void *handle, *fn_ptr;
    int (*fn)(int, int);
    int result;

    last_error[0] = '\0';
    snprintf(out_path, sizeof(out_path), "/tmp/tcc_mod_%u.wasm", seq++);

    s = tcc_new();
    if (!s) {
        snprintf(last_error, sizeof(last_error), "tcc_new failed");
        return -1;
    }
    tcc_set_error_func(s, NULL, capture_error);

    if (tcc_set_wasm32_emscripten(s) != 0) {
        tcc_delete(s);
        return -2;
    }

    if (tcc_compile_string(s, csrc) != 0) {
        tcc_delete(s);
        return -3;
    }

    if (tcc_output_file(s, out_path) != 0) {
        tcc_delete(s);
        remove(out_path);
        return -4;
    }
    tcc_delete(s);

    handle = dlopen(out_path, RTLD_NOW | RTLD_LOCAL);
    remove(out_path);
    if (!handle) {
        snprintf(last_error, sizeof(last_error), "dlopen: %s", dlerror());
        return -5;
    }

    fn_ptr = dlsym(handle, sym);
    if (!fn_ptr) {
        snprintf(last_error, sizeof(last_error), "dlsym(%s): %s", sym, dlerror());
        dlclose(handle);
        return -6;
    }

    fn = (int (*)(int, int))fn_ptr;
    result = fn(a, b);
    dlclose(handle);
    return result;
}

/* Node/standalone entry point: run the canonical add(2,40)==42 loop. */
int main(void)
{
    const char *src = "int add(int a, int b) { return a + b; }";
    int r = tcc_wasm_compile_call2(src, "add", 2, 40);
    if (r != 42) {
        fprintf(stderr, "FAIL: tcc-in-wasm add(2,40) = %d (err: %s)\n",
                r, tcc_wasm_last_error());
        return 1;
    }
    printf("TCC_IN_WASM_OK add(2,40)=%d\n", r);
    return 0;
}
