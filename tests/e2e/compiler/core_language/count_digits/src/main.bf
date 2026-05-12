count_digits(n: int): int {
    if (n == 0) { return 1; }
    cnt: int = 0;
    if (n < 0) { n = -n; }
    while (n > 0) {
        cnt++;
        n = n / 10;
    }
    return cnt;
}
main(): int {
    print(count_digits(0));
    print(count_digits(9));
    print(count_digits(12345));
    return 0;
}
