enum Direction { NORTH, SOUTH, EAST, WEST }
main(): int {
    dirs: array<Direction>;
    dirs.push(Direction.NORTH);
    dirs.push(Direction.EAST);
    dirs.push(Direction.SOUTH);
    dirs.push(Direction.WEST);
    for (d: Direction in dirs) {
        print(d);
    }
    return 0;
}
