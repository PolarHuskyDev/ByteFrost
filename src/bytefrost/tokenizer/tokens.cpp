#include "bytefrost/tokenizer/tokens.h"

#include <unordered_map>

const std::string& tokenTypeToString(TokenType type) {
	static const std::unordered_map<TokenType, std::string> map = {
		// Literals
		{ TokenType::STRING_LITERAL,      "STRING_LITERAL" },
		{ TokenType::INT_LITERAL,         "INT_LITERAL" },
		{ TokenType::FLOAT_LITERAL,       "FLOAT_LITERAL" },
		{ TokenType::TRUE_LITERAL,        "TRUE_LITERAL" },
		{ TokenType::FALSE_LITERAL,       "FALSE_LITERAL" },
		{ TokenType::NULL_LITERAL,        "NULL_LITERAL" },
		// Keywords - control flow
		{ TokenType::IF,                  "IF" },
		{ TokenType::ELSEIF,              "ELSEIF" },
		{ TokenType::ELSE,                "ELSE" },
		{ TokenType::MATCH,               "MATCH" },
		{ TokenType::FOR,                 "FOR" },
		{ TokenType::WHILE,               "WHILE" },
		{ TokenType::BREAK,               "BREAK" },
		{ TokenType::CONTINUE,            "CONTINUE" },
		{ TokenType::RETURN,              "RETURN" },
		// Keywords - type modifiers
		{ TokenType::CONST,               "CONST" },
		{ TokenType::READONLY,            "READONLY" },
		// Keywords - runtime modifiers
		{ TokenType::ASYNC,               "ASYNC" },
		{ TokenType::AWAIT,               "AWAIT" },
		// Keywords - types
		{ TokenType::VOID,                "VOID" },
		{ TokenType::BOOL,                "BOOL" },
		{ TokenType::INT,                 "INT" },
		{ TokenType::FLOAT,               "FLOAT" },
		{ TokenType::STRING,              "STRING" },
		// Keywords - custom types
		{ TokenType::STRUCT,              "STRUCT" },
		{ TokenType::THIS,                "THIS" },
		{ TokenType::ENUM,                "ENUM" },
		{ TokenType::INTERFACE,           "INTERFACE" },
		{ TokenType::IMPLEMENT_METHOD,    "IMPLEMENT_METHOD" },
		{ TokenType::IMPLEMENTS_INTERFACE,"IMPLEMENTS_INTERFACE" },
		// Keywords - data structures, containers, iterators, and utilities
		{ TokenType::ARRAY,               "ARRAY" },
		{ TokenType::MAP,                 "MAP" },
		{ TokenType::ELEMENT_IN,          "ELEMENT_IN" },
		// Keywords - module system
		{ TokenType::IMPORT,              "IMPORT" },
		{ TokenType::EXPORT,              "EXPORT" },
		{ TokenType::IMPORT_FROM,         "IMPORT_FROM" },
		{ TokenType::IMPORT_AS,           "IMPORT_AS" },
		// Operators - arithmetic and assignment
		{ TokenType::PLUS,                "PLUS" },
		{ TokenType::MINUS,               "MINUS" },
		{ TokenType::MULTIPLY,            "MULTIPLY" },
		{ TokenType::DIVIDE,              "DIVIDE" },
		{ TokenType::MODULO,              "MODULO" },
		{ TokenType::ASSIGN,              "ASSIGN" },
		// Operators - compound assignment
		{ TokenType::PLUS_ASSIGN,         "PLUS_ASSIGN" },
		{ TokenType::MINUS_ASSIGN,        "MINUS_ASSIGN" },
		{ TokenType::MULTIPLY_ASSIGN,     "MULTIPLY_ASSIGN" },
		{ TokenType::DIVIDE_ASSIGN,       "DIVIDE_ASSIGN" },
		{ TokenType::MODULO_ASSIGN,       "MODULO_ASSIGN" },
		// Operators - comparison
		{ TokenType::EQUAL,               "EQUAL" },
		{ TokenType::NOT_EQUAL,           "NOT_EQUAL" },
		{ TokenType::LESS,                "LESS" },
		{ TokenType::LESS_EQUAL,          "LESS_EQUAL" },
		{ TokenType::GREATER,             "GREATER" },
		{ TokenType::GREATER_EQUAL,       "GREATER_EQUAL" },
		// Operators - logical
		{ TokenType::LOGICAL_AND,         "LOGICAL_AND" },
		{ TokenType::LOGICAL_OR,          "LOGICAL_OR" },
		{ TokenType::LOGICAL_NOT,         "LOGICAL_NOT" },
		{ TokenType::LOGICAL_XOR,         "LOGICAL_XOR" },
		// Operators - bitwise
		{ TokenType::BITWISE_AND,         "BITWISE_AND" },
		{ TokenType::BITWISE_OR,          "BITWISE_OR" },
		{ TokenType::BITWISE_XOR,         "BITWISE_XOR" },
		{ TokenType::BITWISE_NOT,         "BITWISE_NOT" },
		{ TokenType::SHIFT_LEFT,          "SHIFT_LEFT" },
		{ TokenType::SHIFT_RIGHT,         "SHIFT_RIGHT" },
		// Increment and decrement
		{ TokenType::INCREMENT,           "INCREMENT" },
		{ TokenType::DECREMENT,           "DECREMENT" },
		// Punctuation and delimiters
		{ TokenType::LEFT_PAREN,          "LEFT_PAREN" },
		{ TokenType::RIGHT_PAREN,         "RIGHT_PAREN" },
		{ TokenType::LEFT_BRACE,          "LEFT_BRACE" },
		{ TokenType::RIGHT_BRACE,         "RIGHT_BRACE" },
		{ TokenType::LEFT_BRACKET,        "LEFT_BRACKET" },
		{ TokenType::RIGHT_BRACKET,       "RIGHT_BRACKET" },
		{ TokenType::COMMA,               "COMMA" },
		{ TokenType::SEMICOLON,           "SEMICOLON" },
		{ TokenType::COLON,               "COLON" },
		{ TokenType::DOT,                 "DOT" },
		{ TokenType::QUESTION_MARK,       "QUESTION_MARK" },
		{ TokenType::QUESTION_MARK_DOT,   "QUESTION_MARK_DOT" },
		{ TokenType::ARROW,               "ARROW" },
		{ TokenType::UNDERSCORE,          "UNDERSCORE" },
		// Special tokens
		{ TokenType::IDENTIFIER,          "IDENTIFIER" },
		{ TokenType::END_OF_FILE,         "END_OF_FILE" },
		{ TokenType::UNKNOWN,             "UNKNOWN" },
	};

	auto it = map.find(type);
	if (it != map.end()) {
		return it->second;
	}
	static const std::string unknown = "UNKNOWN";
	return unknown;
}