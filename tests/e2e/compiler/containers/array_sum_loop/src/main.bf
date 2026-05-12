main(): int {
    arr: array<int>;
    arr.push(1);
    arr.push(2);
    arr.push(3);
    arr.push(4);
    arr.push(5);
    sum: int = 0;
    for (v: int in arr) {
        sum += v;
    }
    print(sum);
    return 0;
}
