main(): int {
    total: int = 0;
    for (i: int = 1; i <= 3; i++) {
        for (j: int = 1; j <= 3; j++) {
            total += i * j;
        }
    }
    print(total);
    return 0;
}
