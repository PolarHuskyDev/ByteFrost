main(): int {
    arr: array<int>;
    for (i: int = 1; i <= 10; i++) {
        arr.push(i);
    }
    lo: int = 0;
    hi: int = arr.length() - 1;
    while (lo < hi) {
        tmp: int = arr[lo];
        arr[lo] = arr[hi];
        arr[hi] = tmp;
        lo++;
        hi--;
    }
    for (v: int in arr) {
        print(v);
    }
    return 0;
}
