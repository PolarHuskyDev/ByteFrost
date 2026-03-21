#pragma once

#include <stdexcept>
#include <string>

enum TokenType {
	// Literals and identifiers
	TOKEN_NUMBER,      // Numeric literals
	TOKEN_STRING,      // String literals
	TOKEN_IDENTIFIER,  // Identifiers
	TOKEN_COLON,       // ':'
	
	// Operators
	TOKEN_PLUS,        // '+'
	TOKEN_MINUS,       // '-'
	TOKEN_MULTIPLY,    // '*'
	TOKEN_DIVIDE,      // '/'
	TOKEN_ASSIGN,      // '='
	
	// Delimiters
	TOKEN_LPAREN,      // '('
	TOKEN_RPAREN,      // ')'
	TOKEN_LBRACE,      // '{'
	TOKEN_RBRACE,      // '}'
	TOKEN_SEMICOLON,   // ';'
	TOKEN_COMMA,       // ','
	
	// Comparison operators
	TOKEN_LT,          // '<'
	TOKEN_GT,          // '>'
	TOKEN_LE,          // '<='
	TOKEN_GE,          // '>='
	TOKEN_EQ,          // '=='
	TOKEN_NEQ,         // '!='

	// Keywords
	TOKEN_IF,          // 'if' keyword
	TOKEN_ELSE,        // 'else' keyword
	TOKEN_RETURN,      // 'return' keyword

	// Types
	TOKEN_TYPE_INT,    // 'int' type
	TOKEN_TYPE_STRING, // 'string' type
	TOKEN_TYPE_VOID,   // 'void' type
	TOKEN_TYPE_BOOL,   // 'bool' type
	TOKEN_TYPE_FLOAT,  // 'float' type

	// End of file
	TOKEN_EOF,         // End of file
};

struct Token {
	TokenType type;
	std::string value;
	int line;
	int column;
};

class Lexer {
   private:
	std::string source;
	size_t position;
	int line;
	int column;

   public:
	Lexer(const std::string& source) : source(source), position(0), line(1), column(1) {
	}

	Token nextToken() {
		skipWhitespace();

		if (position >= source.length()) {
			return {TOKEN_EOF, "", line, column};
		}

		char current = source[position];

		// Numbers
		if (std::isdigit(current)) {
			return parseNumber();
		}

		// String literals
		if (current == '"') {
			return parseString();
		}

		// Identifiers and keywords
		if (std::isalpha(current) || current == '_') {
			return parseIdentifier();
		}

		// Single character tokens
		switch (current) {
			case '+':
				advance();
				return {TOKEN_PLUS, "+", line, column - 1};
			case '-':
				advance();
				return {TOKEN_MINUS, "-", line, column - 1};
			case '*':
				advance();
				return {TOKEN_MULTIPLY, "*", line, column - 1};
			case '/':
				advance();
				return {TOKEN_DIVIDE, "/", line, column - 1};
			case '=': {
				advance();
				if (position < source.length() && source[position] == '=') {
					advance();
					return {TOKEN_EQ, "==", line, column - 2};
				}
				return {TOKEN_ASSIGN, "=", line, column - 1};
			}
			case '<': {
				advance();
				if (position < source.length() && source[position] == '=') {
					advance();
					return {TOKEN_LE, "<=", line, column - 2};
				}
				return {TOKEN_LT, "<", line, column - 1};
			}
			case '>': {
				advance();
				if (position < source.length() && source[position] == '=') {
					advance();
					return {TOKEN_GE, ">=", line, column - 2};
				}
				return {TOKEN_GT, ">", line, column - 1};
			}
			case '!': {
				advance();
				if (position < source.length() && source[position] == '=') {
					advance();
					return {TOKEN_NEQ, "!=", line, column - 2};
				}
				throw std::runtime_error("Unexpected character: '!' at line " + std::to_string(line));
			}
			case '(':
				advance();
				return {TOKEN_LPAREN, "(", line, column - 1};
			case ')':
				advance();
				return {TOKEN_RPAREN, ")", line, column - 1};
			case '{':
				advance();
				return {TOKEN_LBRACE, "{", line, column - 1};
			case '}':
				advance();
				return {TOKEN_RBRACE, "}", line, column - 1};
			case ':':
				advance();
				return {TOKEN_COLON, ":", line, column - 1};
			case ';':
				advance();
				return {TOKEN_SEMICOLON, ";", line, column - 1};
			case ',':
				advance();
				return {TOKEN_COMMA, ",", line, column - 1};
			default:
				throw std::runtime_error("Unexpected character: " + std::string(1, current));
		}
	}

   private:
	void advance() {
		if (position < source.length()) {
			if (source[position] == '\n') {
				line++;
				column = 1;
			} else {
				column++;
			}
			position++;
		}
	}

	void skipWhitespace() {
		while (position < source.length() && std::isspace(source[position])) {
			advance();
		}
	}

	Token parseNumber() {
		std::string number;
		int startColumn = column;

		while (position < source.length() && (std::isdigit(source[position]) || source[position] == '.')) {
			number += source[position];
			advance();
		}

		return {TOKEN_NUMBER, number, line, startColumn};
	}

	Token parseString() {
		int startColumn = column;
		advance(); // consume opening '"'
		std::string value;

		while (position < source.length() && source[position] != '"') {
			if (source[position] == '\n') {
				throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
			}
			if (source[position] == '\\' && position + 1 < source.length()) {
				advance(); // consume '\'
				switch (source[position]) {
					case 'n':  value += '\n'; break;
					case 't':  value += '\t'; break;
					case '"':  value += '"';  break;
					case '\\': value += '\\'; break;
					default:   value += source[position]; break;
				}
			} else {
				value += source[position];
			}
			advance();
		}

		if (position >= source.length()) {
			throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
		}
		advance(); // consume closing '"'
		return {TOKEN_STRING, value, line, startColumn};
	}

	Token parseIdentifier() {
		std::string identifier;
		int startColumn = column;

		while (position < source.length() && (std::isalnum(source[position]) || source[position] == '_')) {
			identifier += source[position];
			advance();
		}

		// Check for keywords and types
		TokenType type = TOKEN_IDENTIFIER;
		if (identifier == "if")
			type = TOKEN_IF;
		else if (identifier == "else")
			type = TOKEN_ELSE;
		else if (identifier == "return")
			type = TOKEN_RETURN;
		else if (identifier == "int")
			type = TOKEN_TYPE_INT;
		else if (identifier == "string")
			type = TOKEN_TYPE_STRING;
		else if (identifier == "void")
			type = TOKEN_TYPE_VOID;
		else if (identifier == "bool")
			type = TOKEN_TYPE_BOOL;
		else if (identifier == "float")
			type = TOKEN_TYPE_FLOAT;

		return {type, identifier, line, startColumn};
	}
};
