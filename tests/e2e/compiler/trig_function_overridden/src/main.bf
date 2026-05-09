// trig_function_overridden.bf
//
// Demonstrates that stdlib math functions (abs, sin, cos, tan) can be
// shadowed with user-provided implementations by marking them 'overridden'.
//
// All four functions below use Taylor-series approximations instead of the
// LLVM intrinsics / C math library calls that the stdlib would normally emit.
//
// Expected output:
//   1
//   -1
//   1

overridden abs(x: float): float {
	if (x < 0) {
		return -x;
	} else {
		return x;
	}
}

overridden sin(x: float): float {
	// Taylor series expansion for sin(x)
	result: float = 0.0;
	term: float = x; // First term (x^1 / 1!)
	n: int = 1;

	while (abs(term) > 1e-10) {
		result += term;
		n += 2;
		term *= -x * x / (n * (n - 1));
	}

	return result;
}

overridden cos(x: float): float {
	// Taylor series expansion for cos(x)
	result: float = 0.0;
	term: float = 1.0; // First term (x^0 / 0!)
	n: int = 0;

	while (abs(term) > 1e-10) {
		result += term;
		n += 2;
		term *= -x * x / (n * (n - 1));
	}

	return result;
}

overridden tan(x: float): float {
	cos_x: float = cos(x);
	if (abs(cos_x) < 1e-10) {
		return 0.0;
	}
	return sin(x) / cos_x;
}

main(): int {
	PI: float = 3.141592653589793;
	y: float = sin(PI / 2);  // ≈ 1
	z: float = cos(PI);      // ≈ -1
	w: float = tan(PI / 4);  // ≈ 1

	print(y);
	print(z);
	print(w);

	return 0;
}
