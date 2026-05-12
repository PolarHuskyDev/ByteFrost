main(): int {
    count: int = 0;
    items: array<string>;
    items.push("a");
    items.push("b");
    items.push("c");
    for (s: string in items) {
        count++;
        print("{count}: {s}");
    }
    return 0;
}
