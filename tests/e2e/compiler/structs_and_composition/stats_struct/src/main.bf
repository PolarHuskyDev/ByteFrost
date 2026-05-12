struct Stats {
    evens: int;
    odds: int;
    constructor() {
        this.evens = 0;
        this.odds = 0;
    }
    add(n: int): void {
        if (n % 2 == 0) {
            this.evens += 1;
        } else {
            this.odds += 1;
        }
    }
}
main(): int {
    s: Stats = Stats();
    s.add(1);
    s.add(2);
    s.add(3);
    s.add(4);
    s.add(5);
    print(s.evens);
    print(s.odds);
    return 0;
}
