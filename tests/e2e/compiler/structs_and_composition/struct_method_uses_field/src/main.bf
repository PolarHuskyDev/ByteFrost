struct Circle {
    radius: float;
    constructor(r: float) {
        this.radius = r;
    }
    diameter(): float {
        return this.radius * 2.0;
    }
}
main(): int {
    c: Circle = Circle(5.0);
    d: float = c.diameter();
    print(d);
    return 0;
}
