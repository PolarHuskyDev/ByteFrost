main(): int {
    acc: int = 1;
    for (i: int = 1; i <= 10; i++) {
        acc *= i;
    }
    print(acc);
    return 0;
}
