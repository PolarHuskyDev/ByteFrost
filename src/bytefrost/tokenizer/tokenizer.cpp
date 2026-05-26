#include "bytefrost/tokenizer/tokenizer.h"

const std::unordered_map<std::string, TokenType> Tokenizer::keywords = {
	// Keywords - control flow
	{"if", TokenType::IF},
	{"elseif", TokenType::ELSEIF},
	{"else", TokenType::ELSE},
	{"match", TokenType::MATCH},
	{"for", TokenType::FOR},
	{"while", TokenType::WHILE},
	{"break", TokenType::BREAK},
	{"continue", TokenType::CONTINUE},
	{"return", TokenType::RETURN},
	// Keywords - type modifiers
	{"const", TokenType::CONST},
	{"readonly", TokenType::READONLY},
	// Keywords - runtime modifiers
	{"async", TokenType::ASYNC},
	{"await", TokenType::AWAIT},
	// Keywords - types
	{"void", TokenType::VOID},
	{"bool", TokenType::BOOL},
	{"string", TokenType::STRING},
	// Keywords - integer types
	{"i8", TokenType::I8}, {"i16", TokenType::I16}, {"i32", TokenType::I32}, {"i64", TokenType::I64}, {"i128", TokenType::I128},
	{"u8", TokenType::U8}, {"u16", TokenType::U16}, {"u32", TokenType::U32}, {"u64", TokenType::U64}, {"u128", TokenType::U128},
	{"isize", TokenType::ISIZE}, {"usize", TokenType::USIZE},
	// Keywords - float types
	{"f16", TokenType::F16}, {"f32", TokenType::F32}, {"f64", TokenType::F64}, {"f128", TokenType::F128},
	// Keywords - custom types
	{"struct", TokenType::STRUCT},
	{"is", TokenType::IS},
	{"this", TokenType::THIS},
	{"enum", TokenType::ENUM},
	{"interface", TokenType::INTERFACE},
	{"implement", TokenType::IMPLEMENT_METHOD},
	{"implements", TokenType::IMPLEMENTS_INTERFACE},
	{"operator", TokenType::CUSTOM_OPERATOR},
	// Keywords - data structures, containers, iterators, and utilities
	{"array", TokenType::ARRAY},
	{"map", TokenType::MAP},
	{"in", TokenType::ELEMENT_IN},
	// Keywords - module system
	{"import", TokenType::IMPORT},
	{"export", TokenType::EXPORT},
	{"from", TokenType::IMPORT_FROM},
	{"as", TokenType::IMPORT_AS},
	// Literals
	{"true", TokenType::TRUE_LITERAL},
	{"false", TokenType::FALSE_LITERAL},
	{"null", TokenType::NULL_LITERAL},
	// Operators - logical
	{"and", TokenType::LOGICAL_AND},
	{"or", TokenType::LOGICAL_OR},
	{"not", TokenType::LOGICAL_NOT},
	{"xor", TokenType::LOGICAL_XOR},
};

const std::unordered_map<std::string, char> Tokenizer::escapeSequences = {
	{"n",  '\n'},
	{"t",  '\t'},
	{"r",  '\r'},
	{"\\", '\\'},
	{"'",  '\''},
	{"{",  '{'},
	{"}",  '}'},
	{"\"", '\"'},
	{"0",  '\0'},
};

const std::unordered_map<std::string, TokenType> Tokenizer::singleCharTokens = {
	{"+", TokenType::PLUS},
	{"-", TokenType::MINUS},
	{"*", TokenType::MULTIPLY},
	{"/", TokenType::DIVIDE},
	{"%", TokenType::MODULO},
	{"=", TokenType::ASSIGN},
	{"&", TokenType::BITWISE_AND},
	{"|", TokenType::BITWISE_OR},
	{"^", TokenType::BITWISE_XOR},
	{"~", TokenType::BITWISE_NOT},
	{"<", TokenType::LESS},
	{">", TokenType::GREATER},
	{"(", TokenType::LEFT_PAREN},
	{")", TokenType::RIGHT_PAREN},
	{"{", TokenType::LEFT_BRACE},
	{"}", TokenType::RIGHT_BRACE},
	{"[", TokenType::LEFT_BRACKET},
	{"]", TokenType::RIGHT_BRACKET},
	{",", TokenType::COMMA},
	{";", TokenType::SEMICOLON},
	{":", TokenType::COLON},
	{".", TokenType::DOT},
	{"?", TokenType::QUESTION_MARK},
	{"_", TokenType::UNDERSCORE},
};

char Tokenizer::current() const {
	if (position < source.size())
		return source[position];
	return '\0';
}

char Tokenizer::peek() const {
	if (position + 1 < source.size())
		return source[position + 1];
	return '\0';
}

void Tokenizer::advance() {
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

// ====================
// Skipping whitespace and comments
// ====================

void Tokenizer::skipWhitespace() {
	while (position < source.size() && std::isspace(static_cast<unsigned char>(current()))) {
		advance();
	}
}

void Tokenizer::skipLineComment() {
	// Skip past the opening //
	advance();	// consume first '/'
	advance();	// consume second '/'
	while (position < source.size() && current() != '\n') {
		advance();
	}
}

void Tokenizer::skipBlockComment() {
	// Skip past the opening /*
	advance();	// consume '/'
	advance();	// consume '*'
	while (position < source.size()) {
		if (current() == '*' && peek() == '/') {
			advance();	// consume '*'
			advance();	// consume '/'
			return;
		}
		advance();
	}
}

// ====================
// Main tokenization logic
// ====================

std::vector<Token> Tokenizer::tokenize() {
	std::vector<Token> tokens;
	while (true) {
		Token token = nextToken();
		tokens.push_back(token);
		if (token.type == TokenType::END_OF_FILE) {
			break;
		}
	}
	return tokens;
}

Token Tokenizer::nextToken() {
	skipWhitespace();
	char c = current();
	
	if (c == '\0') {
		return Token(TokenType::END_OF_FILE, "", line, column);
	}

	if (c == '/' && peek() == '/') {
		skipLineComment();
		return nextToken();
	}

	if (c == '/' && peek() == '*') {
		skipBlockComment();
		return nextToken();
	}

	if (c == '"') {
		return parseStringLiteral();
	}

	if (isdigit(static_cast<unsigned char>(c)) || (c == '.' && isdigit(  static_cast<unsigned char>(peek()) ))) {
		return parseNumberLiteral();
	}

	if (isalpha(static_cast<unsigned char>(c))) {
		return parseIdentifierOrKeyword();
	}

	if (isSpecialCharacter(c)) {
		return parseOperatorOrDelimiter();
	}

	int startColumn = column;
	advance();
	return Token(TokenType::UNKNOWN, std::string(1, c), line, startColumn);
}

// ====================
// Parsing methods
// ====================

bool Tokenizer::isSpecialCharacter(char c) const {
	return std::string("+-*/%=!<>&|^~(){}[];,.:?_").find(c) != std::string::npos;
}

Token Tokenizer::parseStringLiteral() {
	int startColumn = column;
	int startLine = line;
	std::string value = "";
	advance();  // consume opening quote

	while (position < source.size()) {
		char c = current();

		if (c == '"') {
			advance();  // consume closing quote
			return Token(TokenType::STRING_LITERAL, value, startLine, startColumn);
		}

		if (c == '\\') {
			advance();  // consume backslash
			if (position >= source.size()) break;  // EOF after backslash
			char escapeChar = current();
			auto it = escapeSequences.find(std::string(1, escapeChar));
			if (it != escapeSequences.end()) {
				value += static_cast<char>(it->second);
			} else {
				// Unknown escape sequence, fail with unknown token
				return Token(TokenType::UNKNOWN, std::string("\\") + escapeChar, startLine, startColumn);
			}
			
			advance();  // consume escape character
		} else {
			value += c;
			advance();
		}
	}
	return Token(TokenType::UNKNOWN, value, startLine, startColumn);
}

Token Tokenizer::parseNumberLiteral() {
	int startColumn = column;
	std::string value;
	char c = current();

	// ── Hex literal: 0x… / 0X… ────────────────────────────────────────────────
	if (c == '0' && (peek() == 'x' || peek() == 'X')) {
		value += c; advance();          // '0'
		value += current(); advance();  // 'x' / 'X'
		if (!isxdigit(static_cast<unsigned char>(current()))) {
			return Token(TokenType::UNKNOWN, value, line, startColumn);
		}
		while (position < source.size() && isxdigit(static_cast<unsigned char>(current()))) {
			value += current(); advance();
		}
		return Token(TokenType::INT_LITERAL, value, line, startColumn);
	}

	// ── Binary literal: 0b… / 0B… ─────────────────────────────────────────────
	if (c == '0' && (peek() == 'b' || peek() == 'B')) {
		value += c; advance();          // '0'
		value += current(); advance();  // 'b' / 'B'
		if (current() != '0' && current() != '1') {
			return Token(TokenType::UNKNOWN, value, line, startColumn);
		}
		while (position < source.size() && (current() == '0' || current() == '1')) {
			value += current(); advance();
		}
		return Token(TokenType::INT_LITERAL, value, line, startColumn);
	}

	// ── Decimal integer or float ───────────────────────────────────────────────
	bool hasDecimalPoint = false;
	bool hasExponent = false;
	// Tracks only the pre-decimal portion (digits + '_') for grouping validation
	std::string integerPart;

	// Leading decimal point (.75, .3e4, .6e-3)
	if (c == '.') {
		hasDecimalPoint = true;
		value += '.';
		advance();
	}

	// Integer part digits (and '_' thousand separators before the decimal point)
	while (position < source.size()) {
		c = current();
		if (isdigit(static_cast<unsigned char>(c))) {
			value += c;
			if (!hasDecimalPoint) integerPart += c;
			advance();
		} else if (c == '_' && !hasDecimalPoint) {
			value += c;
			integerPart += c;
			advance();
		} else {
			break;
		}
	}

	// Decimal point (only if not already seen)
	if (!hasDecimalPoint && current() == '.') {
		hasDecimalPoint = true;
		value += '.';
		advance();
		while (position < source.size() && isdigit(static_cast<unsigned char>(current()))) {
			value += current(); advance();
		}
	}

	// Exponent: only consume when followed by digits (with optional +/- sign)
	if (current() == 'e' || current() == 'E') {
		size_t lookahead = position + 1;
		char nextC = lookahead < source.size() ? source[lookahead] : '\0';
		if (nextC == '+' || nextC == '-') {
			nextC = ++lookahead < source.size() ? source[lookahead] : '\0';
		}
		if (isdigit(static_cast<unsigned char>(nextC))) {
			hasExponent = true;
			value += current(); advance();  // 'e' / 'E'
			if (current() == '+' || current() == '-') {
				value += current(); advance();
			}
			while (position < source.size() && isdigit(static_cast<unsigned char>(current()))) {
				value += current(); advance();
			}
		}
	}

	if (hasDecimalPoint || hasExponent) {
		return Token(TokenType::FLOAT_LITERAL, value, line, startColumn);
	}

	// Validate '_' grouping: each segment between separators must be 1–3 digits
	if (integerPart.find('_') != std::string::npos) {
		size_t pos = 0;
		while (pos <= integerPart.size()) {
			size_t next = integerPart.find('_', pos);
			if (next == std::string::npos) next = integerPart.size();
			size_t groupLen = next - pos;
			if (groupLen == 0 || groupLen > 3) {
				return Token(TokenType::UNKNOWN, value, line, startColumn);
			}
			pos = next + 1;
		}
	}

	return Token(TokenType::INT_LITERAL, value, line, startColumn);
}

Token Tokenizer::parseIdentifierOrKeyword() {
	int startColumn = column;
	std::string value;
	while (position < source.size() && (isalnum(static_cast<unsigned char>(current())) || current() == '_')) {
		value += current();
		advance();
	}

	auto it = keywords.find(value);
	if (it != keywords.end()) {
		return Token(it->second, value, line, startColumn);
	}
	return Token(TokenType::IDENTIFIER, value, line, startColumn);
}

Token Tokenizer::parseOperatorOrDelimiter() {
	int startColumn = column;
	std::string value;
	char c = current();
	value += c;
	advance();

	// Check for multi-character operators
	if (c == '+' && current() == '+') {
		value += current(); advance();
		return Token(TokenType::INCREMENT, value, line, startColumn);
	}
	if (c == '-' && current() == '-') {
		value += current(); advance();
		return Token(TokenType::DECREMENT, value, line, startColumn);
	}
	if (c == '=' && current() == '=') {
		value += current(); advance();
		return Token(TokenType::EQUAL, value, line, startColumn);
	}
	if (c == '!' && current() == '=') {
		value += current(); advance();
		return Token(TokenType::NOT_EQUAL, value, line, startColumn);
	}
	if (c == '<' && current() == '=') {
		value += current(); advance();
		return Token(TokenType::LESS_EQUAL, value, line, startColumn);
	}
	if (c == '>' && current() == '=') {
		value += current(); advance();
		return Token(TokenType::GREATER_EQUAL, value, line, startColumn);
	}
	if (c == '?' && current() == '.') {
		value += current(); advance();
		return Token(TokenType::QUESTION_MARK_DOT, value, line, startColumn);
	}
	if (c == '=' && current() == '>') {
		value += current(); advance();
		return Token(TokenType::ARROW, value, line, startColumn);
	}

	auto it = singleCharTokens.find(value);
	if (it != singleCharTokens.end()) {
		return Token(it->second, value, line, startColumn);
	}
	return Token(TokenType::UNKNOWN, value, line, startColumn);
}
