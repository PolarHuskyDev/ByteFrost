main(): int {
    x: int = 100;
    if (true) {
        x: int = 0;
        x += 50;
        print(x);
    }
    print(x);
    return 0;
}
