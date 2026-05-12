enum Color { RED, GREEN, BLUE }
main(): int {
    c: Color = Color.GREEN;
    x: int = 10;
    match(c) {
        Color.RED => {
            x: int = 100;
            print(x);
        }
        Color.GREEN => {
            x: int = 200;
            print(x);
        }
        Color.BLUE => {
            x: int = 300;
            print(x);
        }
    }
    print(x);
    return 0;
}
