// main.bf — importing an overridden stdlib name WITHOUT an alias.
//
// This should be rejected at compile time: importing 'abs' directly would
// shadow the stdlib name in an ambiguous way — calls to abs() would silently
// use the stdlib intrinsic instead of the imported function.
//
// Expected: compiler error mentioning the name conflict and the required alias.

import abs from math.utils;

main(): int {
	a: int = abs(-7);
	print(a);
	return 0;
}
