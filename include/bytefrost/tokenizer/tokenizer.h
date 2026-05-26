#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "logger/logger.h"
#include "tokens.h"

class Tokenizer {
   public:
	Tokenizer(const std::string& source) : source(source), position(0), line(1), column(1) {
	}

	std::vector<Token> tokenize();
	Token nextToken();

   private:
	void skipWhitespace();
	void skipLineComment();
	void skipBlockComment();

	void advance();
	char current() const;
	char peek() const;

	bool isSpecialCharacter(char c) const;
	Token parseStringLiteral();
	Token parseNumberLiteral();
	Token parseIdentifierOrKeyword();
	Token parseOperatorOrDelimiter();

   private:
	std::string source;
	size_t position;
	int line;
	int column;

	static const std::unordered_map<std::string, TokenType> keywords;
	static const std::unordered_map<std::string, char> escapeSequences;
	static const std::unordered_map<std::string, TokenType> singleCharTokens;
};
