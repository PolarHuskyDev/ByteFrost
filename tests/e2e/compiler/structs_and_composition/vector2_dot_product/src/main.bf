struct Vector2 {
    x: float;
    y: float;
    dot(other: Vector2): float {
        return this.x * other.x + this.y * other.y;
    }
}
main(): int {
    a: Vector2 = { x: 1.0, y: 2.0 };
    b: Vector2 = { x: 3.0, y: 4.0 };
    print(a.dot(b));
    return 0;
}
