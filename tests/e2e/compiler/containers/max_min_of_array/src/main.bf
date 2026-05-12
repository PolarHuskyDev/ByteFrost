main(): int {
    max_val: int = -999999;
    min_val: int = 999999;
    nums: array<int>;
    nums.push(3);
    nums.push(-7);
    nums.push(15);
    nums.push(0);
    nums.push(-2);
    for (n: int in nums) {
        if (n > max_val) { max_val = n; }
        if (n < min_val) { min_val = n; }
    }
    print(max_val);
    print(min_val);
    return 0;
}
