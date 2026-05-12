main(): int {
    nums: array<int>;
    nums.push(10);
    nums.push(20);
    nums.push(30);
    total: int = 0;
    for (n: int in nums) {
        total += n;
    }
    print(total);
    return 0;
}
