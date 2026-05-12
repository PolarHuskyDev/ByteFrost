struct Fraction {
    num: int;
    den: int;
    constructor(n: int, d: int) {
        this.num = n;
        this.den = d;
    }
    print_frac(): void {
        print("{this.num}/{this.den}");
    }
    value(): float {
        n: float = this.num;
        d: float = this.den;
        return n / d;
    }
}
main(): int {
    f: Fraction = Fraction(1, 4);
    f.print_frac();
    v: float = f.value();
    print(v);
    return 0;
}
