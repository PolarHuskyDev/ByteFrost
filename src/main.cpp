#include <cstring>
#include <fstream>
#include <iostream>

#include "tokenizer/lexer.h"
#include "tokenizer/tokens.h"

int main(int argc, char* argv[]) {
	const char* filename = "tests/fib.bf";
	if (argc > 1) {
		filename = argv[1];
	}

	std::ifstream file(filename);

	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << filename << std::endl;
		return 1;
	}

	std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	Lexer lexer(source);
	Token token;

	do {
		token = lexer.nextToken();
		std::printf("Token: %s (Type: %s, Line: %d, Column: %d)\n",
					token.value.c_str(),
					tokenTypeToString(token.type),
					token.line,
					token.column);
	} while (token.type != EOF_TOKEN);

	return 0;
}
