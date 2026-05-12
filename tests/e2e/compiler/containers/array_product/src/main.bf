main(): int {
    arr: array<int>;
    arr.push(2);
    arr.push(3);
    arr.push(4);
    arr.push(5);
    prod: int = 1;
    for (n: int in arr) {
        prod = prod * n;
    }
    print(prod);
    return 0;
}
