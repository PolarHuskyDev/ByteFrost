main(): int {
    words: array<string>;
    words.push("hello");
    words.push("world");
    words.push("bye");
    for (w: string in words) {
        print(w);
    }
    return 0;
}
