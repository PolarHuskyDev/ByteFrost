enum Season { SPRING, SUMMER, AUTUMN, WINTER }
is_warm(s: Season): bool {
    if (s == Season.SUMMER) { return true; }
    if (s == Season.SPRING) { return true; }
    return false;
}
main(): int {
    print(is_warm(Season.SUMMER));
    print(is_warm(Season.WINTER));
    return 0;
}
