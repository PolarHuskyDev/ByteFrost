// math/utils.bf — exports an overridden abs.

export overridden abs(x: int): int {
	if (x < 0) {
		return -x;
	}
	return x;
}
