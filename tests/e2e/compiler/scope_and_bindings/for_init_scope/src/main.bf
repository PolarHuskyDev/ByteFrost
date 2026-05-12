main(): int {
    product: int = 1;
    for (i: int = 1; i <= 5; i++) {
        product *= i;
    }
    print(product);
    return 0;
}
