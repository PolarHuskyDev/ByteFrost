main(): int {
    a: int = 5;
    b: int = 7;
    a = a + b;
    b = a - b;
    a = a - b;
    print(a);
    print(b);
    return 0;
}
