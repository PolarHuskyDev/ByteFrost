main(): int {
    total: float = 0.0;
    nums: array<float>;
    nums.push(1.5);
    nums.push(2.5);
    nums.push(3.0);
    nums.push(4.0);
    for (n: float in nums) {
        total = total + n;
    }
    avg: float = total / 4.0;
    print(avg);
    return 0;
}
