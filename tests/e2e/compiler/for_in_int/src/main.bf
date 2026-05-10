main(): int {
    nums: array<int> = [10, 20, 30];
    total: int = 0;
    for (n: int in nums) {
        total = total + n;
    }
    print(total);
    return 0;
}
