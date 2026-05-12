main(): int {
    x: int = 5;
    arr: array<int>;
    arr.push(0);
    for (x: int in arr) {
        print(x);
    }
    print(x);
    return 0;
}
