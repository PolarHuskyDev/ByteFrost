main(): int {
    n: int = 5;
    result: int = 1;
    for (i: int = 1; i <= n; i++) {
        result *= i;
    }
    print(result);
    return 0;
}
