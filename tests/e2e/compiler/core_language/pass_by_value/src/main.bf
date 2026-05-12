double_it(x: int): int {
    x = x * 2;
    return x;
}
main(): int {
    n: int = 5;
    result: int = double_it(n);
    print(n);
    print(result);
    return 0;
}
