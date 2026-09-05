int diff_if(int a, int b)
{
    if (a > b)
        return a - b;
    else
        return b - a;
}

int diff_for(int a, int b)
{
    int s = 0, i;
    for (i = 0; i < a; i++)
        s += b;
    return s;
}

int diff_while(int a, int b)
{
    int s = 0;
    while (a > 0) {
        s += b;
        a--;
    }
    return s;
}

int diff_switch(int a, int b)
{
    switch (a) {
    case 0: return b;
    case 1: return b + 10;
    case 2: return b + 20;
    default: return -1;
    }
}

int diff_recursion(int a, int b)
{
    if (a <= 0)
        return b;
    return diff_recursion(a - 1, b + 1);
}
