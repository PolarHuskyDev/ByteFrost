fib(n: int): int {
	if n <= 1 {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}

main(): int {
	f: int = fib(10);
	print(f);

	return 0;
}
