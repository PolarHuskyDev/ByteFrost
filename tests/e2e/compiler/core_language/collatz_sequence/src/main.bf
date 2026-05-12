collatz(n: int): int {
    steps: int = 0;
    while (n != 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        steps++;
    }
    return steps;
}
main(): int {
    print(collatz(6));
    print(collatz(27));
    return 0;
}
