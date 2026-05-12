celsius_to_fahrenheit(c: float): float {
    return c * 9.0 / 5.0 + 32.0;
}
main(): int {
    print(celsius_to_fahrenheit(0.0));
    print(celsius_to_fahrenheit(100.0));
    return 0;
}
