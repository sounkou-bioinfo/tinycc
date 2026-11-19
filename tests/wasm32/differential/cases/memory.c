/* Linear-memory loads and stores through pointer arguments, and the C
   narrowing rules for char/short values held in wasm locals. */

int mem_sum_i32(const int *x, int n)
{
    int i, s = 0;
    for (i = 0; i < n; ++i)
        s += x[i];
    return s;
}

double mem_sum_f64(const double *x, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; ++i)
        s += x[i];
    return s;
}

double mem_sum_f32(const float *x, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; ++i)
        s += x[i];
    return s;
}

void mem_scale_f64(double *x, int n, double a)
{
    int i;
    for (i = 0; i < n; ++i)
        x[i] *= a;
}

void mem_store_f32(float *x, int n, double a)
{
    while (n--)
        *x++ = (float)a;
}

int mem_sum_u8(const unsigned char *p, int n)
{
    int s = 0;
    while (n--)
        s += *p++;
    return s;
}

int mem_sum_i8(const signed char *p, int n)
{
    int s = 0;
    while (n--)
        s += *p++;
    return s;
}

int mem_sum_i16(const short *p, int n)
{
    int i, s = 0;
    for (i = 0; i < n; ++i)
        s += p[i];
    return s;
}

int mem_sum_u16(const unsigned short *p, int n)
{
    int i, s = 0;
    for (i = 0; i < n; ++i)
        s += p[i];
    return s;
}

void mem_store_u8(unsigned char *p, int i, int v)
{
    p[i] = v;
}

void mem_store_i16(short *p, int i, int v)
{
    p[i] = v;
}

void mem_fill_i8(signed char *p, int n, int v)
{
    int i;
    for (i = 0; i < n; ++i)
        p[i] = v + i;
}

int mem_swap_ends(int *p, int n)
{
    int t = p[0];
    p[0] = p[n - 1];
    p[n - 1] = t;
    return p[0] - p[n - 1];
}

int mem_offset_load(const int *p, int k)
{
    const int *q = p + 2;
    return q[k] + q[-1];
}

int mem_char_local(int a)
{
    char c = a;
    return c;
}

int mem_uchar_local(int a)
{
    unsigned char c = a;
    return c;
}

int mem_short_local(int a)
{
    short s = a;
    return s;
}

int mem_ushort_local(int a)
{
    unsigned short s = a;
    return s;
}

int mem_char_leaf(signed char c) { return c * 2; }
int mem_char_param(int a)
{
    return mem_char_leaf(a);
}
