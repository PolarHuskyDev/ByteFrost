struct Score {
    value: int;
}
main(): int {
    s: Score = { value: 10 };
    s.value += 5;
    print(s.value);
    s.value -= 3;
    print(s.value);
    return 0;
}
