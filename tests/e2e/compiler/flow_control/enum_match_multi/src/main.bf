enum State { A, B, C }

main(): int {
    s: State = State.B;
    match(s) {
        State.A => {
            print("a");
        }
        State.B | State.C => {
            print("bc");
        }
        _ => {
            print("x");
        }
    }
    return 0;
}
