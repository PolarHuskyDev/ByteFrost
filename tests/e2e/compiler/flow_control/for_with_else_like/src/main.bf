main(): int {
    found: int = -1;
    for (i: int = 0; i < 10; i++) {
        if (i * i > 30) {
            found = i;
            break;
        }
    }
    print(found);
    return 0;
}
