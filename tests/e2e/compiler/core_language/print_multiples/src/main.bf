print_multiples(n: int, count: int): void {
    for (i: int = 1; i <= count; i++) {
        print(n * i);
    }
}
main(): int {
    print_multiples(7, 5);
    return 0;
}
