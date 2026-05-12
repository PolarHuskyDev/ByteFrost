main(): int {
    nums: array<int>;
    nums.push(1);
    nums.push(4);
    nums.push(9);
    nums.push(2);
    nums.push(7);
    count: int = 0;
    for (n: int in nums) {
        if (n > 3) {
            count++;
        }
    }
    print(count);
    return 0;
}
