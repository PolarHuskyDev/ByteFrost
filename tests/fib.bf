fib(n: int): int {
	if (n <= 1) {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}

main(): int {
	// Full for loop
	for (i: int = 0 ; i < 10 ; i++) {
		print(fib(i));
	}

	return 0;
}
