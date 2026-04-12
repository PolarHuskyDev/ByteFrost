main(): int {
	state: string = "HIGH";

	match(state) {
		"HIGH" => {
			print("High state matched");
		}
		"LOW" => {
			print("Low state matched");
		}
		"MIDDLE" | "INTERMEDIATE" => {
			print("Middle or intermediate state matched");
		}
		_ => {
			print("None matched");
		}
	}

	return 0;
}
