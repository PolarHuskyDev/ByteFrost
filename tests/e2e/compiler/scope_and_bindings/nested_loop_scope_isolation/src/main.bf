main(): int {
    total: int = 0;
    for (i: int = 0; i < 3; i++) {
        for (j: int = 0; j < 3; j++) {
            total += 1;
        }
    }
    print(total);
    return 0;
}
