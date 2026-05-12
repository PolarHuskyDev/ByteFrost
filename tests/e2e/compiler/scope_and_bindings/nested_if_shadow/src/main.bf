main(): int {
    x: int = 1;
    if (true) {
        x: int = 2;
        if (true) {
            x: int = 3;
            print(x);
        }
        print(x);
    }
    print(x);
    return 0;
}
