enum Color { RED, GREEN, BLUE }
color_name(c: Color): string {
    match(c) {
        Color.RED => { return "red"; }
        Color.GREEN => { return "green"; }
        Color.BLUE => { return "blue"; }
    }
    return "unknown";
}
main(): int {
    print(color_name(Color.RED));
    print(color_name(Color.GREEN));
    print(color_name(Color.BLUE));
    return 0;
}
