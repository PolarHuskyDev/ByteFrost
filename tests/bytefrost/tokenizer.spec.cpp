#include <gtest/gtest.h>

#include "bytefrost/tokenizer/tokenizer.h"

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Tokenize src and strip the trailing EOF token.
static std::vector<Token> lex(const std::string& src) {
	Tokenizer t(src);
	auto tokens = t.tokenize();
	if (!tokens.empty() && tokens.back().type == TokenType::END_OF_FILE)
		tokens.pop_back();
	return tokens;
}

// Tokenize and return the very first (non-EOF) token.
static Token lexOne(const std::string& src) {
	return lex(src).front();
}

// ─── EOF ──────────────────────────────────────────────────────────────────────

TEST(TokenizerEOF, EmptySourceYieldsOnlyEOF) {
	Tokenizer t("");
	auto tokens = t.tokenize();
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST(TokenizerEOF, WhitespaceOnlyYieldsNoTokens) {
	EXPECT_TRUE(lex("   \t\n  ").empty());
}

// ─── Integer literals ─────────────────────────────────────────────────────────

TEST(TokenizerIntLiterals, Decimal) {
	Token t = lexOne("42");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "42");
}

TEST(TokenizerIntLiterals, Zero) {
	Token t = lexOne("0");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "0");
}

TEST(TokenizerIntLiterals, ThousandSeparators) {
	Token t = lexOne("1_000_000");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "1_000_000");
}

TEST(TokenizerIntLiterals, HexLowercase) {
	Token t = lexOne("0xff");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "0xff");
}

TEST(TokenizerIntLiterals, HexUppercase) {
	Token t = lexOne("0XFF");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "0XFF");
}

TEST(TokenizerIntLiterals, BinaryLowercase) {
	Token t = lexOne("0b1010");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "0b1010");
}

TEST(TokenizerIntLiterals, BinaryUppercase) {
	Token t = lexOne("0B1101");
	EXPECT_EQ(t.type, TokenType::INT_LITERAL);
	EXPECT_EQ(t.value, "0B1101");
}

TEST(TokenizerIntLiterals, InvalidHexNoDigits) {
	EXPECT_EQ(lexOne("0x").type, TokenType::UNKNOWN);
}

TEST(TokenizerIntLiterals, InvalidBinaryNoDigits) {
	EXPECT_EQ(lexOne("0b").type, TokenType::UNKNOWN);
}

TEST(TokenizerIntLiterals, InvalidSeparatorGroupTooLarge) {
	// Group of 4 digits — must be at most 3
	EXPECT_EQ(lexOne("1_0000").type, TokenType::UNKNOWN);
}

// ─── Float literals ───────────────────────────────────────────────────────────

TEST(TokenizerFloatLiterals, SimpleDecimal) {
	Token t = lexOne("3.14");
	EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(t.value, "3.14");
}

TEST(TokenizerFloatLiterals, ExponentOnly) {
	Token t = lexOne("1e5");
	EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(t.value, "1e5");
}

TEST(TokenizerFloatLiterals, DecimalWithNegativeExponent) {
	Token t = lexOne("1.5e-3");
	EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(t.value, "1.5e-3");
}

TEST(TokenizerFloatLiterals, DecimalWithPositiveExponent) {
	Token t = lexOne("2.0e+4");
	EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(t.value, "2.0e+4");
}

TEST(TokenizerFloatLiterals, LeadingDot) {
	Token t = lexOne(".75");
	EXPECT_EQ(t.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(t.value, ".75");
}

// ─── String literals ──────────────────────────────────────────────────────────

TEST(TokenizerStringLiterals, SimpleString) {
	Token t = lexOne("\"hello\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "hello");
}

TEST(TokenizerStringLiterals, EmptyString) {
	Token t = lexOne("\"\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "");
}

TEST(TokenizerStringLiterals, EscapeNewline) {
	Token t = lexOne("\"a\\nb\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "a\nb");
}

TEST(TokenizerStringLiterals, EscapeTab) {
	Token t = lexOne("\"a\\tb\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "a\tb");
}

TEST(TokenizerStringLiterals, EscapeCarriageReturn) {
	Token t = lexOne("\"a\\rb\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "a\rb");
}

TEST(TokenizerStringLiterals, EscapeBackslash) {
	Token t = lexOne("\"a\\\\b\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "a\\b");
}

TEST(TokenizerStringLiterals, EscapeDoubleQuote) {
	Token t = lexOne("\"a\\\"b\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "a\"b");
}

TEST(TokenizerStringLiterals, EscapeNullByte) {
	Token t = lexOne("\"a\\0b\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, (std::string{"a\0b", 3}));
}

// \{ and \} escape sequences let you write a literal brace in an interpolated string
TEST(TokenizerStringLiterals, EscapeBraces) {
	Token t = lexOne("\"\\{\\}\"");
	EXPECT_EQ(t.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(t.value, "{}");
}

TEST(TokenizerStringLiterals, UnknownEscapeIsUnknown) {
	EXPECT_EQ(lexOne("\"\\x\"").type, TokenType::UNKNOWN);
}

TEST(TokenizerStringLiterals, UnterminatedStringIsUnknown) {
	EXPECT_EQ(lexOne("\"hello").type, TokenType::UNKNOWN);
}

// ─── Boolean / null literals ──────────────────────────────────────────────────

TEST(TokenizerKeywordsLiterals, TrueLiteral) {
	EXPECT_EQ(lexOne("true").type, TokenType::TRUE_LITERAL);
}

TEST(TokenizerKeywordsLiterals, FalseLiteral) {
	EXPECT_EQ(lexOne("false").type, TokenType::FALSE_LITERAL);
}

TEST(TokenizerKeywordsLiterals, NullLiteral) {
	EXPECT_EQ(lexOne("null").type, TokenType::NULL_LITERAL);
}

// ─── Control-flow keywords ────────────────────────────────────────────────────

TEST(TokenizerKeywordsControlFlow, All) {
	EXPECT_EQ(lexOne("if").type,       TokenType::IF);
	EXPECT_EQ(lexOne("elseif").type,   TokenType::ELSEIF);
	EXPECT_EQ(lexOne("else").type,     TokenType::ELSE);
	EXPECT_EQ(lexOne("match").type,    TokenType::MATCH);
	EXPECT_EQ(lexOne("for").type,      TokenType::FOR);
	EXPECT_EQ(lexOne("while").type,    TokenType::WHILE);
	EXPECT_EQ(lexOne("break").type,    TokenType::BREAK);
	EXPECT_EQ(lexOne("continue").type, TokenType::CONTINUE);
	EXPECT_EQ(lexOne("return").type,   TokenType::RETURN);
}

// ─── Type keywords ────────────────────────────────────────────────────────────

TEST(TokenizerKeywordsTypes, PrimitiveTypes) {
	EXPECT_EQ(lexOne("void").type,   TokenType::VOID);
	EXPECT_EQ(lexOne("bool").type,   TokenType::BOOL);
	EXPECT_EQ(lexOne("string").type, TokenType::STRING);
}

TEST(TokenizerKeywordsTypes, IntegerTypes) {
	EXPECT_EQ(lexOne("i8").type,    TokenType::I8);
	EXPECT_EQ(lexOne("i16").type,   TokenType::I16);
	EXPECT_EQ(lexOne("i32").type,   TokenType::I32);
	EXPECT_EQ(lexOne("i64").type,   TokenType::I64);
	EXPECT_EQ(lexOne("i128").type,  TokenType::I128);
	EXPECT_EQ(lexOne("u8").type,    TokenType::U8);
	EXPECT_EQ(lexOne("u16").type,   TokenType::U16);
	EXPECT_EQ(lexOne("u32").type,   TokenType::U32);
	EXPECT_EQ(lexOne("u64").type,   TokenType::U64);
	EXPECT_EQ(lexOne("u128").type,  TokenType::U128);
	EXPECT_EQ(lexOne("isize").type, TokenType::ISIZE);
	EXPECT_EQ(lexOne("usize").type, TokenType::USIZE);
}

TEST(TokenizerKeywordsTypes, FloatTypes) {
	EXPECT_EQ(lexOne("f16").type,  TokenType::F16);
	EXPECT_EQ(lexOne("f32").type,  TokenType::F32);
	EXPECT_EQ(lexOne("f64").type,  TokenType::F64);
	EXPECT_EQ(lexOne("f128").type, TokenType::F128);
}

TEST(TokenizerKeywordsTypes, CustomTypeKeywords) {
	EXPECT_EQ(lexOne("struct").type,      TokenType::STRUCT);
	EXPECT_EQ(lexOne("enum").type,        TokenType::ENUM);
	EXPECT_EQ(lexOne("interface").type,   TokenType::INTERFACE);
	EXPECT_EQ(lexOne("implement").type,   TokenType::IMPLEMENT_METHOD);
	EXPECT_EQ(lexOne("implements").type,  TokenType::IMPLEMENTS_INTERFACE);
	EXPECT_EQ(lexOne("is").type,          TokenType::IS);
	EXPECT_EQ(lexOne("this").type,        TokenType::THIS);
	EXPECT_EQ(lexOne("operator").type,    TokenType::CUSTOM_OPERATOR);
}

TEST(TokenizerKeywordsModifiers, All) {
	EXPECT_EQ(lexOne("const").type,    TokenType::CONST);
	EXPECT_EQ(lexOne("readonly").type, TokenType::READONLY);
	EXPECT_EQ(lexOne("async").type,    TokenType::ASYNC);
	EXPECT_EQ(lexOne("await").type,    TokenType::AWAIT);
}

TEST(TokenizerKeywordsModuleSystem, All) {
	EXPECT_EQ(lexOne("import").type, TokenType::IMPORT);
	EXPECT_EQ(lexOne("export").type, TokenType::EXPORT);
	EXPECT_EQ(lexOne("from").type,   TokenType::IMPORT_FROM);
	EXPECT_EQ(lexOne("as").type,     TokenType::IMPORT_AS);
}

TEST(TokenizerKeywordsDataStructures, All) {
	EXPECT_EQ(lexOne("array").type, TokenType::ARRAY);
	EXPECT_EQ(lexOne("map").type,   TokenType::MAP);
	EXPECT_EQ(lexOne("in").type,    TokenType::ELEMENT_IN);
}

TEST(TokenizerKeywordsLogical, All) {
	EXPECT_EQ(lexOne("and").type, TokenType::LOGICAL_AND);
	EXPECT_EQ(lexOne("or").type,  TokenType::LOGICAL_OR);
	EXPECT_EQ(lexOne("not").type, TokenType::LOGICAL_NOT);
	EXPECT_EQ(lexOne("xor").type, TokenType::LOGICAL_XOR);
}

// ─── Operators ────────────────────────────────────────────────────────────────

TEST(TokenizerOperators, Arithmetic) {
	EXPECT_EQ(lexOne("+").type, TokenType::PLUS);
	EXPECT_EQ(lexOne("-").type, TokenType::MINUS);
	EXPECT_EQ(lexOne("*").type, TokenType::MULTIPLY);
	EXPECT_EQ(lexOne("/").type, TokenType::DIVIDE);
	EXPECT_EQ(lexOne("%").type, TokenType::MODULO);
}

TEST(TokenizerOperators, Assignment) {
	EXPECT_EQ(lexOne("=").type, TokenType::ASSIGN);
}

TEST(TokenizerOperators, Comparison) {
	EXPECT_EQ(lexOne("==").type, TokenType::EQUAL);
	EXPECT_EQ(lexOne("!=").type, TokenType::NOT_EQUAL);
	EXPECT_EQ(lexOne("<").type,  TokenType::LESS);
	EXPECT_EQ(lexOne("<=").type, TokenType::LESS_EQUAL);
	EXPECT_EQ(lexOne(">").type,  TokenType::GREATER);
	EXPECT_EQ(lexOne(">=").type, TokenType::GREATER_EQUAL);
}

TEST(TokenizerOperators, IncrementDecrement) {
	EXPECT_EQ(lexOne("++").type, TokenType::INCREMENT);
	EXPECT_EQ(lexOne("--").type, TokenType::DECREMENT);
}

TEST(TokenizerOperators, Bitwise) {
	EXPECT_EQ(lexOne("&").type, TokenType::BITWISE_AND);
	EXPECT_EQ(lexOne("|").type, TokenType::BITWISE_OR);
	EXPECT_EQ(lexOne("^").type, TokenType::BITWISE_XOR);
	EXPECT_EQ(lexOne("~").type, TokenType::BITWISE_NOT);
}

TEST(TokenizerOperators, SpecialOperators) {
	EXPECT_EQ(lexOne("?.").type, TokenType::QUESTION_MARK_DOT);
	EXPECT_EQ(lexOne("=>").type, TokenType::ARROW);
}

// `!` alone is UNKNOWN — logical negation uses the keyword `not`
TEST(TokenizerOperators, BangAloneIsUnknown) {
	EXPECT_EQ(lexOne("!").type, TokenType::UNKNOWN);
}

// ─── Delimiters ───────────────────────────────────────────────────────────────

TEST(TokenizerDelimiters, All) {
	EXPECT_EQ(lexOne("(").type, TokenType::LEFT_PAREN);
	EXPECT_EQ(lexOne(")").type, TokenType::RIGHT_PAREN);
	EXPECT_EQ(lexOne("{").type, TokenType::LEFT_BRACE);
	EXPECT_EQ(lexOne("}").type, TokenType::RIGHT_BRACE);
	EXPECT_EQ(lexOne("[").type, TokenType::LEFT_BRACKET);
	EXPECT_EQ(lexOne("]").type, TokenType::RIGHT_BRACKET);
	EXPECT_EQ(lexOne(",").type, TokenType::COMMA);
	EXPECT_EQ(lexOne(";").type, TokenType::SEMICOLON);
	EXPECT_EQ(lexOne(":").type, TokenType::COLON);
	EXPECT_EQ(lexOne(".").type, TokenType::DOT);
	EXPECT_EQ(lexOne("?").type, TokenType::QUESTION_MARK);
}

// ─── Identifiers ──────────────────────────────────────────────────────────────

TEST(TokenizerIdentifiers, Simple) {
	Token t = lexOne("foo");
	EXPECT_EQ(t.type,  TokenType::IDENTIFIER);
	EXPECT_EQ(t.value, "foo");
}

TEST(TokenizerIdentifiers, UpperCase) {
	Token t = lexOne("FooBar");
	EXPECT_EQ(t.type,  TokenType::IDENTIFIER);
	EXPECT_EQ(t.value, "FooBar");
}

TEST(TokenizerIdentifiers, WithInternalUnderscore) {
	Token t = lexOne("foo_bar");
	EXPECT_EQ(t.type,  TokenType::IDENTIFIER);
	EXPECT_EQ(t.value, "foo_bar");
}

TEST(TokenizerIdentifiers, WithTrailingUnderscore) {
	Token t = lexOne("foo_");
	EXPECT_EQ(t.type,  TokenType::IDENTIFIER);
	EXPECT_EQ(t.value, "foo_");
}

TEST(TokenizerIdentifiers, UnderscoreAloneIsUnderscore) {
	EXPECT_EQ(lexOne("_").type, TokenType::UNDERSCORE);
}

// A leading `_` produces UNDERSCORE then the rest is a separate IDENTIFIER
TEST(TokenizerIdentifiers, LeadingUnderscoreProducesTwoTokens) {
	auto tokens = lex("_foo");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].type,  TokenType::UNDERSCORE);
	EXPECT_EQ(tokens[1].type,  TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[1].value, "foo");
}

TEST(TokenizerIdentifiers, DoubleUnderscoreProducesTwoTokens) {
	auto tokens = lex("__");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].type, TokenType::UNDERSCORE);
	EXPECT_EQ(tokens[1].type, TokenType::UNDERSCORE);
}

TEST(TokenizerIdentifiers, KeywordsAreNotIdentifiers) {
	EXPECT_NE(lexOne("if").type,     TokenType::IDENTIFIER);
	EXPECT_NE(lexOne("return").type, TokenType::IDENTIFIER);
	EXPECT_NE(lexOne("i32").type,    TokenType::IDENTIFIER);
}

// ─── Comments ─────────────────────────────────────────────────────────────────

TEST(TokenizerComments, LineCommentSkipped) {
	auto tokens = lex("// this is a comment\nfoo");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].type,  TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[0].value, "foo");
}

TEST(TokenizerComments, LineCommentAtEndWithoutNewline) {
	EXPECT_TRUE(lex("// comment only").empty());
}

TEST(TokenizerComments, BlockCommentSkipped) {
	auto tokens = lex("/* block comment */ foo");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].type,  TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[0].value, "foo");
}

TEST(TokenizerComments, MultilineBlockCommentSkipped) {
	auto tokens = lex("/* line1\n   line2\n*/ bar");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].value, "bar");
}

TEST(TokenizerComments, InlineBlockComment) {
	auto tokens = lex("a /* mid */ b");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].value, "a");
	EXPECT_EQ(tokens[1].value, "b");
}

// ─── Position tracking ────────────────────────────────────────────────────────

TEST(TokenizerPositions, ColumnTracking) {
	auto tokens = lex("  foo");  // 'f' is the 3rd character
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].line,   1);
	EXPECT_EQ(tokens[0].column, 3);
}

TEST(TokenizerPositions, LineTracking) {
	auto tokens = lex("foo\nbar");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].line, 1);
	EXPECT_EQ(tokens[1].line, 2);
}

TEST(TokenizerPositions, SecondTokenColumnOnSameLine) {
	auto tokens = lex("a b");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].column, 1);
	EXPECT_EQ(tokens[1].column, 3);
}

TEST(TokenizerPositions, SecondTokenColumnOnNextLine) {
	auto tokens = lex("a\nb");
	ASSERT_EQ(tokens.size(), 2u);
	EXPECT_EQ(tokens[0].column, 1);
	EXPECT_EQ(tokens[1].column, 1);
}

TEST(TokenizerPositions, TabsCountAsOneColumn) {
	auto tokens = lex("\t\tfoo");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].column, 3);  // 'f' is the 3rd character (tabs count as 1 column)
}

TEST(TokenizerPositions, NewlinesInBlockComments) {
	auto tokens = lex("/*\n\n*/ foo");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].line, 3);    // 'foo' is on the 3rd line
	EXPECT_EQ(tokens[0].column, 4);  // 'f' is the 4th character on the line
}

TEST(TokenizerPositions, NewlinesInLineComments) {
	auto tokens = lex("// comment\n\nbar");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].line, 3);    // 'bar' is on the 3rd line
	EXPECT_EQ(tokens[0].column, 1);  // 'b' is the 1st character on the line
}

TEST(TokenizerPositions, NewlinesInStringLiterals) {
	auto tokens = lex("\"line1\nline2\"");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].line, 1);    // Token starts on line 1
	EXPECT_EQ(tokens[0].column, 1);  // Token starts at column 1
}

TEST(TokenizerPositions, NewlinesInStringLiteralsWithEscapes) {
	auto tokens = lex("\"line1\\nline2\"");
	ASSERT_EQ(tokens.size(), 1u);
	EXPECT_EQ(tokens[0].line, 1);    // Token starts on line 1
	EXPECT_EQ(tokens[0].column, 1);  // Token starts at column 1
}

TEST(TokenizerPositions, MultiLineVariableDeclaration) {
	auto tokens = lex("x\n=\n42\n;");
	ASSERT_EQ(tokens.size(), 4u);
	EXPECT_EQ(tokens[0].type,  TokenType::IDENTIFIER); EXPECT_EQ(tokens[0].value, "x");
	EXPECT_EQ(tokens[1].type,  TokenType::ASSIGN);
	EXPECT_EQ(tokens[2].type,  TokenType::INT_LITERAL); EXPECT_EQ(tokens[2].value, "42");
	EXPECT_EQ(tokens[3].type,  TokenType::SEMICOLON);
}

// ─── Multi-token sequences ────────────────────────────────────────────────────

TEST(TokenizerSequences, VariableDeclaration) {
	// x: i32 = 42;
	auto t = lex("x: i32 = 42;");
	ASSERT_EQ(t.size(), 6u);
	EXPECT_EQ(t[0].type,  TokenType::IDENTIFIER);  EXPECT_EQ(t[0].value, "x");
	EXPECT_EQ(t[1].type,  TokenType::COLON);
	EXPECT_EQ(t[2].type,  TokenType::I32);
	EXPECT_EQ(t[3].type,  TokenType::ASSIGN);
	EXPECT_EQ(t[4].type,  TokenType::INT_LITERAL); EXPECT_EQ(t[4].value, "42");
	EXPECT_EQ(t[5].type,  TokenType::SEMICOLON);
}

TEST(TokenizerSequences, IfStatement) {
	// if (x == 0) { return 1; }
	auto t = lex("if (x == 0) { return 1; }");
	ASSERT_EQ(t.size(), 11u);
	EXPECT_EQ(t[0].type,  TokenType::IF);
	EXPECT_EQ(t[1].type,  TokenType::LEFT_PAREN);
	EXPECT_EQ(t[2].type,  TokenType::IDENTIFIER);   EXPECT_EQ(t[2].value, "x");
	EXPECT_EQ(t[3].type,  TokenType::EQUAL);
	EXPECT_EQ(t[4].type,  TokenType::INT_LITERAL);  EXPECT_EQ(t[4].value, "0");
	EXPECT_EQ(t[5].type,  TokenType::RIGHT_PAREN);
	EXPECT_EQ(t[6].type,  TokenType::LEFT_BRACE);
	EXPECT_EQ(t[7].type,  TokenType::RETURN);
	EXPECT_EQ(t[8].type,  TokenType::INT_LITERAL);  EXPECT_EQ(t[8].value, "1");
	EXPECT_EQ(t[9].type,  TokenType::SEMICOLON);
	EXPECT_EQ(t[10].type, TokenType::RIGHT_BRACE);
}

TEST(TokenizerSequences, FunctionCall) {
	// add(a, b)
	auto t = lex("add(a, b)");
	ASSERT_EQ(t.size(), 6u);
	EXPECT_EQ(t[0].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[0].value, "add");
	EXPECT_EQ(t[1].type,  TokenType::LEFT_PAREN);
	EXPECT_EQ(t[2].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[2].value, "a");
	EXPECT_EQ(t[3].type,  TokenType::COMMA);
	EXPECT_EQ(t[4].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[4].value, "b");
	EXPECT_EQ(t[5].type,  TokenType::RIGHT_PAREN);
}

TEST(TokenizerSequences, ImportStatement) {
	// import Math from math;
	auto t = lex("import Math from math;");
	ASSERT_EQ(t.size(), 5u);
	EXPECT_EQ(t[0].type, TokenType::IMPORT);
	EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);  EXPECT_EQ(t[1].value, "Math");
	EXPECT_EQ(t[2].type, TokenType::IMPORT_FROM);
	EXPECT_EQ(t[3].type, TokenType::IDENTIFIER);  EXPECT_EQ(t[3].value, "math");
	EXPECT_EQ(t[4].type, TokenType::SEMICOLON);
}

TEST(TokenizerSequences, LogicalExpression) {
	// a and b or not c
	auto t = lex("a and b or not c");
	ASSERT_EQ(t.size(), 6u);
	EXPECT_EQ(t[0].type, TokenType::IDENTIFIER);
	EXPECT_EQ(t[1].type, TokenType::LOGICAL_AND);
	EXPECT_EQ(t[2].type, TokenType::IDENTIFIER);
	EXPECT_EQ(t[3].type, TokenType::LOGICAL_OR);
	EXPECT_EQ(t[4].type, TokenType::LOGICAL_NOT);
	EXPECT_EQ(t[5].type, TokenType::IDENTIFIER);
}

// TODO: {name} should be modified for string interpolation
// Right now it is passing through raw as part of the string literal,
// but eventually we want to recognize it as a separate token type for
// interpolation. For now, just test that braces are allowed inside string
// literals and not treated as escape sequences.
TEST(TokenizerSequences, StringInterpolation) {
	// "Hello {name}!" — braces inside strings are NOT escape sequences; they pass through raw
	auto t = lex("\"Hello {name}!\"");
	ASSERT_EQ(t.size(), 1u);
	EXPECT_EQ(t[0].type,  TokenType::STRING_LITERAL);
	EXPECT_EQ(t[0].value, "Hello {name}!");
}


TEST(TokenizerSequences, StringInterpolationWithEscapedBraces) {
	// "Hello \{name\}!" — \{ and \} are escape sequences for literal braces inside interpolated strings
	auto t = lex("\"Hello \\{name\\}!\"");
	ASSERT_EQ(t.size(), 1u);
	EXPECT_EQ(t[0].type,  TokenType::STRING_LITERAL);
	EXPECT_EQ(t[0].value, "Hello {name}!");
}

TEST(TokenizerSequences, ArithmeticExpression) {
	// 1 + 2 * (3 - 4)
	auto t = lex("1 + 2 * (3 - 4)");
	ASSERT_EQ(t.size(), 9u);
	EXPECT_EQ(t[0].type,  TokenType::INT_LITERAL); EXPECT_EQ(t[0].value, "1");
	EXPECT_EQ(t[1].type,  TokenType::PLUS);
	EXPECT_EQ(t[2].type,  TokenType::INT_LITERAL); EXPECT_EQ(t[2].value, "2");
	EXPECT_EQ(t[3].type,  TokenType::MULTIPLY);
	EXPECT_EQ(t[4].type,  TokenType::LEFT_PAREN);
	EXPECT_EQ(t[5].type,  TokenType::INT_LITERAL); EXPECT_EQ(t[5].value, "3");
	EXPECT_EQ(t[6].type,  TokenType::MINUS);
	EXPECT_EQ(t[7].type,  TokenType::INT_LITERAL); EXPECT_EQ(t[7].value, "4");
	EXPECT_EQ(t[8].type,  TokenType::RIGHT_PAREN);
}

TEST(TokenizerSequences, ComplexExpression) {
	// foo(x, y) + 3.14 * bar(z)
	auto t = lex("foo(x, y) + 3.14 * bar(z)");
	ASSERT_EQ(t.size(), 13u);
	EXPECT_EQ(t[0].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[0].value, "foo");
	EXPECT_EQ(t[1].type,  TokenType::LEFT_PAREN);
	EXPECT_EQ(t[2].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[2].value, "x");
	EXPECT_EQ(t[3].type,  TokenType::COMMA);
	EXPECT_EQ(t[4].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[4].value, "y");
	EXPECT_EQ(t[5].type,  TokenType::RIGHT_PAREN);
	EXPECT_EQ(t[6].type,  TokenType::PLUS);
	EXPECT_EQ(t[7].type,  TokenType::FLOAT_LITERAL); EXPECT_EQ(t[7].value, "3.14");
	EXPECT_EQ(t[8].type,  TokenType::MULTIPLY);
	EXPECT_EQ(t[9].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[9].value, "bar");
	EXPECT_EQ(t[10].type, TokenType::LEFT_PAREN);
	EXPECT_EQ(t[11].type, TokenType::IDENTIFIER); EXPECT_EQ(t[11].value, "z");
	EXPECT_EQ(t[12].type, TokenType::RIGHT_PAREN);
}

TEST(TokenizerSequences, ConstVariableDeclaration) {
	// const pi: f64 = 3.14;
	auto t = lex("const pi: f64 = 3.14;");
	ASSERT_EQ(t.size(), 7u);
	EXPECT_EQ(t[0].type,  TokenType::CONST);
	EXPECT_EQ(t[1].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[1].value, "pi");
	EXPECT_EQ(t[2].type,  TokenType::COLON);
	EXPECT_EQ(t[3].type,  TokenType::F64);
	EXPECT_EQ(t[4].type,  TokenType::ASSIGN);
	EXPECT_EQ(t[5].type,  TokenType::FLOAT_LITERAL); EXPECT_EQ(t[5].value, "3.14");
	EXPECT_EQ(t[6].type,  TokenType::SEMICOLON);
}

TEST(TokenizerSequences, AsyncFunctionDeclaration) {
	// async foo(): void {}
	auto t = lex("async foo(): void {}");
	ASSERT_EQ(t.size(), 8u);
	EXPECT_EQ(t[0].type,  TokenType::ASYNC);
	EXPECT_EQ(t[1].type,  TokenType::IDENTIFIER); EXPECT_EQ(t[1].value, "foo");
	EXPECT_EQ(t[2].type,  TokenType::LEFT_PAREN);
	EXPECT_EQ(t[3].type,  TokenType::RIGHT_PAREN);
	EXPECT_EQ(t[4].type,  TokenType::COLON);
	EXPECT_EQ(t[5].type,  TokenType::VOID);
	EXPECT_EQ(t[6].type,  TokenType::LEFT_BRACE);
	EXPECT_EQ(t[7].type,  TokenType::RIGHT_BRACE);
}
