main(): int {
	list: array<int> = [0, 1, 2, 3, 4, 5];
	squared: array<int>;
	for (i: int = 0 ; i < list.length() ; i++ ) {
		squared.push( list[i] * list[i] );
	}

	print(list);
	print(squared);

	httpCodes: map<int, string>;
	httpCodes[200] = "Ok";
	httpCodes[201] = "Created";
	httpCodes[404] = "Not Found";

	print(httpCodes);

	return 0;
}
