main(): int {
    i: int = 0;
    while (i < 3) {
        j: int = 0;
        while (j < 10) {
            if (j == 2) { break; }
            j++;
        }
        print(j);
        i++;
    }
    return 0;
}
