main(): int {
    sum: int = 0;
    for (i: int = 1; i <= 10; i++) {
        if (i % 2 == 0) {
            sum += i;
        }
    }
    print(sum);
    return 0;
}
