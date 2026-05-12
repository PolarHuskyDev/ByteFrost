odd_sum(n: int): int {
    if (n <= 0) { return 0; }
    if (n % 2 == 0) {
        return odd_sum(n - 1);
    }
    return n + odd_sum(n - 2);
}
main(): int {
    print(odd_sum(9));
    return 0;
}
