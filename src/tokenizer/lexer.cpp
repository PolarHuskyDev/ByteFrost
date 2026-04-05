#include "tokenizer/lexer.h"

#include "lexer.h"

Token Lexer::nextToken() {
	


}

void Lexer::skipWhitespace() {
	while (position < source.size() && std::isspace(source[position])) {
		advance();
	}
}

void Lexer::skipComment() {
}

void Lexer::advance() {
	if (position < source.size()) {
		if (source[position] == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
		position++;
	}
}

Token Lexer::parseNumber() {
	std::string numberValue;
	bool isFloat = false;
	int startColumn = column;

	while (position < source.size() && (std::isdigit(source[position]) || source[position] == '.')) {
		if (source[position] == '.') {
			if (isFloat) {
				break;  // Second dot, stop parsing number
			}
			isFloat = true;
		}
		numberValue += source[position];
		advance();
	}

	return {
		isFloat ? FLOAT_LITERAL_TOKEN : INT_LITERAL_TOKEN,
		numberValue,
		line,
		startColumn
	};
}

Token Lexer::parseString() {
	return Token();
}

Token Lexer::parseIdentifierOrKeyword() {
	return Token();
}

Token Lexer::parseOperatorOrDelimiter() {
	return Token();
}
