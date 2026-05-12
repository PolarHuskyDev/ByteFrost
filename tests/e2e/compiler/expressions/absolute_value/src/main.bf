abs_val(x: int): int {
    if (x < 0) { return -x; }
    return x;
}
main(): int {
    print(abs_val(-7));
    print(abs_val(3));
    print(abs_val(0));
    return 0;
}
