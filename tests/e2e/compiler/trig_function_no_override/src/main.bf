
fac(x: int): int {
	if (x == 0) {
		return 1;
	} else {
		return x * fac(x - 1);
	}
}

abs(x: float): float {
	if (x < 0) {
		return -x;
	} else {
		return x;
	}
}

sin(x: float): float {
	// Taylor series expansion for sin(x)
	result: float = 0.0;
	term: float = x; // First term (x^1 / 1!)
	n: int = 1;

	while (abs(term) > 1e-10) { // Continue until the term is small enough
		result += term;
		n += 2; // Move to the next odd term
		term *= -x * x / (n * (n - 1)); // Calculate the next term
	}

	return result;
}

cos(x: float): float {
	// Taylor series expansion for cos(x)
	result: float = 0.0;
	term: float = 1.0; // First term (x^0 / 0!)
	n: int = 0;

	while (abs(term) > 1e-10) { // Continue until the term is small enough
		result += term;
		n += 2; // Move to the next even term
		term *= -x * x / (n * (n - 1)); // Calculate the next term
	}

	return result;
}

tan(x: float): float {
	cos_x: float = cos(x);
	if (abs(cos_x) < 1e-10) { // Avoid division by zero
		return 0.0; // Return 0 or some large value to indicate undefined
	}
	return sin(x) / cos_x;
}

main(): int {
	PI: float = 3.141592653589793;
	y: float = sin(PI / 2); // Should be close to 1
	z: float = cos(PI); // Should be close to -1
	w: float = tan(PI / 4); // Should be close to 1

	print(y);
	print(z);
	print(w);


	return 0;
}
