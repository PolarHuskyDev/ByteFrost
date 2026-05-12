main(): int {
    nums: array<int> = [1, 2, 3, 4, 5];
    found: int = -1;
    for (n: int in nums) {
        if (n == 3) {
            found = n;
            break;
        }
    }
    print(found);
    return 0;
}
