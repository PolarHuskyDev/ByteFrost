struct Matrix2x2 {
    a: int; b: int;
    c: int; d: int;
    det(): int {
        return this.a * this.d - this.b * this.c;
    }
    trace(): int {
        return this.a + this.d;
    }
}
main(): int {
    m: Matrix2x2 = { a: 3, b: 1, c: 2, d: 4 };
    print(m.det());
    print(m.trace());
    return 0;
}
