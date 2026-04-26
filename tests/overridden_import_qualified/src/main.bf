// main.bf — call an overridden stdlib function via a namespace-qualified call.
//
// 'import math.utils;' is a namespace import: the last path segment 'utils'
// becomes a local module namespace.  Calling 'utils.abs(x)' bypasses the
// stdlib intrinsic because the qualification makes the intent unambiguous.
//
// Expected output:
//   7
//   3
//   42

import math.utils;

main(): int {
	a: int = utils.abs(-7);   // 7
	b: int = utils.abs(3);    // 3
	c: int = utils.abs(-42);  // 42
	print(a);
	print(b);
	print(c);
	return 0;
}
