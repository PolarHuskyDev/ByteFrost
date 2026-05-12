all_positive(a: int, b: int, c: int): int {
    if (a > 0 && b > 0 && c > 0) { return 1; }
    return 0;
}
any_negative(a: int, b: int, c: int): int {
    if (a < 0 || b < 0 || c < 0) { return 1; }
    return 0;
}
main(): int {
    print(all_positive(1, 2, 3));
    print(all_positive(1, -1, 3));
    print(any_negative(1, 2, 3));
    print(any_negative(1, -1, 3));
    return 0;
}
