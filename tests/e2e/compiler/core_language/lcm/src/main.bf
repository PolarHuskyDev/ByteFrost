lcm(a: int, b: int): int {
    orig_a: int = a;
    orig_b: int = b;
    while (a != b) {
        if (a < b) { a += orig_a; }
        else { b += orig_b; }
    }
    return a;
}
main(): int {
    print(lcm(4, 6));
    print(lcm(3, 5));
    return 0;
}
