inc(x: int): int { return x + 1; }
double_it(x: int): int { return x * 2; }
square(x: int): int { return x * x; }
main(): int {
    result: int = square(double_it(inc(3)));
    print(result);
    return 0;
}
