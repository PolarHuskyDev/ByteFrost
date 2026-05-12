main(): int {
    arr: array<int>;
    for (i: int = 1; i <= 5; i++) {
        arr.push(i * i);
    }
    for (v: int in arr) {
        print(v);
    }
    return 0;
}
