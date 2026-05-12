enum Season { SPRING, SUMMER, AUTUMN, WINTER }
enum Hemisphere { NORTH, SOUTH }
flip_season(s: Season, h: Hemisphere): Season {
    if (h == Hemisphere.SOUTH) {
        if (s == Season.SUMMER) { return Season.WINTER; }
        if (s == Season.WINTER) { return Season.SUMMER; }
        if (s == Season.SPRING) { return Season.AUTUMN; }
        return Season.SPRING;
    }
    return s;
}
main(): int {
    r: Season = flip_season(Season.SUMMER, Hemisphere.SOUTH);
    print(r);
    return 0;
}
