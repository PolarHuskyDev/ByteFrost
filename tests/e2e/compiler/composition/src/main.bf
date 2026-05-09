struct Point {
	x: int;
	y: int;
}

struct Circle {
	center: Point;
	radius: float;
}

main(): int {
	c: Circle = {
		center: {
			x: 10,
			y: 20
		},
		radius: 5.5
	};

	print("Circle center: ({c.center.x}, {c.center.y}), radius: {c.radius}");

	return 0;
}
