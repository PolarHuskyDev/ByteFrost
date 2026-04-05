#pragma once

#include <string>

#include "tokens.h"

class Lexer {
   public:
	Lexer(const std::string& source) : source(source), position(0), line(1), column(1) {
	}

	Token nextToken();

	private:
	void skipWhitespace();
	void skipComment();
	void advance();

	Token parseNumber();
	Token parseString();
	Token parseIdentifierOrKeyword();
	Token parseOperatorOrDelimiter();

   private:
	std::string source;
	size_t position;
	int line;
	int column;
};