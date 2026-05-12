power(base: int, exp: int): int {
    if (exp == 0) { return 1; }
    return base * power(base, exp - 1);
}
main(): int {
    print(power(2, 10));
    print(power(3, 4));
    return 0;
}
