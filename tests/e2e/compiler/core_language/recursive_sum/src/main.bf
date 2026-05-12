sum_to(n: int): int {
    if (n <= 0) { return 0; }
    return n + sum_to(n - 1);
}
main(): int {
    print(sum_to(5));
    print(sum_to(10));
    return 0;
}
