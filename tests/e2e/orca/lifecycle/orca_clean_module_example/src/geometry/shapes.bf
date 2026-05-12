// geometry/shapes.bf
//
// Exported geometry functions, demonstrating the export keyword.
// These become available to other modules that import from geometry.shapes.

export squareArea(side: int): int {
	return side * side;
}

export rectArea(width: int, height: int): int {
	return width * height;
}

export trianglePerimeter(a: int, b: int, c: int): int {
	return a + b + c;
}
