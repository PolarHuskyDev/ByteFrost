struct Counter {
    readonly count: int;
    constructor(n: int) {
        this.count = n;
    }
}

main(): int {
    c: Counter = Counter(0);
    c.count = 5;
    return 0;
}
