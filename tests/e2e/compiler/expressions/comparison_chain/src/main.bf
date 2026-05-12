in_range(x: int, lo: int, hi: int): int {
    if (x >= lo && x <= hi) { return 1; }
    return 0;
}
main(): int {
    print(in_range(5, 1, 10));
    print(in_range(15, 1, 10));
    print(in_range(1, 1, 10));
    return 0;
}
