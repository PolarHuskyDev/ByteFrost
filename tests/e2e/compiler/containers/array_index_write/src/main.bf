main(): int {
    arr: array<int>;
    arr.push(0);
    arr.push(0);
    arr[0] = 42;
    arr[1] = 99;
    print(arr[0]);
    print(arr[1]);
    return 0;
}
