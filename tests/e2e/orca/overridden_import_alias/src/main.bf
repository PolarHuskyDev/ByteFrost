// main.bf — import an overridden stdlib function using an alias.
//
// 'abs' is a stdlib math name.  Importing it without an alias would be a
// compile error because the bare name is ambiguous with the intrinsic.
// Using 'as myAbs' disambiguates it and routes calls to the user's impl.
//
// Expected output:
//   7
//   3
//   42

import abs as myAbs from math.utils;

main(): int {
	a: int = myAbs(-7);   // 7
	b: int = myAbs(3);    // 3
	c: int = myAbs(-42);  // 42
	print(a);
	print(b);
	print(c);
	return 0;
}
