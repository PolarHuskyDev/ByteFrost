main(): int {
    arr: array<int>;
    arr.push(1);
    arr.push(2);
    arr.push(3);
    arr.push(4);
    arr.push(5);
    n: int = arr.length();
    i: int = n - 1;
    while (i >= 0) {
        print(arr[i]);
        i--;
    }
    return 0;
}
