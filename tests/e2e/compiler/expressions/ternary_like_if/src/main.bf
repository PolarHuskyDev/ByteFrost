abs_val(x: int): int {
    result: int = 0;
    if (x < 0) { result = -x; } else { result = x; }
    return result;
}
main(): int {
    print(abs_val(-5));
    print(abs_val(3));
    print(abs_val(0));
    return 0;
}
