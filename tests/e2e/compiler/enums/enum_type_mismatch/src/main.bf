// Should fail: assigning an enum value to an int variable is a type error.
enum Color { Red, Green, Blue }

main(): int {
    color: Color = Color.Red;
    invalid: int = color;
    return 0;
}
