struct Counter {
    count: int;
    constructor(start: int) {
        this.count = start;
    }
    increment(): void {
        this.count += 1;
    }
    get(): int {
        return this.count;
    }
}
main(): int {
    a: Counter = Counter(0);
    b: Counter = Counter(10);
    a.increment();
    a.increment();
    b.increment();
    print(a.get());
    print(b.get());
    return 0;
}
