struct Person {
	name: string;
	age: int;

	salute(): void {
		print("Hi I am {this.name}, and I am {this.age} years old");
	}
}

main(): int {
	p: Person = {
		name: "Peter",
		age: 21
	};

	p.salute();
	return 0;
}
