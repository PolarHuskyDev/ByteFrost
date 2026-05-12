sign(x: int): int {
    if (x > 0) { return 1; }
    if (x < 0) { return -1; }
    return 0;
}
main(): int {
    print(sign(42));
    print(sign(-17));
    print(sign(0));
    return 0;
}
