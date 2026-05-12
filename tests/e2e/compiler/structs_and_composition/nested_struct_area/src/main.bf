struct Point {
    x: int;
    y: int;
}
struct Rect {
    tl: Point;
    br: Point;
    area(): int {
        w: int = this.br.x - this.tl.x;
        h: int = this.br.y - this.tl.y;
        return w * h;
    }
}
main(): int {
    origin: Point = { x: 0, y: 0 };
    corner: Point = { x: 10, y: 5 };
    r: Rect = { tl: origin, br: corner };
    print(r.area());
    return 0;
}
