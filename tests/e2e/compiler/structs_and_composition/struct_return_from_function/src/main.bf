struct Point {
    x: int;
    y: int;
}
midpoint(p1: Point, p2: Point): Point {
    result: Point = { x: (p1.x + p2.x) / 2, y: (p1.y + p2.y) / 2 };
    return result;
}
main(): int {
    a: Point = { x: 0, y: 0 };
    b: Point = { x: 10, y: 8 };
    mid: Point = midpoint(a, b);
    print(mid.x);
    print(mid.y);
    return 0;
}
