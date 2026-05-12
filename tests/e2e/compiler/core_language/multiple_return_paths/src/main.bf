classify(n: int): string {
    if (n < 0) { return "negative"; }
    if (n == 0) { return "zero"; }
    return "positive";
}
main(): int {
    print(classify(-5));
    print(classify(0));
    print(classify(7));
    return 0;
}
