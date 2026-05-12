struct Counter {
    value: int;
    constructor(start: int) {
        this.value = start;
    }
    increment(): void {
        this.value += 1;
    }
    decrement(): void {
        this.value -= 1;
    }
    get_value(): int {
        return this.value;
    }
}
main(): int {
    c: Counter = Counter(10);
    c.increment();
    c.increment();
    c.increment();
    c.decrement();
    print(c.get_value());
    return 0;
}
