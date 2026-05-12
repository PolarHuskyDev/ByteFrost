main(): int {
    sum: int = 0;
    i: int = 0;
    while (i < 10) {
        i++;
        if (i % 2 == 0) { continue; }
        sum += i;
    }
    print(sum);
    return 0;
}
