// math/utils.bf — same module reused for the qualified-call test.

export overridden abs(x: int): int {
	if (x < 0) {
		return -x;
	}
	return x;
}
