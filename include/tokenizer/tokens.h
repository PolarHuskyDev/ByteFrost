#pragma once

#include <string>

enum TokenType {
	// Program Structure
	IDENTIFIER_TOKEN,  // Variable names, function names, etc.

	// Literals
	INT_LITERAL_TOKEN,	   // Integer literals
	FLOAT_LITERAL_TOKEN,   // Floating-point literals
	CHAR_LITERAL_TOKEN,	   // Character literals
	STRING_LITERAL_TOKEN,  // String literals
	TRUE_LITERAL_TOKEN,	   // true
	FALSE_LITERAL_TOKEN,   // false

	// Keywords
	// Control Flow
	IF_TOKEN,		// if
	ELSE_TOKEN,		// else
	ELSE_IF_TOKEN,	// elseif
	FOR_TOKEN,		// for
	WHILE_TOKEN,	// while
	MATCH_TOKEN,	// match
	// Loop Control
	BREAK_TOKEN,	 // break
	CONTINUE_TOKEN,	 // continue
	RETURN_TOKEN,	 // return
	// Runtime Management
	// ASYNC_TOKEN,               // async
	// AWAIT_TOKEN,               // await

	// Operators
	// Arithmetic Operators
	PLUS_TOKEN,		 // +
	MINUS_TOKEN,	 // -
	MULTIPLY_TOKEN,	 // *
	DIVIDE_TOKEN,	 // /
	MODULO_TOKEN,	 // %
	// Assignment Operators
	ASSIGN_TOKEN,			// =
	PLUS_ASSIGN_TOKEN,		// +=
	MINUS_ASSIGN_TOKEN,		// -=
	MULTIPLY_ASSIGN_TOKEN,	// *=
	DIVIDE_ASSIGN_TOKEN,	// /=
	MODULO_ASSIGN_TOKEN,	// %=
	// Comparison Operators
	EQUAL_TOKEN,		  // ==
	NOT_EQUAL_TOKEN,	  // !=
	LESS_TOKEN,			  // <
	LESS_EQUAL_TOKEN,	  // <=
	GREATER_TOKEN,		  // >
	GREATER_EQUAL_TOKEN,  // >=
	// Logical Operators
	AND_TOKEN,	// &&
	OR_TOKEN,	// ||
	NOT_TOKEN,	// !
	XOR_TOKEN,	// ^^
	// Bitwise Operators
	BITWISE_AND_TOKEN,			// &
	BITWISE_OR_TOKEN,			// |
	BITWISE_XOR_TOKEN,			// ^
	BITWISE_NOT_TOKEN,			// ~
	BITWISE_LEFT_SHIFT_TOKEN,	// <<
	BITWISE_RIGHT_SHIFT_TOKEN,	// >>
	// Access Operators
	DOT_TOKEN,	// .

	// Delimiters
	COMMA_TOKEN,	  // ,
	SEMICOLON_TOKEN,  // ;
	COLON_TOKEN,	  // :
	LPAREN_TOKEN,	  // (
	RPAREN_TOKEN,	  // )
	LBRACE_TOKEN,	  // {
	RBRACE_TOKEN,	  // }
	LBRACKET_TOKEN,	  // [
	RBRACKET_TOKEN,	  // ]
	FAT_ARROW_TOKEN,  // =>

	// Comments
	LINE_COMMENT_TOKEN,	 // // Comment

	// Special Tokens
	UNKNOWN_TOKEN,	// Unrecognized token
	EOF_TOKEN		// End of file
};

struct Token {
	TokenType type;
	std::string value;
	int line;
	int column;
};

// Useful to give human-readable names to token types for debugging and error messages
const char *tokenTypeToString(TokenType type);
