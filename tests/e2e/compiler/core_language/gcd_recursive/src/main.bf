gcd(a: int, b: int): int {
    if (b == 0) { return a; }
    return gcd(b, a % b);
}
main(): int {
    print(gcd(48, 18));
    print(gcd(100, 75));
    return 0;
}
