max_val(a: int, b: int): int {
    if (a > b) { return a; }
    return b;
}
min_val(a: int, b: int): int {
    if (a < b) { return a; }
    return b;
}
main(): int {
    print(max_val(10, 20));
    print(min_val(10, 20));
    print(max_val(-5, 3));
    return 0;
}
