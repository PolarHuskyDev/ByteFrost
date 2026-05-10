struct Rectangle {
	width: float;
	height: float;

	constructor(w: float, h: float) {
		this.width = w;
		this.height = h;
	}

	area(): float {
		return this.width * this.height;
	}
}

main(): int {
	r: Rectangle = Rectangle(4.0, 5.5);
	a: float = r.area();

	print("Area: {a}");
	return 0;
}
