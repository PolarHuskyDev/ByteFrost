#include <gtest/gtest.h>

#include "tokenizer/lexer.h"
#include "tokenizer/tokens.h"

// Helper: tokenize source and return all tokens (including EOF)
static std::vector<Token> lex(const std::string& source) {
	Lexer lexer(source);
	return lexer.tokenize();
}

// Convenience: check a token's type and value
static void expectToken(const Token& t, TokenType type, const std::string& value) {
	EXPECT_EQ(t.type, type) << "Expected " << tokenTypeToString(type) << " but got "
							<< tokenTypeToString(t.type) << " (value: \"" << t.value << "\")";
	EXPECT_EQ(t.value, value);
}

// ============================================================
// Basic tokens
// ============================================================

TEST(LexerBasic, EmptySource) {
	auto tokens = lex("");
	ASSERT_EQ(tokens.size(), 1u);
	expectToken(tokens[0], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, SingleOperators) {
	auto tokens = lex("+ - * / % = ( ) < > ; :");
	ASSERT_EQ(tokens.size(), 13u);
	expectToken(tokens[0], TokenType::PLUS_TOKEN, "+");
	expectToken(tokens[1], TokenType::MINUS_TOKEN, "-");
	expectToken(tokens[2], TokenType::MULTIPLY_TOKEN, "*");
	expectToken(tokens[3], TokenType::DIVIDE_TOKEN, "/");
	expectToken(tokens[4], TokenType::MODULO_TOKEN, "%");
	expectToken(tokens[5], TokenType::ASSIGN_TOKEN, "=");
	expectToken(tokens[6], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[7], TokenType::RIGHT_PAREN_TOKEN, ")");
	expectToken(tokens[8], TokenType::LESS_TOKEN, "<");
	expectToken(tokens[9], TokenType::GREATER_TOKEN, ">");
	expectToken(tokens[10], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[11], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[12], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, TwoCharOperators) {
	auto tokens = lex("== != <= >=");
	ASSERT_EQ(tokens.size(), 5u);
	expectToken(tokens[0], TokenType::EQUAL_TOKEN, "==");
	expectToken(tokens[1], TokenType::NOT_EQUAL_TOKEN, "!=");
	expectToken(tokens[2], TokenType::LESS_EQUAL_TOKEN, "<=");
	expectToken(tokens[3], TokenType::GREATER_EQUAL_TOKEN, ">=");
	expectToken(tokens[4], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, NumberLiterals) {
	auto tokens = lex("0x1A 0b1010 3.14 2e10 1.5e-3");
	ASSERT_EQ(tokens.size(), 6u);
	expectToken(tokens[0], TokenType::INT_LITERAL_TOKEN, "0x1A");
	expectToken(tokens[1], TokenType::INT_LITERAL_TOKEN, "0b1010");
	expectToken(tokens[2], TokenType::FLOAT_LITERAL_TOKEN, "3.14");
	expectToken(tokens[3], TokenType::FLOAT_LITERAL_TOKEN, "2e10");
	expectToken(tokens[4], TokenType::FLOAT_LITERAL_TOKEN, "1.5e-3");
	expectToken(tokens[5], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, CombinedOperators) {
	auto tokens = lex("1+2*(3-4)/5%6");
	ASSERT_EQ(tokens.size(), 14u);
	expectToken(tokens[0], TokenType::INT_LITERAL_TOKEN, "1");
	expectToken(tokens[1], TokenType::PLUS_TOKEN, "+");
	expectToken(tokens[2], TokenType::INT_LITERAL_TOKEN, "2");
	expectToken(tokens[3], TokenType::MULTIPLY_TOKEN, "*");
	expectToken(tokens[4], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[5], TokenType::INT_LITERAL_TOKEN, "3");
	expectToken(tokens[6], TokenType::MINUS_TOKEN, "-");
	expectToken(tokens[7], TokenType::INT_LITERAL_TOKEN, "4");
	expectToken(tokens[8], TokenType::RIGHT_PAREN_TOKEN, ")");
	expectToken(tokens[9], TokenType::DIVIDE_TOKEN, "/");
	expectToken(tokens[10], TokenType::INT_LITERAL_TOKEN, "5");
	expectToken(tokens[11], TokenType::MODULO_TOKEN, "%");
	expectToken(tokens[12], TokenType::INT_LITERAL_TOKEN, "6");
	expectToken(tokens[13], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, UnknownTokens) {
	auto tokens = lex("@ # $");
	ASSERT_EQ(tokens.size(), 4u);
	expectToken(tokens[0], TokenType::UNKNOWN_TOKEN, "@");
	expectToken(tokens[1], TokenType::UNKNOWN_TOKEN, "#");
	expectToken(tokens[2], TokenType::UNKNOWN_TOKEN, "$");
	expectToken(tokens[3], TokenType::EOF_TOKEN, "");
}

TEST(LexerBasic, ComparisonOperators) {
	auto tokens = lex("= == != < <= > >=");
	ASSERT_EQ(tokens.size(), 8u);
	expectToken(tokens[0], TokenType::ASSIGN_TOKEN, "=");
	expectToken(tokens[1], TokenType::EQUAL_TOKEN, "==");
	expectToken(tokens[2], TokenType::NOT_EQUAL_TOKEN, "!=");
	expectToken(tokens[3], TokenType::LESS_TOKEN, "<");
	expectToken(tokens[4], TokenType::LESS_EQUAL_TOKEN, "<=");
	expectToken(tokens[5], TokenType::GREATER_TOKEN, ">");
	expectToken(tokens[6], TokenType::GREATER_EQUAL_TOKEN, ">=");
	expectToken(tokens[7], TokenType::EOF_TOKEN, "");
}

// ============================================================
// New token types
// ============================================================

TEST(LexerKeywords, AllKeywords) {
	auto tokens = lex("if else elseif for while return break continue match struct true false this in void int float bool char string array slice map");
	ASSERT_EQ(tokens.size(), 24u);
	expectToken(tokens[0], TokenType::IF_TOKEN, "if");
	expectToken(tokens[1], TokenType::ELSE_TOKEN, "else");
	expectToken(tokens[2], TokenType::ELSEIF_TOKEN, "elseif");
	expectToken(tokens[3], TokenType::FOR_TOKEN, "for");
	expectToken(tokens[4], TokenType::WHILE_TOKEN, "while");
	expectToken(tokens[5], TokenType::RETURN_TOKEN, "return");
	expectToken(tokens[6], TokenType::BREAK_TOKEN, "break");
	expectToken(tokens[7], TokenType::CONTINUE_TOKEN, "continue");
	expectToken(tokens[8], TokenType::MATCH_TOKEN, "match");
	expectToken(tokens[9], TokenType::STRUCT_TOKEN, "struct");
	expectToken(tokens[10], TokenType::TRUE_TOKEN, "true");
	expectToken(tokens[11], TokenType::FALSE_TOKEN, "false");
	expectToken(tokens[12], TokenType::THIS_TOKEN, "this");
	expectToken(tokens[13], TokenType::IN_TOKEN, "in");
	expectToken(tokens[14], TokenType::VOID_TOKEN, "void");
	expectToken(tokens[15], TokenType::INT_TOKEN, "int");
	expectToken(tokens[16], TokenType::FLOAT_TOKEN, "float");
	expectToken(tokens[17], TokenType::BOOL_TOKEN, "bool");
	expectToken(tokens[18], TokenType::CHAR_TOKEN, "char");
	expectToken(tokens[19], TokenType::STRING_TOKEN, "string");
	expectToken(tokens[20], TokenType::ARRAY_TOKEN, "array");
	expectToken(tokens[21], TokenType::SLICE_TOKEN, "slice");
	expectToken(tokens[22], TokenType::MAP_TOKEN, "map");
	expectToken(tokens[23], TokenType::EOF_TOKEN, "");
}

TEST(LexerKeywords, IdentifierVsKeyword) {
	auto tokens = lex("ifx return_val iff my_int");
	ASSERT_EQ(tokens.size(), 5u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "ifx");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "return_val");
	expectToken(tokens[2], TokenType::IDENTIFIER_TOKEN, "iff");
	expectToken(tokens[3], TokenType::IDENTIFIER_TOKEN, "my_int");
	expectToken(tokens[4], TokenType::EOF_TOKEN, "");
}

TEST(LexerStrings, SimpleString) {
	auto tokens = lex("\"Hello, World!\"");
	ASSERT_EQ(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::STRING_LITERAL_TOKEN, "Hello, World!");
	expectToken(tokens[1], TokenType::EOF_TOKEN, "");
}

TEST(LexerStrings, EscapeSequences) {
	auto tokens = lex("\"line1\\nline2\\ttab\\\\backslash\"");
	ASSERT_EQ(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::STRING_LITERAL_TOKEN, "line1\nline2\ttab\\backslash");
}

TEST(LexerStrings, EmptyString) {
	auto tokens = lex("\"\"");
	ASSERT_EQ(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::STRING_LITERAL_TOKEN, "");
}

TEST(LexerChars, SimpleChar) {
	auto tokens = lex("'a'");
	ASSERT_EQ(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::CHAR_LITERAL_TOKEN, "a");
}

TEST(LexerChars, EscapedChar) {
	auto tokens = lex("'\\n'");
	ASSERT_EQ(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::CHAR_LITERAL_TOKEN, "\n");
}

TEST(LexerDelimiters, BracesAndBrackets) {
	auto tokens = lex("{ } [ ]");
	ASSERT_EQ(tokens.size(), 5u);
	expectToken(tokens[0], TokenType::LEFT_BRACE_TOKEN, "{");
	expectToken(tokens[1], TokenType::RIGHT_BRACE_TOKEN, "}");
	expectToken(tokens[2], TokenType::LEFT_BRACKET_TOKEN, "[");
	expectToken(tokens[3], TokenType::RIGHT_BRACKET_TOKEN, "]");
	expectToken(tokens[4], TokenType::EOF_TOKEN, "");
}

TEST(LexerDelimiters, CommaAndDot) {
	auto tokens = lex(", .");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::COMMA_TOKEN, ",");
	expectToken(tokens[1], TokenType::DOT_TOKEN, ".");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, LogicalOperators) {
	auto tokens = lex("&& || ! ^^");
	ASSERT_EQ(tokens.size(), 5u);
	expectToken(tokens[0], TokenType::AND_TOKEN, "&&");
	expectToken(tokens[1], TokenType::OR_TOKEN, "||");
	expectToken(tokens[2], TokenType::NOT_TOKEN, "!");
	expectToken(tokens[3], TokenType::XOR_TOKEN, "^^");
	expectToken(tokens[4], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, BitwiseOperators) {
	auto tokens = lex("& | ^ ~ << >>");
	ASSERT_EQ(tokens.size(), 7u);
	expectToken(tokens[0], TokenType::BIT_AND_TOKEN, "&");
	expectToken(tokens[1], TokenType::BIT_OR_TOKEN, "|");
	expectToken(tokens[2], TokenType::BIT_XOR_TOKEN, "^");
	expectToken(tokens[3], TokenType::BIT_NOT_TOKEN, "~");
	expectToken(tokens[4], TokenType::LEFT_SHIFT_TOKEN, "<<");
	expectToken(tokens[5], TokenType::RIGHT_SHIFT_TOKEN, ">>");
	expectToken(tokens[6], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, CompoundAssignment) {
	auto tokens = lex("+= -= *= /= %=");
	ASSERT_EQ(tokens.size(), 6u);
	expectToken(tokens[0], TokenType::PLUS_ASSIGN_TOKEN, "+=");
	expectToken(tokens[1], TokenType::MINUS_ASSIGN_TOKEN, "-=");
	expectToken(tokens[2], TokenType::MULTIPLY_ASSIGN_TOKEN, "*=");
	expectToken(tokens[3], TokenType::DIVIDE_ASSIGN_TOKEN, "/=");
	expectToken(tokens[4], TokenType::MODULO_ASSIGN_TOKEN, "%=");
	expectToken(tokens[5], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, IncrementDecrement) {
	auto tokens = lex("++ --");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::INCREMENT_TOKEN, "++");
	expectToken(tokens[1], TokenType::DECREMENT_TOKEN, "--");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, WalrusAndArrow) {
	auto tokens = lex(":= =>");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::WALRUS_TOKEN, ":=");
	expectToken(tokens[1], TokenType::ARROW_TOKEN, "=>");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, DotDotRange) {
	auto tokens = lex("0..10");
	ASSERT_EQ(tokens.size(), 4u);
	expectToken(tokens[0], TokenType::INT_LITERAL_TOKEN, "0");
	expectToken(tokens[1], TokenType::DOTDOT_TOKEN, "..");
	expectToken(tokens[2], TokenType::INT_LITERAL_TOKEN, "10");
	expectToken(tokens[3], TokenType::EOF_TOKEN, "");
}

TEST(LexerOperators, Underscore) {
	auto tokens = lex("_ =>");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::UNDERSCORE_TOKEN, "_");
	expectToken(tokens[1], TokenType::ARROW_TOKEN, "=>");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

TEST(LexerComments, LineComment) {
	auto tokens = lex("a // this is a comment\nb");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "a");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "b");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

TEST(LexerComments, BlockComment) {
	auto tokens = lex("a /* block\ncomment */ b");
	ASSERT_EQ(tokens.size(), 3u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "a");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "b");
	expectToken(tokens[2], TokenType::EOF_TOKEN, "");
}

// ============================================================
// Full program tokenization (.bf examples)
// ============================================================

TEST(LexerPrograms, HelloWorld) {
	auto tokens = lex(R"(
main(): int {
	print("Hello, World!");
	return 0;
}
)");
	// main ( ) : int { print ( "Hello, World!" ) ; return 0 ; } EOF
	ASSERT_GE(tokens.size(), 14u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "main");
	expectToken(tokens[1], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[2], TokenType::RIGHT_PAREN_TOKEN, ")");
	expectToken(tokens[3], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[4], TokenType::INT_TOKEN, "int");
	expectToken(tokens[5], TokenType::LEFT_BRACE_TOKEN, "{");
	expectToken(tokens[6], TokenType::IDENTIFIER_TOKEN, "print");
	expectToken(tokens[7], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[8], TokenType::STRING_LITERAL_TOKEN, "Hello, World!");
	expectToken(tokens[9], TokenType::RIGHT_PAREN_TOKEN, ")");
	expectToken(tokens[10], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[11], TokenType::RETURN_TOKEN, "return");
	expectToken(tokens[12], TokenType::INT_LITERAL_TOKEN, "0");
	expectToken(tokens[13], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[14], TokenType::RIGHT_BRACE_TOKEN, "}");
}

TEST(LexerPrograms, VariableDeclaration) {
	auto tokens = lex("x: int = 3;");
	ASSERT_EQ(tokens.size(), 7u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "x");
	expectToken(tokens[1], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[2], TokenType::INT_TOKEN, "int");
	expectToken(tokens[3], TokenType::ASSIGN_TOKEN, "=");
	expectToken(tokens[4], TokenType::INT_LITERAL_TOKEN, "3");
	expectToken(tokens[5], TokenType::SEMICOLON_TOKEN, ";");
}

TEST(LexerPrograms, WalrusDeclaration) {
	auto tokens = lex("f := a & 0xFF;");
	ASSERT_EQ(tokens.size(), 7u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "f");
	expectToken(tokens[1], TokenType::WALRUS_TOKEN, ":=");
	expectToken(tokens[2], TokenType::IDENTIFIER_TOKEN, "a");
	expectToken(tokens[3], TokenType::BIT_AND_TOKEN, "&");
	expectToken(tokens[4], TokenType::INT_LITERAL_TOKEN, "0xFF");
	expectToken(tokens[5], TokenType::SEMICOLON_TOKEN, ";");
}

TEST(LexerPrograms, ForLoop) {
	auto tokens = lex("for (i: int = 0 ; i < 10 ; i++) {}");
	// for ( i : int = 0 ; i < 10 ; i ++ ) { }
	ASSERT_GE(tokens.size(), 15u);
	expectToken(tokens[0], TokenType::FOR_TOKEN, "for");
	expectToken(tokens[1], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[2], TokenType::IDENTIFIER_TOKEN, "i");
	expectToken(tokens[3], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[4], TokenType::INT_TOKEN, "int");
	expectToken(tokens[5], TokenType::ASSIGN_TOKEN, "=");
	expectToken(tokens[6], TokenType::INT_LITERAL_TOKEN, "0");
	expectToken(tokens[7], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[8], TokenType::IDENTIFIER_TOKEN, "i");
	expectToken(tokens[9], TokenType::LESS_TOKEN, "<");
	expectToken(tokens[10], TokenType::INT_LITERAL_TOKEN, "10");
	expectToken(tokens[11], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[12], TokenType::IDENTIFIER_TOKEN, "i");
	expectToken(tokens[13], TokenType::INCREMENT_TOKEN, "++");
}

TEST(LexerPrograms, MatchStatement) {
	auto tokens = lex(R"(
match(state) {
	"HIGH" => { }
	_ => { }
}
)");
	ASSERT_GE(tokens.size(), 12u);
	expectToken(tokens[0], TokenType::MATCH_TOKEN, "match");
	expectToken(tokens[1], TokenType::LEFT_PAREN_TOKEN, "(");
	expectToken(tokens[2], TokenType::IDENTIFIER_TOKEN, "state");
	expectToken(tokens[3], TokenType::RIGHT_PAREN_TOKEN, ")");
	expectToken(tokens[4], TokenType::LEFT_BRACE_TOKEN, "{");
	expectToken(tokens[5], TokenType::STRING_LITERAL_TOKEN, "HIGH");
	expectToken(tokens[6], TokenType::ARROW_TOKEN, "=>");
}

TEST(LexerPrograms, StructDecl) {
	auto tokens = lex(R"(
struct Person {
	name: string;
	age: int;
}
)");
	ASSERT_GE(tokens.size(), 10u);
	expectToken(tokens[0], TokenType::STRUCT_TOKEN, "struct");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "Person");
	expectToken(tokens[2], TokenType::LEFT_BRACE_TOKEN, "{");
	expectToken(tokens[3], TokenType::IDENTIFIER_TOKEN, "name");
	expectToken(tokens[4], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[5], TokenType::STRING_TOKEN, "string");
	expectToken(tokens[6], TokenType::SEMICOLON_TOKEN, ";");
	expectToken(tokens[7], TokenType::IDENTIFIER_TOKEN, "age");
	expectToken(tokens[8], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[9], TokenType::INT_TOKEN, "int");
	expectToken(tokens[10], TokenType::SEMICOLON_TOKEN, ";");
}

TEST(LexerPrograms, ArrayType) {
	auto tokens = lex("list: array<int> = [0, 1, 2];");
	ASSERT_GE(tokens.size(), 12u);
	expectToken(tokens[0], TokenType::IDENTIFIER_TOKEN, "list");
	expectToken(tokens[1], TokenType::COLON_TOKEN, ":");
	expectToken(tokens[2], TokenType::ARRAY_TOKEN, "array");
	expectToken(tokens[3], TokenType::LESS_TOKEN, "<");
	expectToken(tokens[4], TokenType::INT_TOKEN, "int");
	expectToken(tokens[5], TokenType::GREATER_TOKEN, ">");
	expectToken(tokens[6], TokenType::ASSIGN_TOKEN, "=");
	expectToken(tokens[7], TokenType::LEFT_BRACKET_TOKEN, "[");
	expectToken(tokens[8], TokenType::INT_LITERAL_TOKEN, "0");
	expectToken(tokens[9], TokenType::COMMA_TOKEN, ",");
	expectToken(tokens[10], TokenType::INT_LITERAL_TOKEN, "1");
	expectToken(tokens[11], TokenType::COMMA_TOKEN, ",");
	expectToken(tokens[12], TokenType::INT_LITERAL_TOKEN, "2");
	expectToken(tokens[13], TokenType::RIGHT_BRACKET_TOKEN, "]");
	expectToken(tokens[14], TokenType::SEMICOLON_TOKEN, ";");
}

TEST(LexerLineTracking, LineAndColumn) {
	auto tokens = lex("x\ny");
	ASSERT_EQ(tokens.size(), 3u);
	EXPECT_EQ(tokens[0].line, 1);
	EXPECT_EQ(tokens[0].column, 1);
	EXPECT_EQ(tokens[1].line, 2);
	EXPECT_EQ(tokens[1].column, 1);
}

// ============================================================
// Interpolated strings
// ============================================================

TEST(LexerInterpolation, SimpleInterpolation) {
	auto tokens = lex("\"Hello {name}!\"");
	ASSERT_GE(tokens.size(), 4u);
	expectToken(tokens[0], TokenType::INTERP_STRING_START_TOKEN, "Hello ");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "name");
	expectToken(tokens[2], TokenType::INTERP_STRING_END_TOKEN, "!");
}

TEST(LexerInterpolation, MultipleInterpolations) {
	auto tokens = lex("\"Hi {a} and {b} end\"");
	ASSERT_GE(tokens.size(), 6u);
	expectToken(tokens[0], TokenType::INTERP_STRING_START_TOKEN, "Hi ");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "a");
	expectToken(tokens[2], TokenType::INTERP_STRING_MID_TOKEN, " and ");
	expectToken(tokens[3], TokenType::IDENTIFIER_TOKEN, "b");
	expectToken(tokens[4], TokenType::INTERP_STRING_END_TOKEN, " end");
}

TEST(LexerInterpolation, EmptyFragments) {
	auto tokens = lex("\"{x}\"");
	ASSERT_GE(tokens.size(), 4u);
	expectToken(tokens[0], TokenType::INTERP_STRING_START_TOKEN, "");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "x");
	expectToken(tokens[2], TokenType::INTERP_STRING_END_TOKEN, "");
}

TEST(LexerInterpolation, ExpressionInterpolation) {
	auto tokens = lex("\"sum={a+b}\"");
	ASSERT_GE(tokens.size(), 6u);
	expectToken(tokens[0], TokenType::INTERP_STRING_START_TOKEN, "sum=");
	expectToken(tokens[1], TokenType::IDENTIFIER_TOKEN, "a");
	expectToken(tokens[2], TokenType::PLUS_TOKEN, "+");
	expectToken(tokens[3], TokenType::IDENTIFIER_TOKEN, "b");
	expectToken(tokens[4], TokenType::INTERP_STRING_END_TOKEN, "");
}

TEST(LexerInterpolation, NoInterpolation) {
	auto tokens = lex("\"plain string\"");
	ASSERT_GE(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::STRING_LITERAL_TOKEN, "plain string");
}

TEST(LexerInterpolation, EscapedBrace) {
	auto tokens = lex("\"curly \\{brace\\}\"");
	ASSERT_GE(tokens.size(), 2u);
	expectToken(tokens[0], TokenType::STRING_LITERAL_TOKEN, "curly {brace}");
}

TEST(LexerInterpolation, MemberAccessInInterpolation) {
	auto tokens = lex("\"name is {this.name}\"");
	ASSERT_GE(tokens.size(), 5u);
	expectToken(tokens[0], TokenType::INTERP_STRING_START_TOKEN, "name is ");
	expectToken(tokens[1], TokenType::THIS_TOKEN, "this");
	expectToken(tokens[2], TokenType::DOT_TOKEN, ".");
	expectToken(tokens[3], TokenType::IDENTIFIER_TOKEN, "name");
	expectToken(tokens[4], TokenType::INTERP_STRING_END_TOKEN, "");
}
	