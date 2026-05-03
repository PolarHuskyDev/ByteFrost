#pragma once

#include <string>
#include <vector>

enum class TokenType {
	// Special Tokens
	EOF_TOKEN,
	UNKNOWN_TOKEN,

	// Literals
	INT_LITERAL_TOKEN,
	FLOAT_LITERAL_TOKEN,
	STRING_LITERAL_TOKEN,
	CHAR_LITERAL_TOKEN,
	IDENTIFIER_TOKEN,

	// Interpolated string tokens
	INTERP_STRING_START_TOKEN,	// "text{ — opening fragment before first interpolation
	INTERP_STRING_MID_TOKEN,	// }text{ — fragment between interpolations
	INTERP_STRING_END_TOKEN,	// }text" — closing fragment after last interpolation

	// Keywords
	IF_TOKEN,
	ELSE_TOKEN,
	ELSEIF_TOKEN,
	FOR_TOKEN,
	WHILE_TOKEN,
	RETURN_TOKEN,
	BREAK_TOKEN,
	CONTINUE_TOKEN,
	MATCH_TOKEN,
	STRUCT_TOKEN,
	TRUE_TOKEN,
	FALSE_TOKEN,
	NULL_TOKEN,
	THIS_TOKEN,
	IN_TOKEN,
	VOID_TOKEN,
	INT_TOKEN,
	FLOAT_TOKEN,
	BOOL_TOKEN,
	CHAR_TOKEN,
	STRING_TOKEN,
	ARRAY_TOKEN,
	SLICE_TOKEN,
	MAP_TOKEN,
	IMPORT_TOKEN,
	EXPORT_TOKEN,
	FROM_TOKEN,
	AS_TOKEN,
	OVERRIDDEN_TOKEN,
	ENUM_TOKEN,

	// Arithmetic operators
	PLUS_TOKEN,		 // +
	MINUS_TOKEN,	 // -
	MULTIPLY_TOKEN,	 // *
	DIVIDE_TOKEN,	 // /
	MODULO_TOKEN,	 // %

	// Assignment operators
	ASSIGN_TOKEN,			// =
	WALRUS_TOKEN,			// :=
	PLUS_ASSIGN_TOKEN,		// +=
	MINUS_ASSIGN_TOKEN,		// -=
	MULTIPLY_ASSIGN_TOKEN,	// *=
	DIVIDE_ASSIGN_TOKEN,	// /=
	MODULO_ASSIGN_TOKEN,	// %=

	// Comparison operators
	EQUAL_TOKEN,		  // ==
	NOT_EQUAL_TOKEN,	  // !=
	LESS_TOKEN,			  // <
	LESS_EQUAL_TOKEN,	  // <=
	GREATER_TOKEN,		  // >
	GREATER_EQUAL_TOKEN,  // >=

	// Logical operators
	AND_TOKEN,	// &&
	OR_TOKEN,	// ||
	NOT_TOKEN,	// !
	XOR_TOKEN,	// ^^

	// Bitwise operators
	BIT_AND_TOKEN,		// &
	BIT_OR_TOKEN,		// |
	BIT_XOR_TOKEN,		// ^
	BIT_NOT_TOKEN,		// ~
	LEFT_SHIFT_TOKEN,	// <<
	RIGHT_SHIFT_TOKEN,	// >>

	// Increment/Decrement
	INCREMENT_TOKEN,  // ++
	DECREMENT_TOKEN,  // --

	// Delimiters
	LEFT_PAREN_TOKEN,	  // (
	RIGHT_PAREN_TOKEN,	  // )
	LEFT_BRACE_TOKEN,	  // {
	RIGHT_BRACE_TOKEN,	  // }
	LEFT_BRACKET_TOKEN,	  // [
	RIGHT_BRACKET_TOKEN,  // ]
	SEMICOLON_TOKEN,	  // ;
	COLON_TOKEN,		  // :
	COMMA_TOKEN,		  // ,
	DOT_TOKEN,			  // .
	ARROW_TOKEN,		  // =>
	UNDERSCORE_TOKEN,	  // _ (wildcard in match)
	DOTDOT_TOKEN,		  // .. (range)
};

const char* tokenTypeToString(TokenType type);

struct Token {
	TokenType type;
	std::string value;
	int line;
	int column;

	Token() : type(TokenType::UNKNOWN_TOKEN), value(""), line(0), column(0) {
	}

	Token(TokenType type, const std::string& value, int line, int column)
		: type(type), value(value), line(line), column(column) {
	}

	const std::string toString() const {
		if (type == TokenType::EOF_TOKEN) {
			return "EOF";
		}
		if (type == TokenType::UNKNOWN_TOKEN) {
			return "UNKNOWN(\"" + value + "\") at " + std::to_string(line) + ":" + std::to_string(column);
		}
		return "Token {Type: " + std::string(tokenTypeToString(this->type)) + ", Literal: \"" + this->value + "\" at "
			   + std::to_string(this->line) + ":" + std::to_string(this->column) + "}";
	}
};
