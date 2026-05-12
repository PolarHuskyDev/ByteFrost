// Should fail to compile because assigning an integer to an enum variable is not allowed, even if the integer value corresponds to a valid enum member.
enum Color { RED, GREEN, BLUE }

main(): int {
    c: Color = 1;
    return 0;
}
