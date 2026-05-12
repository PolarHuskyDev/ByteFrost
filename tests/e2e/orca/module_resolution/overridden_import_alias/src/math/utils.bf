// math/utils.bf
//
// Custom integer abs() that shadows the stdlib math 'abs'.
// Must be marked 'overridden' because 'abs' is a stdlib math function.

export overridden abs(x: int): int {
	if (x < 0) {
		return -x;
	}
	return x;
}
