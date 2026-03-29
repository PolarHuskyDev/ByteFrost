#pragma once

#include <stdexcept>
#include <vector>

#include "ast.h"
#include "lexer.h"

class Parser {
   private:
	std::vector<Token> tokens;
	size_t pos;

   public:
	explicit Parser(Lexer& lexer) : pos(0) {
		Token tok;
		do {
			tok = lexer.nextToken();
			tokens.push_back(tok);
		} while (tok.type != TOKEN_EOF);
	}

	Program parse() {
		Program program;
		while (!isAtEnd()) {
			program.functions.push_back(parseFunctionDecl());
		}
		return program;
	}

   private:
	// ---- Token helpers ----

	const Token& current() const { return tokens[pos]; }

	const Token& peek(size_t offset) const {
		size_t idx = pos + offset;
		if (idx >= tokens.size()) return tokens.back();
		return tokens[idx];
	}

	bool isAtEnd() const { return current().type == TOKEN_EOF; }

	bool check(TokenType type) const { return current().type == type; }

	Token advance() {
		Token tok = current();
		if (!isAtEnd()) pos++;
		return tok;
	}

	bool match(TokenType type) {
		if (check(type)) {
			advance();
			return true;
		}
		return false;
	}

	Token expect(TokenType type, const std::string& message) {
		if (!check(type)) {
			error(message);
		}
		return advance();
	}

	[[noreturn]] void error(const std::string& message) const {
		const Token& tok = current();
		throw std::runtime_error(
			"Parse error at line " + std::to_string(tok.line) +
			", column " + std::to_string(tok.column) +
			": " + message + " (got '" + tok.value + "')");
	}

	// ---- Type parsing ----

	bool isTypeToken(TokenType type) const {
		return type == TOKEN_TYPE_INT || type == TOKEN_TYPE_STRING ||
			   type == TOKEN_TYPE_VOID || type == TOKEN_TYPE_BOOL ||
			   type == TOKEN_TYPE_FLOAT;
	}

	std::string parseType() {
		if (!isTypeToken(current().type)) {
			error("expected type name");
		}
		return advance().value;
	}

	// ---- Declarations ----

	FunctionDecl parseFunctionDecl() {
		FunctionDecl func;
		func.line = current().line;
		func.column = current().column;
		func.name = expect(TOKEN_IDENTIFIER, "expected function name").value;

		expect(TOKEN_LPAREN, "expected '(' after function name");

		if (!check(TOKEN_RPAREN)) {
			func.parameters = parseParamList();
		}

		expect(TOKEN_RPAREN, "expected ')' after parameters");
		expect(TOKEN_COLON, "expected ':' after ')'");
		func.returnType = parseType();
		func.body = parseBlock();

		return func;
	}

	std::vector<Parameter> parseParamList() {
		std::vector<Parameter> params;
		params.push_back(parseParam());
		while (match(TOKEN_COMMA)) {
			params.push_back(parseParam());
		}
		return params;
	}

	Parameter parseParam() {
		std::string name = expect(TOKEN_IDENTIFIER, "expected parameter name").value;
		expect(TOKEN_COLON, "expected ':' after parameter name");
		std::string type = parseType();
		return {name, type};
	}

	// ---- Statements ----

	std::unique_ptr<Block> parseBlock() {
		int line = current().line;
		int col = current().column;
		expect(TOKEN_LBRACE, "expected '{'");

		std::vector<StmtPtr> stmts;
		while (!check(TOKEN_RBRACE) && !isAtEnd()) {
			stmts.push_back(parseStatement());
		}

		expect(TOKEN_RBRACE, "expected '}'");
		return std::make_unique<Block>(std::move(stmts), line, col);
	}

	StmtPtr parseStatement() {
		if (check(TOKEN_IF)) return parseIfStmt();
		if (check(TOKEN_RETURN)) return parseReturnStmt();

		// Variable declaration: IDENTIFIER ':' ...
		if (check(TOKEN_IDENTIFIER) && peek(1).type == TOKEN_COLON) {
			return parseVarDecl();
		}

		return parseExprStmt();
	}

	StmtPtr parseVarDecl() {
		int line = current().line;
		int col = current().column;
		std::string name = advance().value;  // consume IDENTIFIER
		advance();                           // consume ':'
		std::string typeName = parseType();
		expect(TOKEN_ASSIGN, "expected '=' in variable declaration");
		ExprPtr init = parseExpression();
		expect(TOKEN_SEMICOLON, "expected ';' after variable declaration");
		return std::make_unique<VarDecl>(
			std::move(name), std::move(typeName), std::move(init), line, col);
	}

	StmtPtr parseIfStmt() {
		int line = current().line;
		int col = current().column;
		advance();  // consume 'if'

		ExprPtr condition = parseExpression();
		auto thenBlock = parseBlock();

		std::unique_ptr<Block> elseBlock = nullptr;
		if (match(TOKEN_ELSE)) {
			elseBlock = parseBlock();
		}

		return std::make_unique<IfStmt>(
			std::move(condition), std::move(thenBlock),
			std::move(elseBlock), line, col);
	}

	StmtPtr parseReturnStmt() {
		int line = current().line;
		int col = current().column;
		advance();  // consume 'return'

		ExprPtr value = nullptr;
		if (!check(TOKEN_SEMICOLON)) {
			value = parseExpression();
		}

		expect(TOKEN_SEMICOLON, "expected ';' after return statement");
		return std::make_unique<ReturnStmt>(std::move(value), line, col);
	}

	StmtPtr parseExprStmt() {
		int line = current().line;
		int col = current().column;
		ExprPtr expr = parseExpression();
		expect(TOKEN_SEMICOLON, "expected ';' after expression");
		return std::make_unique<ExprStmt>(std::move(expr), line, col);
	}

	// ---- Expressions (precedence climbing) ----
	//
	// Precedence (low → high):
	//   comparison:     ==  !=  <  >  <=  >=
	//   addition:       +  -
	//   multiplication: *  /
	//   unary:          -expr
	//   call:           f(args)
	//   primary:        number, string, identifier, (expr)

	ExprPtr parseExpression() { return parseComparison(); }

	ExprPtr parseComparison() {
		ExprPtr left = parseAddition();

		while (check(TOKEN_LT) || check(TOKEN_GT) || check(TOKEN_LE) ||
			   check(TOKEN_GE) || check(TOKEN_EQ) || check(TOKEN_NEQ)) {
			Token op = advance();
			ExprPtr right = parseAddition();
			left = std::make_unique<BinaryExpr>(
				std::move(left), op.value, std::move(right), op.line, op.column);
		}

		return left;
	}

	ExprPtr parseAddition() {
		ExprPtr left = parseMultiplication();

		while (check(TOKEN_PLUS) || check(TOKEN_MINUS)) {
			Token op = advance();
			ExprPtr right = parseMultiplication();
			left = std::make_unique<BinaryExpr>(
				std::move(left), op.value, std::move(right), op.line, op.column);
		}

		return left;
	}

	ExprPtr parseMultiplication() {
		ExprPtr left = parseUnary();

		while (check(TOKEN_MULTIPLY) || check(TOKEN_DIVIDE)) {
			Token op = advance();
			ExprPtr right = parseUnary();
			left = std::make_unique<BinaryExpr>(
				std::move(left), op.value, std::move(right), op.line, op.column);
		}

		return left;
	}

	ExprPtr parseUnary() {
		if (check(TOKEN_MINUS)) {
			Token op = advance();
			ExprPtr operand = parseUnary();
			return std::make_unique<UnaryExpr>(
				op.value, std::move(operand), op.line, op.column);
		}
		return parseCall();
	}

	ExprPtr parseCall() {
		ExprPtr expr = parsePrimary();

		if (check(TOKEN_LPAREN) && expr->kind == ExprKind::Identifier) {
			advance();  // consume '('
			auto* ident = static_cast<Identifier*>(expr.get());
			std::string callee = ident->name;
			int callLine = expr->line;
			int callCol = expr->column;

			std::vector<ExprPtr> args;
			if (!check(TOKEN_RPAREN)) {
				args.push_back(parseExpression());
				while (match(TOKEN_COMMA)) {
					args.push_back(parseExpression());
				}
			}

			expect(TOKEN_RPAREN, "expected ')' after arguments");
			return std::make_unique<CallExpr>(
				std::move(callee), std::move(args), callLine, callCol);
		}

		return expr;
	}

	ExprPtr parsePrimary() {
		if (check(TOKEN_NUMBER)) {
			Token tok = advance();
			return std::make_unique<NumberLiteral>(tok.value, tok.line, tok.column);
		}

		if (check(TOKEN_STRING)) {
			Token tok = advance();
			return std::make_unique<StringLiteral>(tok.value, tok.line, tok.column);
		}

		if (check(TOKEN_IDENTIFIER)) {
			Token tok = advance();
			return std::make_unique<Identifier>(tok.value, tok.line, tok.column);
		}

		if (check(TOKEN_LPAREN)) {
			advance();  // consume '('
			ExprPtr expr = parseExpression();
			expect(TOKEN_RPAREN, "expected ')'");
			return expr;
		}

		error("expected expression");
	}
};
