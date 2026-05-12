struct Box {
    width: int;
    height: int;
    constructor(w: int, h: int) {
        this.width = w;
        this.height = h;
    }
    area(): int {
        return this.width * this.height;
    }
    perimeter(): int {
        return 2 * (this.width + this.height);
    }
}
main(): int {
    b: Box = Box(4, 3);
    print(b.area());
    print(b.perimeter());
    return 0;
}
