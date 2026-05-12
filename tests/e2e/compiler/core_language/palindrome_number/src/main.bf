is_palindrome_num(n: int): bool {
    original: int = n;
    reversed: int = 0;
    while (n > 0) {
        reversed = reversed * 10 + n % 10;
        n = n / 10;
    }
    if (original == reversed) { return true; }
    return false;
}
main(): int {
    print(is_palindrome_num(121));
    print(is_palindrome_num(1221));
    print(is_palindrome_num(123));
    return 0;
}
