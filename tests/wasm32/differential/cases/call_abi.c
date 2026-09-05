int diff_max_args(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7,
                  int p8, int p9, int p10, int p11, int p12, int p13, int p14, int p15,
                  int p16, int p17, int p18, int p19, int p20, int p21, int p22, int p23,
                  int p24, int p25, int p26, int p27, int p28, int p29, int p30, int p31)
{
    return p0 + 2*p31;
}
int diff_max_call(int a, int b)
{
    return diff_max_args(a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, b);
}

int diff_forward_leaf(int);
int diff_forward(int a, int b)
{
    return diff_forward_leaf(a) + diff_forward_leaf(b);
}
int diff_forward_leaf(int a) { return a + 1; }

int diff_pair(int a, int b) { return a * 10 + b; }
int diff_argument_expr(int a, int b)
{
    return diff_pair(a + b, a - b);
}

int diff_nine(int a, int b, int c, int d, int e, int f, int g, int h, int i)
{
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i;
}
int diff_nine_call(int a, int b)
{
    return diff_nine(a+b, a-b, a*2, b*2, a+3, b+3, a+4, b+4, a+b+5);
}

double diff_nine_float(double a, double b, double c, double d, double e,
                       double f, double g, double h, double i)
{
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i;
}

double diff_nine_float_call(double a, double b)
{
    return diff_nine_float(a+b, a-b, a*2, b*2, a+3, b+3, a+4, b+4, a+b+5);
}

double diff_nine_mixed(int a, double b, float c, int d, double e, float f,
                       int g, double h, float i)
{
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i;
}

double diff_nine_mixed_call(double a, double b)
{
    return diff_nine_mixed((int)(a+b), a-b, (float)(a*2), (int)(b*2), a+3,
                           (float)(b+3), (int)(a+4), b+4, (float)(a+b+5));
}
