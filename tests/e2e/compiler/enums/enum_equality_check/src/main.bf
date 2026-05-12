enum Day { MON, TUE, WED, THU, FRI, SAT, SUN }
main(): int {
    d: Day = Day.WED;
    if (d == Day.WED) { print("midweek"); }
    if (d != Day.SAT) { print("not weekend"); }
    return 0;
}
