main(): int {
    n: int = 8;
    row: int = 0;
    while (row < n) {
        val: int = 1;
        for (col: int = 0; col <= row; col++) {
            print(val);
            val = val * (row - col) / (col + 1);
        }
        row++;
    }
    return 0;
}
