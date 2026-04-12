#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "tokens.h"

class Lexer {
   public:
	Lexer(const std::string& source) : source(source), position(0), line(1), column(1) {
	}

	std::vector<Token> tokenize();
	Token nextToken();

   public:
	void skipWhitespace();
	void skipLineComment();
	void skipBlockComment();
	void advance();
	char current() const;
	char peek() const;

	bool isSpecialChar(char c) const;
	Token parseOperatorOrDelimiter();
	Token parseNumber();
	Token parseIdentifierOrKeyword();
	Token parseString();
	Token parseCharLiteral();
	Token continueInterpolatedString();

   private:
	std::string source;
	size_t position;
	int line;
	int column;

	// Interpolated string state
	bool inInterpolation = false;
	int interpBraceDepth = 0;

	static const std::unordered_map<std::string, TokenType> keywords;
};