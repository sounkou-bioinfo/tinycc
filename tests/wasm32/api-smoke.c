/* Native-hosted wasm libtcc lifetime test; also run with ASan/UBSan/LSan. */
#include <stdio.h>
#include <string.h>
#include "libtcc.h"

static void count_error(void *opaque, const char *message)
{
    int *errors = opaque;
    (void)message;
    ++*errors;
}

int main(void)
{
    int i, j;
    for (i = 0; i < 64; ++i) {
        TCCState *s = tcc_new();
        int errors = 0, bad = i % 3 == 0, rc;
        if (!s) return 1;
        tcc_set_error_func(s, &errors, count_error);
        if (tcc_set_wasm32_emscripten(s) != 0) return 2;
        rc = tcc_compile_string(s, bad
            ? "int bad(int a) { long long x=(long long)a*a; return (int)x; }"
            : "int leaf(int); int f(int a){return leaf(a)+leaf(a+1);} int leaf(int a){return a+2;}");
        if ((rc < 0) != bad || (errors != 0) != bad) return 3;
        if (!bad) {
            for (j = 0; j < 2; ++j) {
                unsigned char *bytes = NULL;
                unsigned long len = 0;
                rc = tcc_output_wasm_to_memory(s, &bytes, &len);
                if (rc != 0 || len < 8 || memcmp(bytes, "\0asm", 4)) return 4;
                tcc_free_wasm_buffer(bytes);
            }
        }
        tcc_delete(s);
    }
    puts("WASM32_API_SMOKE_OK 64 states, success/error recovery and repeated output");
    return 0;
}
