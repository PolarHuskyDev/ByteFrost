main(): int {
    items: array<int>;
    items.push(10);
    items.push(20);
    items.push(30);
    n: int = items.length() - 1;
    print(items.length());
    print(items[n]);
    return 0;
}
