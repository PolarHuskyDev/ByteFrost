struct Pair {
    first: int;
    second: int;
    constructor(a: int, b: int) {
        this.first = a;
        this.second = b;
    }
    swap(): Pair {
        return Pair(this.second, this.first);
    }
    sum(): int {
        return this.first + this.second;
    }
}
main(): int {
    p: Pair = Pair(3, 7);
    print(p.sum());
    q: Pair = p.swap();
    print(q.first);
    print(q.second);
    return 0;
}
