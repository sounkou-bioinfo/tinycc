/* Comparisons must preserve unordered semantics when tccgen inverts branches. */
int diff_f64_cmp(double a, double b)
{
    return (a == b) + 2 * (a != b) + 4 * (a < b) + 8 * (a <= b)
        + 16 * (a > b) + 32 * (a >= b);
}

int diff_f64_branch(double a, double b)
{
    int r = 0;
    if (a == b) r += 1;
    if (a != b) r += 2;
    if (a < b) r += 4;
    if (a <= b) r += 8;
    if (a > b) r += 16;
    if (a >= b) r += 32;
    return r;
}

int diff_f64_logic(double a, double b)
{
    return (!(a < b)) + 2 * (a && b) + 4 * (a || b)
        + 8 * ((a < b) || (a > b));
}

int diff_f32_cmp(double x, double y)
{
    float a = (float)x, b = (float)y;
    return (a == b) + 2 * (a != b) + 4 * (a < b) + 8 * (a <= b)
        + 16 * (a > b) + 32 * (a >= b);
}

int diff_f32_branch(double x, double y)
{
    float a = (float)x, b = (float)y;
    if (!(a < b)) return a != b ? 3 : 2;
    return 1;
}

double diff_i32_to_f64(int a, int b)
{
    return (double)a + (double)b;
}

double diff_u32_to_f64(unsigned a, unsigned b)
{
    return (double)a + (double)b;
}

double diff_i32_to_f32(int a, int b)
{
    return (float)a + (float)b;
}

double diff_u32_to_f32(unsigned a, unsigned b)
{
    return (float)a + (float)b;
}

int diff_f64_to_i32(double a, double b)
{
    return (int)a + (int)b;
}

double diff_f64_to_u32(double a, double b)
{
    return (double)(unsigned)a + (double)(unsigned)b;
}

int diff_f32_to_i32(double a, double b)
{
    return (int)(float)a + (int)(float)b;
}

double diff_f32_to_u32(double a, double b)
{
    return (double)(unsigned)(float)a + (double)(unsigned)(float)b;
}

double diff_float_roundtrip(double a, double b)
{
    return (double)(float)a + (double)(float)b;
}

double diff_float_neg(double a, double b)
{
    return -a;
}

double diff_cast_live(double a, double b, int c, int d)
{
    return (a + b) + (double)c + (double)d;
}

int diff_cast_live_int(int a, int b, double c, double d)
{
    return (a + b) + (int)c + (int)d;
}

float diff_f32_call_leaf(float a, float b)
{
    return a * b;
}

double diff_f32_call(double a, double b)
{
    return diff_f32_call_leaf((float)a, (float)b)
        + diff_f32_call_leaf((float)b, (float)a);
}
