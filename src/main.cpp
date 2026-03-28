#include <iostream>
#include <fstream>

#include "lexer.h"

const char* tokenTypeToString(TokenType type) {
	switch (type) {
		case TOKEN_NUMBER: return "NUMBER";
		case TOKEN_STRING: return "STRING";
		case TOKEN_IDENTIFIER: return "IDENTIFIER";
		case TOKEN_COLON: return "COLON";
		case TOKEN_PLUS: return "PLUS";
		case TOKEN_MINUS: return "MINUS";
		case TOKEN_MULTIPLY: return "MULTIPLY";
		case TOKEN_DIVIDE: return "DIVIDE";
		case TOKEN_ASSIGN: return "ASSIGN";
		case TOKEN_LPAREN: return "LPAREN";
		case TOKEN_RPAREN: return "RPAREN";
		case TOKEN_LBRACE: return "LBRACE";
		case TOKEN_RBRACE: return "RBRACE";
		case TOKEN_SEMICOLON: return "SEMICOLON";
		case TOKEN_COMMA: return "COMMA";
		case TOKEN_LT: return "LT";
		case TOKEN_GT: return "GT";
		case TOKEN_LE: return "LE";
		case TOKEN_GE: return "GE";
		case TOKEN_EQ: return "EQ";
		case TOKEN_NEQ: return "NEQ";
		case TOKEN_IF: return "IF";
		case TOKEN_ELSE: return "ELSE";
		case TOKEN_RETURN: return "RETURN";
		case TOKEN_TYPE_INT: return "TYPE_INT";
		case TOKEN_TYPE_STRING: return "TYPE_STRING";
		case TOKEN_TYPE_VOID: return "TYPE_VOID";
		case TOKEN_TYPE_BOOL: return "TYPE_BOOL";
		case TOKEN_TYPE_FLOAT: return "TYPE_FLOAT";
		case TOKEN_EOF: return "EOF";
		default: return "UNKNOWN";
	}
}

int main(int argc, char* argv[]) {
	const char* filename = "tests/hello_world.bf";
	if (argc > 1) {
		filename = argv[1];
	}
	// std::ifstream file("tests/fib.bf");
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
		printf("Token: %s (Type: %s, Line: %d, Column: %d)\n", token.value.c_str(), tokenTypeToString(token.type), token.line, token.column);
	} while (token.type != TOKEN_EOF);

	return 0;
}
