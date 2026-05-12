main(): int {
    for (i: int = 0; i < 10; i++) {
        if (i % 3 == 0) { continue; }
        print(i);
    }
    return 0;
}
