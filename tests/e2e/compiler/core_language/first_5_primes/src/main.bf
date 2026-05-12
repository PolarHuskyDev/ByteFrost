main(): int {
    primes: array<int>;
    n: int = 2;
    while (primes.length() < 5) {
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
            primes.push(n);
        }
        n++;
    }
    for (p: int in primes) {
        print(p);
    }
    return 0;
}
