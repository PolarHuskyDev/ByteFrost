is_even(n: int): bool {
    if (n == 0) { return true; }
    return is_odd(n - 1);
}
is_odd(n: int): bool {
    if (n == 0) { return false; }
    return is_even(n - 1);
}
main(): int {
    print(is_even(4));
    print(is_even(7));
    print(is_odd(3));
    return 0;
}
