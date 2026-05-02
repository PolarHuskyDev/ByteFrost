#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "ast.h"
#include "tokenizer/tokens.h"

class ParseError : public std::runtime_error {
   public:
	int line;
	int column;
	ParseError(const std::string& msg, int line, int column) : std::runtime_error(msg), line(line), column(column) {
	}
};

class Parser {
   public:
	Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {
	}

	Program parseProgram();

   private:
	// Top-level
	std::unique_ptr<ImportDecl> parseImportDecl();
	std::unique_ptr<FunctionDecl> parseFunctionDecl();
	std::unique_ptr<StructDecl> parseStructDecl();

	// Types
	std::unique_ptr<TypeNode> parseType();

	// Statements
	StmtPtr parseStatement();
	Block parseBlock();
	StmtPtr parseVarDeclOrExprStmt();
	std::unique_ptr<IfStmt> parseIfStmt();
	std::unique_ptr<WhileStmt> parseWhileStmt();
	StmtPtr parseForStmt();
	std::unique_ptr<MatchStmt> parseMatchStmt();
	std::unique_ptr<ReturnStmt> parseReturnStmt();

	// Expressions (precedence climbing)
	ExprPtr parseExpression();
	ExprPtr parseLogicalOr();
	ExprPtr parseLogicalXor();
	ExprPtr parseLogicalAnd();
	ExprPtr parseBitwiseOr();
	ExprPtr parseBitwiseXor();
	ExprPtr parseBitwiseAnd();
	ExprPtr parseEquality();
	ExprPtr parseRelational();
	ExprPtr parseShift();
	ExprPtr parseAdditive();
	ExprPtr parseMultiplicative();
	ExprPtr parseUnary();
	ExprPtr parsePostfix();
	ExprPtr parsePrimary();
	ExprPtr parseInterpolatedString();
	std::vector<ExprPtr> parseArgumentList();

	// Token helpers
	const Token& current() const;
	const Token& peek() const;
	const Token& advance();
	bool check(TokenType type) const;
	bool match(TokenType type);
	const Token& expect(TokenType type, const std::string& msg);
	[[noreturn]] void error(const std::string& msg) const;

	bool isAssignOp(TokenType type) const;
	bool isAtEnd() const;

	std::vector<Token> tokens;
	size_t pos;
};
