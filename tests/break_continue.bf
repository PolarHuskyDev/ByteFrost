main(): int {
	x: int = 0;
	while (x <= 10) {
		if (x == 2) {
			x++;
			continue;
		}

		if (x == 7) {
			break;
		}

		print(x);
		x++;
	}

	return 0;
}
