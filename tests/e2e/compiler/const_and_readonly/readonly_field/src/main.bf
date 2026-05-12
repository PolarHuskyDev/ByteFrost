struct Point {
    readonly x: int;
    readonly y: int;

    constructor(px: int, py: int) {
        this.x = px;
        this.y = py;
    }

    describe(): void {
        print("Point({this.x}, {this.y})");
    }
}

main(): int {
    p: Point = Point(3, 7);
    p.describe();
    return 0;
}
