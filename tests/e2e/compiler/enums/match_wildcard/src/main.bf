enum Color { RED, GREEN, BLUE, YELLOW }
main(): int {
    c: Color = Color.YELLOW;
    match(c) {
        Color.RED => { print("red"); }
        Color.GREEN => { print("green"); }
        _ => { print("other"); }
    }
    return 0;
}
