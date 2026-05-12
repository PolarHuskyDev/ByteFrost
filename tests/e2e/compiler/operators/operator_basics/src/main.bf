main(): int {
	// Arithmetic
	a: int = 1 + 2;
	b: int = 5 - 3;
	c: int = 2 * 4;
	d: int = 10 / 2;
	e: int = 10 % 3;
	print(a);
	print(b);
	print(c);
	print(d);
	print(e);

	// Comparison
	if (a == 3) { print("a == 3"); }
	if (a != 0) { print("a != 0"); }
	if (a < 5) { print("a < 5"); }
	if (a > 1) { print("a > 1"); }
	if (a <= 3) { print("a <= 3"); }
	if (a >= 3) { print("a >= 3"); }

	// Logical
	if (a > 0 && b > 0) { print("a > 0 && b > 0"); }
	if (a > 0 || b > 0) { print("a > 0 || b > 0"); }
	if (!false) { print("!false"); }
	if (a > 0 ^^ b > 0) {} else { print("a > 0 ^^ b > 0 is false"); }

	// Bitwise
	f := a & 0xFF;
	g := a | 0x0F;
	h := a ^ b;
	i := ~a;
	j := a << 2;
	k := a >> 1;
	print(f);
	print(g);
	print(h);
	print(i);
	print(j);
	print(k);

	// Compound assignment
	a += 5;
	print(a);
	a -= 3;
	print(a);
	a *= 2;
	print(a);
	a /= 4;
	print(a);
	a %= 3;
	print(a);

	// Increment / decrement
	a++;
	print(a);
	a--;
	print(a);

	return 0;
}
