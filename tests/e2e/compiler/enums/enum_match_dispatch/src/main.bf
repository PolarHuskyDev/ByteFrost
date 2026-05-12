enum Op { ADD, SUB, MUL }
apply(op: Op, a: int, b: int): int {
    match(op) {
        Op.ADD => { return a + b; }
        Op.SUB => { return a - b; }
        Op.MUL => { return a * b; }
    }
    return 0;
}
main(): int {
    print(apply(Op.ADD, 3, 4));
    print(apply(Op.SUB, 10, 3));
    print(apply(Op.MUL, 6, 7));
    return 0;
}
