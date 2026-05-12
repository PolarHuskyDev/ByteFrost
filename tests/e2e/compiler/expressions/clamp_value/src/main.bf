clamp(x: int, lo: int, hi: int): int {
    if (x < lo) { return lo; }
    if (x > hi) { return hi; }
    return x;
}
main(): int {
    print(clamp(-5, 0, 10));
    print(clamp(5, 0, 10));
    print(clamp(15, 0, 10));
    return 0;
}
