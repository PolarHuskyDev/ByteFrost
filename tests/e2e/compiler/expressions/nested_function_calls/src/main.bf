add(a: int, b: int): int { return a + b; }
mul(a: int, b: int): int { return a * b; }
main(): int {
    result: int = add(mul(2, 3), mul(4, 5));
    print(result);
    return 0;
}
