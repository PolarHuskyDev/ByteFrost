// enums.bf — Standalone enum feature test.
// Tests: enum declaration, variable assignment, print (name not integer),
//        equality comparison, function taking an enum param, function returning an enum type.

enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
}

opposite(d: Direction): Direction {
    result: Direction = Direction.NORTH;
    if (d == Direction.NORTH) { result = Direction.SOUTH; }
    if (d == Direction.SOUTH) { result = Direction.NORTH; }
    if (d == Direction.EAST)  { result = Direction.WEST; }
    if (d == Direction.WEST)  { result = Direction.EAST; }
    return result;
}

main(): int {
    // print(d) outputs the variant name, not an integer
    d: Direction = Direction.NORTH;
    print(d);

    // Function returning an enum value, printed directly
    opp: Direction = opposite(d);
    print(opp);

    // Equality comparison
    if (d == Direction.NORTH) {
        print("Heading north");
    }

    // All four variants via enum literals printed directly
    print(Direction.EAST);
    print(Direction.WEST);

    return 0;
}
