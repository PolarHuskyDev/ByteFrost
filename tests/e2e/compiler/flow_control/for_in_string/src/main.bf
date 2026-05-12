main(): int {
    words: array<string> = ["hello", "world", "foo"];
    for (w: string in words) {
        print(w);
    }
    return 0;
}
