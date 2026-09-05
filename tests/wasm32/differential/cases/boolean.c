int diff_bool(int a, int b)
{
    return (a && b) + 2 * (a || b) + 4 * !(a && b)
        + 8 * ((a < b) || (a > b)) + 16 * ((a < b) && (b != 0));
}

int diff_bool_effects(int a, int b)
{
    int n = 0;
    int r = (a && ++n) || (b && ++n);
    return 10 * n + r;
}

int diff_conditional(int a, int b)
{
    return (a < b ? a + 1 : b - 1) + (a == b ? 10 : 20);
}

int diff_break_continue(int a, int b)
{
    int i, n = 0;
    for (i = 0; i < a; ++i) {
        if (i == 2) continue;
        if (i == b) break;
        n += i;
    }
    return n;
}

int diff_goto(int a, int b)
{
    int n = 0;
again:
    if (a-- <= 0) goto done;
    n += b;
    goto again;
done:
    return n;
}
