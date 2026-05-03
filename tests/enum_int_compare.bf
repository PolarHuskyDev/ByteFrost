// Should fail to compile because comparing an enum variable with an integer is not allowed, even if the integer value corresponds to a valid enum member.
enum Color { RED, GREEN, BLUE }

main(): int {
    c: Color = Color.RED;
    if (c == 0) {
        print("red");
    }
    return 0;
}
