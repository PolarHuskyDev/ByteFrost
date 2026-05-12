main(): int {
    empty: array<int>;
    count: int = 0;
    for (n: int in empty) {
        count = count + 1;
    }
    print(count);
    return 0;
}
