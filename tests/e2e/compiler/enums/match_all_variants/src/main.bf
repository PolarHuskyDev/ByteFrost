enum Direction { NORTH, SOUTH, EAST, WEST }
describe(d: Direction): void {
    match(d) {
        Direction.NORTH => { print("north"); }
        Direction.SOUTH => { print("south"); }
        Direction.EAST => { print("east"); }
        Direction.WEST => { print("west"); }
    }
}
main(): int {
    describe(Direction.NORTH);
    describe(Direction.EAST);
    describe(Direction.WEST);
    return 0;
}
