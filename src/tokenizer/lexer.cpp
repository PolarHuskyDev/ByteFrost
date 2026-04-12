#include "tokenizer/lexer.h"

#include <cctype>

// ==========================
// Keyword map
// ==========================

const std::unordered_map<std::string, TokenType> Lexer::keywords = {
	{"if", TokenType::IF_TOKEN},
	{"else", TokenType::ELSE_TOKEN},
	{"elseif", TokenType::ELSEIF_TOKEN},
	{"for", TokenType::FOR_TOKEN},
	{"while", TokenType::WHILE_TOKEN},
	{"return", TokenType::RETURN_TOKEN},
	{"break", TokenType::BREAK_TOKEN},
	{"continue", TokenType::CONTINUE_TOKEN},
	{"match", TokenType::MATCH_TOKEN},
	{"struct", TokenType::STRUCT_TOKEN},
	{"true", TokenType::TRUE_TOKEN},
	{"false", TokenType::FALSE_TOKEN},
	{"this", TokenType::THIS_TOKEN},
	{"in", TokenType::IN_TOKEN},
	{"void", TokenType::VOID_TOKEN},
	{"int", TokenType::INT_TOKEN},
	{"float", TokenType::FLOAT_TOKEN},
	{"bool", TokenType::BOOL_TOKEN},
	{"char", TokenType::CHAR_TOKEN},
	{"string", TokenType::STRING_TOKEN},
	{"array", TokenType::ARRAY_TOKEN},
	{"slice", TokenType::SLICE_TOKEN},
	{"map", TokenType::MAP_TOKEN},
};

// ==========================
// Helpers
// ==========================

char Lexer::current() const {
	if (position < source.size())
		return source[position];
	return '\0';
}

char Lexer::peek() const {
	if (position + 1 < source.size())
		return source[position + 1];
	return '\0';
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

void Lexer::skipWhitespace() {
	while (position < source.size() && std::isspace(static_cast<unsigned char>(current()))) {
		advance();
	}
}

void Lexer::skipLineComment() {
	while (position < source.size() && current() != '\n') {
		advance();
	}
}

void Lexer::skipBlockComment() {
	// Skip past the opening /*
	advance(); // consume '/'
	advance(); // consume '*'
	while (position < source.size()) {
		if (current() == '*' && peek() == '/') {
			advance(); // consume '*'
			advance(); // consume '/'
			return;
		}
		advance();
	}
}

// ==========================
// Main Tokenization Logic
// ==========================

std::vector<Token> Lexer::tokenize() {
	std::vector<Token> tokens;
	Token token;
	do {
		token = nextToken();
		tokens.push_back(token);
	} while (token.type != TokenType::EOF_TOKEN);
	return tokens;
}

Token Lexer::nextToken() {
	skipWhitespace();

	char c = current();

	if (c == '\0') {
		return Token(TokenType::EOF_TOKEN, "", line, column);
	}

	// Inside interpolated string: track brace depth and detect end of interpolation
	if (inInterpolation) {
		if (c == '}') {
			if (interpBraceDepth > 0) {
				interpBraceDepth--;
				// Fall through to normal tokenization (returns RIGHT_BRACE_TOKEN)
			} else {
				return continueInterpolatedString();
			}
		} else if (c == '{') {
			interpBraceDepth++;
			// Fall through to normal tokenization (returns LEFT_BRACE_TOKEN)
		}
	}

	// Line comment
	if (c == '/' && peek() == '/') {
		skipLineComment();
		return nextToken();
	}

	// Block comment
	if (c == '/' && peek() == '*') {
		skipBlockComment();
		return nextToken();
	}

	// String literal
	if (c == '"') {
		return parseString();
	}

	// Char literal
	if (c == '\'') {
		return parseCharLiteral();
	}

	// Number literal (starts with digit or . followed by digit)
	if (std::isdigit(static_cast<unsigned char>(c))) {
		return parseNumber();
	}

	// Identifier or keyword (starts with letter or _)
	if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
		return parseIdentifierOrKeyword();
	}

	// Operators and delimiters
	if (isSpecialChar(c)) {
		return parseOperatorOrDelimiter();
	}

	// Unknown character
	int startColumn = column;
	advance();
	return Token(TokenType::UNKNOWN_TOKEN, std::string(1, c), line, startColumn);
}

// ==========================
// Parsing logic
// ==========================

bool Lexer::isSpecialChar(char c) const {
	return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
		   c == '=' || c == '!' || c == '<' || c == '>' ||
		   c == '(' || c == ')' || c == '{' || c == '}' ||
		   c == '[' || c == ']' || c == ';' || c == ':' ||
		   c == ',' || c == '.' || c == '&' || c == '|' ||
		   c == '^' || c == '~';
}

Token Lexer::parseOperatorOrDelimiter() {
	char c = current();
	int startColumn = column;

	// Two-character operators (check longest match first)
	switch (c) {
		case '=':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::EQUAL_TOKEN, "==", line, startColumn); }
			if (peek() == '>') { advance(); advance(); return Token(TokenType::ARROW_TOKEN, "=>", line, startColumn); }
			break;
		case '!':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::NOT_EQUAL_TOKEN, "!=", line, startColumn); }
			break;
		case '<':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::LESS_EQUAL_TOKEN, "<=", line, startColumn); }
			if (peek() == '<') { advance(); advance(); return Token(TokenType::LEFT_SHIFT_TOKEN, "<<", line, startColumn); }
			break;
		case '>':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::GREATER_EQUAL_TOKEN, ">=", line, startColumn); }
			if (peek() == '>') { advance(); advance(); return Token(TokenType::RIGHT_SHIFT_TOKEN, ">>", line, startColumn); }
			break;
		case '+':
			if (peek() == '+') { advance(); advance(); return Token(TokenType::INCREMENT_TOKEN, "++", line, startColumn); }
			if (peek() == '=') { advance(); advance(); return Token(TokenType::PLUS_ASSIGN_TOKEN, "+=", line, startColumn); }
			break;
		case '-':
			if (peek() == '-') { advance(); advance(); return Token(TokenType::DECREMENT_TOKEN, "--", line, startColumn); }
			if (peek() == '=') { advance(); advance(); return Token(TokenType::MINUS_ASSIGN_TOKEN, "-=", line, startColumn); }
			break;
		case '*':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::MULTIPLY_ASSIGN_TOKEN, "*=", line, startColumn); }
			break;
		case '/':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::DIVIDE_ASSIGN_TOKEN, "/=", line, startColumn); }
			break;
		case '%':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::MODULO_ASSIGN_TOKEN, "%=", line, startColumn); }
			break;
		case '&':
			if (peek() == '&') { advance(); advance(); return Token(TokenType::AND_TOKEN, "&&", line, startColumn); }
			break;
		case '|':
			if (peek() == '|') { advance(); advance(); return Token(TokenType::OR_TOKEN, "||", line, startColumn); }
			break;
		case '^':
			if (peek() == '^') { advance(); advance(); return Token(TokenType::XOR_TOKEN, "^^", line, startColumn); }
			break;
		case ':':
			if (peek() == '=') { advance(); advance(); return Token(TokenType::WALRUS_TOKEN, ":=", line, startColumn); }
			break;
		case '.':
			if (peek() == '.') { advance(); advance(); return Token(TokenType::DOTDOT_TOKEN, "..", line, startColumn); }
			break;
		default:
			break;
	}

	// Single-character operators and delimiters
	advance();
	switch (c) {
		case '+': return Token(TokenType::PLUS_TOKEN, "+", line, startColumn);
		case '-': return Token(TokenType::MINUS_TOKEN, "-", line, startColumn);
		case '*': return Token(TokenType::MULTIPLY_TOKEN, "*", line, startColumn);
		case '/': return Token(TokenType::DIVIDE_TOKEN, "/", line, startColumn);
		case '%': return Token(TokenType::MODULO_TOKEN, "%", line, startColumn);
		case '=': return Token(TokenType::ASSIGN_TOKEN, "=", line, startColumn);
		case '!': return Token(TokenType::NOT_TOKEN, "!", line, startColumn);
		case '<': return Token(TokenType::LESS_TOKEN, "<", line, startColumn);
		case '>': return Token(TokenType::GREATER_TOKEN, ">", line, startColumn);
		case '&': return Token(TokenType::BIT_AND_TOKEN, "&", line, startColumn);
		case '|': return Token(TokenType::BIT_OR_TOKEN, "|", line, startColumn);
		case '^': return Token(TokenType::BIT_XOR_TOKEN, "^", line, startColumn);
		case '~': return Token(TokenType::BIT_NOT_TOKEN, "~", line, startColumn);
		case '(': return Token(TokenType::LEFT_PAREN_TOKEN, "(", line, startColumn);
		case ')': return Token(TokenType::RIGHT_PAREN_TOKEN, ")", line, startColumn);
		case '{': return Token(TokenType::LEFT_BRACE_TOKEN, "{", line, startColumn);
		case '}': return Token(TokenType::RIGHT_BRACE_TOKEN, "}", line, startColumn);
		case '[': return Token(TokenType::LEFT_BRACKET_TOKEN, "[", line, startColumn);
		case ']': return Token(TokenType::RIGHT_BRACKET_TOKEN, "]", line, startColumn);
		case ';': return Token(TokenType::SEMICOLON_TOKEN, ";", line, startColumn);
		case ':': return Token(TokenType::COLON_TOKEN, ":", line, startColumn);
		case ',': return Token(TokenType::COMMA_TOKEN, ",", line, startColumn);
		case '.': return Token(TokenType::DOT_TOKEN, ".", line, startColumn);
		default:  return Token(TokenType::UNKNOWN_TOKEN, std::string(1, c), line, startColumn);
	}
}

Token Lexer::parseNumber() {
	int startColumn = column;
	std::string numberStr;
	bool isFloat = false;

	// Hexadecimal
	if (current() == '0' && peek() == 'x') {
		numberStr += "0x";
		advance();
		advance();
		while (std::isxdigit(static_cast<unsigned char>(current()))) {
			numberStr += current();
			advance();
		}
		return Token(TokenType::INT_LITERAL_TOKEN, numberStr, line, startColumn);
	}

	// Binary
	if (current() == '0' && peek() == 'b') {
		numberStr += "0b";
		advance();
		advance();
		while (current() == '0' || current() == '1') {
			numberStr += current();
			advance();
		}
		return Token(TokenType::INT_LITERAL_TOKEN, numberStr, line, startColumn);
	}

	// Decimal and floating-point
	while (std::isdigit(static_cast<unsigned char>(current())) || current() == '.' || current() == 'e' || current() == 'E') {
		if (current() == '.') {
			// Check if this is a range operator (..)
			if (peek() == '.') break;
			if (isFloat) break;
			isFloat = true;
			numberStr += '.';
			advance();
			continue;
		}
		if (current() == 'e' || current() == 'E') {
			isFloat = true;
			numberStr += current();
			advance();
			if (current() == '+' || current() == '-') {
				numberStr += current();
				advance();
			}
			continue;
		}
		numberStr += current();
		advance();
	}

	TokenType type = isFloat ? TokenType::FLOAT_LITERAL_TOKEN : TokenType::INT_LITERAL_TOKEN;
	return Token(type, numberStr, line, startColumn);
}

Token Lexer::parseIdentifierOrKeyword() {
	int startColumn = column;
	std::string ident;

	while (std::isalnum(static_cast<unsigned char>(current())) || current() == '_') {
		ident += current();
		advance();
	}

	// Check for the special _ wildcard (standalone underscore)
	if (ident == "_") {
		return Token(TokenType::UNDERSCORE_TOKEN, "_", line, startColumn);
	}

	// Check if it's a keyword
	auto it = keywords.find(ident);
	if (it != keywords.end()) {
		return Token(it->second, ident, line, startColumn);
	}

	return Token(TokenType::IDENTIFIER_TOKEN, ident, line, startColumn);
}

Token Lexer::parseString() {
	int startColumn = column;
	std::string str;
	advance(); // consume opening "

	while (position < source.size() && current() != '"') {
		if (current() == '\\') {
			advance(); // consume backslash
			switch (current()) {
				case 'n': str += '\n'; break;
				case 't': str += '\t'; break;
				case '\\': str += '\\'; break;
				case '"': str += '"'; break;
				case '0': str += '\0'; break;
				case '{': str += '{'; break;  // escaped brace — not interpolation
				case '}': str += '}'; break;
				default: str += current(); break;
			}
		} else if (current() == '{') {
			// Start of string interpolation
			advance(); // consume '{'
			inInterpolation = true;
			interpBraceDepth = 0;
			return Token(TokenType::INTERP_STRING_START_TOKEN, str, line, startColumn);
		} else {
			str += current();
		}
		advance();
	}

	if (current() == '"') {
		advance(); // consume closing "
	}

	return Token(TokenType::STRING_LITERAL_TOKEN, str, line, startColumn);
}

Token Lexer::continueInterpolatedString() {
	int startColumn = column;
	advance(); // consume the closing '}'
	std::string str;

	while (position < source.size() && current() != '"') {
		if (current() == '\\') {
			advance(); // consume backslash
			switch (current()) {
				case 'n': str += '\n'; break;
				case 't': str += '\t'; break;
				case '\\': str += '\\'; break;
				case '"': str += '"'; break;
				case '0': str += '\0'; break;
				case '{': str += '{'; break;
				case '}': str += '}'; break;
				default: str += current(); break;
			}
			advance();
		} else if (current() == '{') {
			advance(); // consume '{'
			interpBraceDepth = 0;
			return Token(TokenType::INTERP_STRING_MID_TOKEN, str, line, startColumn);
		} else {
			str += current();
			advance();
		}
	}

	if (current() == '"') {
		advance(); // consume closing "
	}

	inInterpolation = false;
	return Token(TokenType::INTERP_STRING_END_TOKEN, str, line, startColumn);
}

Token Lexer::parseCharLiteral() {
	int startColumn = column;
	std::string ch;
	advance(); // consume opening '

	if (current() == '\\') {
		advance();
		switch (current()) {
			case 'n': ch += '\n'; break;
			case 't': ch += '\t'; break;
			case '\\': ch += '\\'; break;
			case '\'': ch += '\''; break;
			case '0': ch += '\0'; break;
			default: ch += current(); break;
		}
	} else {
		ch += current();
	}
	advance();

	if (current() == '\'') {
		advance(); // consume closing '
	}

	return Token(TokenType::CHAR_LITERAL_TOKEN, ch, line, startColumn);
}