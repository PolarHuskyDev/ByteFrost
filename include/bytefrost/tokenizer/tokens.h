#pragma once

#include <string>

enum class TokenType {
	// Literals
	STRING_LITERAL,
	INT_LITERAL,
	FLOAT_LITERAL,
	TRUE_LITERAL,
	FALSE_LITERAL,
	NULL_LITERAL,

	// Keywords - control flow
	IF,
	ELSEIF,
	ELSE,
	MATCH,
	FOR,
	WHILE,
	BREAK,
	CONTINUE,
	RETURN,

	// Keywords - type modifiers
	CONST,
	READONLY,

	// Keywords - runtime modifiers
	ASYNC,
	AWAIT,

	// Keywords - types
	VOID,
	BOOL,
	INT,
	FLOAT,
	STRING,

	// Keywords - custom types
	STRUCT,
	THIS,
	ENUM,
	INTERFACE,
	IMPLEMENT_METHOD,
	IMPLEMENTS_INTERFACE,

	// Keywords - data structures, containers, iterators, and utilities
	ARRAY,
	MAP,
	ELEMENT_IN,

	// keywords - module system
	IMPORT,
	EXPORT,
	IMPORT_FROM,
	IMPORT_AS,

	// Operators - arithmetic and assignment
	PLUS,	   // +
	MINUS,	   // -
	MULTIPLY,  // *
	DIVIDE,	   // /
	MODULO,	   // %
	ASSIGN,	   // =

	// Operators - compound assignment
	PLUS_ASSIGN,	  // +=
	MINUS_ASSIGN,	  // -=
	MULTIPLY_ASSIGN,  // *=
	DIVIDE_ASSIGN,	  // /=
	MODULO_ASSIGN,	  // %=

	// Operators - comparison
	EQUAL,			// ==
	NOT_EQUAL,		// !=
	LESS,			// <
	LESS_EQUAL,		// <=
	GREATER,		// >
	GREATER_EQUAL,	// >=

	// Operators - logical
	LOGICAL_AND,  // and
	LOGICAL_OR,	  // or
	LOGICAL_NOT,  // not
	LOGICAL_XOR,  // xor

	// Operators - bitwise
	BITWISE_AND,  // &
	BITWISE_OR,	  // |
	BITWISE_XOR,  // ^
	BITWISE_NOT,  // ~
	SHIFT_LEFT,	  // <<
	SHIFT_RIGHT,  // >>

	// Increment and decrement
	INCREMENT,	// ++
	DECREMENT,	// --

	// Punctuation and delimiters
	LEFT_PAREN,			// (
	RIGHT_PAREN,		// )
	LEFT_BRACE,			// {
	RIGHT_BRACE,		// }
	LEFT_BRACKET,		// [
	RIGHT_BRACKET,		// ]
	COMMA,				// ,
	SEMICOLON,			// ;
	COLON,				// :
	DOT,				// .
	QUESTION_MARK,		// ?
	QUESTION_MARK_DOT,	// ?.
	ARROW,				// =>
	UNDERSCORE,			// _  // match wildcard

	// Special tokens
	IDENTIFIER,
	END_OF_FILE,
	UNKNOWN
};

const std::string& tokenTypeToString(TokenType type);

struct Token {
	TokenType type;
	std::string value;
	int line;
	int column;

	Token() : type(TokenType::UNKNOWN), value(""), line(0), column(0) {
	}

	Token(TokenType type, const std::string& value, int line, int column)
		: type(type), value(value), line(line), column(column) {
	}

	Token(TokenType type, const std::string& value, int line, int column)
		: type(type), value(value), line(line), column(column) {
	}

	const std::string& toString() const {
		if (type == TokenType::END_OF_FILE) {
			static const std::string eofStr = "EOF";
			return eofStr;
		}

		if (type == TokenType::UNKNOWN) {
			static const std::string unknownStr = "UNKNOWN";
			return unknownStr;
		}

		static const std::string tokenStr =
			"Token {" + tokenTypeToString(type) + ", \"" + value + "\", line: " + std::to_string(line)
			+ ", column: " + std::to_string(column) + ", length: " + std::to_string(value.size()) + "}";
		return tokenStr;
	}
};
