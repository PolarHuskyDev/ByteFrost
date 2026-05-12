sum_array(arr: array<int>): int {
    total: int = 0;
    for (n: int in arr) {
        total += n;
    }
    return total;
}
main(): int {
    a: array<int>;
    a.push(10);
    a.push(20);
    a.push(30);
    print(sum_array(a));
    return 0;
}
