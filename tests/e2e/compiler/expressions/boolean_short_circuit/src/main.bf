always_true(): int {
    print("evaluated");
    return 1;
}
main(): int {
    if (true && always_true() == 1) {
        print("both true");
    }
    if (false || always_true() == 1) {
        print("or true");
    }
    return 0;
}
