// main.bf
//
// Entry point for the module_example orca project.
// Imports geometry functions from geometry.shapes and calls them.
//
// Expected output:
//   25
//   12
//   12

import squareArea, rectArea, trianglePerimeter from geometry.shapes;

main(): int {
	a: int = squareArea(5);        // 5*5 = 25
	b: int = rectArea(4, 3);       // 4*3 = 12
	c: int = trianglePerimeter(3, 4, 5); // 3+4+5 = 12

	print(a);
	print(b);
	print(c);

	return 0;
}
