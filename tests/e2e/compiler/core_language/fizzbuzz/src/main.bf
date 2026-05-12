fizzbuzz(n: int): void {
    i: int = 1;
    while (i <= n) {
        if (i % 15 == 0) {
            print("FizzBuzz");
        } elseif (i % 3 == 0) {
            print("Fizz");
        } elseif (i % 5 == 0) {
            print("Buzz");
        } else {
            print(i);
        }
        i++;
    }
}
main(): int {
    fizzbuzz(15);
    return 0;
}
