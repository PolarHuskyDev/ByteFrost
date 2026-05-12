fib_iter(n: int): int {
    if (n <= 1) { return n; }
    a: int = 0;
    b: int = 1;
    for (i: int = 2; i <= n; i++) {
        c: int = a + b;
        a = b;
        b = c;
    }
    return b;
}
main(): int {
    for (i: int = 0; i <= 7; i++) {
        print(fib_iter(i));
    }
    return 0;
}
