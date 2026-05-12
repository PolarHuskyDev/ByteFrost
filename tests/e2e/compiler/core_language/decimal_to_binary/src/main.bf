main(): int {
    bits: array<int>;
    n: int = 13;
    while (n > 0) {
        bits.push(n % 2);
        n = n / 2;
    }
    i: int = bits.length() - 1;
    while (i >= 0) {
        print(bits[i]);
        i--;
    }
    return 0;
}
