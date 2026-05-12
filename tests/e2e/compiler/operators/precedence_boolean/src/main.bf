main(): int {
	if (!false && true) {
		print("A");
	}

	if (true || false && false) {
		print("B");
	}

	if ((true || false) && false) {
		print("C");
	} else {
		print("D");
	}

	return 0;
}
