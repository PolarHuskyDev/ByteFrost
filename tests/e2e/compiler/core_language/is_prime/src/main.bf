is_prime(n: int): bool {
    if (n < 2) { return false; }
    i: int = 2;
    while (i * i <= n) {
        if (n % i == 0) { return false; }
        i++;
    }
    return true;
}
main(): int {
    print(is_prime(1));
    print(is_prime(2));
    print(is_prime(7));
    print(is_prime(9));
    print(is_prime(13));
    return 0;
}
