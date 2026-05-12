sum_of_primes(limit: int): int {
    total: int = 0;
    n: int = 2;
    while (n <= limit) {
        prime: bool = true;
        i: int = 2;
        while (i * i <= n) {
            if (n % i == 0) {
                prime = false;
                break;
            }
            i++;
        }
        if (prime) {
            total += n;
        }
        n++;
    }
    return total;
}
main(): int {
    print(sum_of_primes(10));
    return 0;
}
