#include "parser/parser.h"

#include <cassert>
#include <sstream>

// ==========================
// Token helpers
// ==========================

const Token& Parser::current() const {
	return tokens[pos];
}

const Token& Parser::peek() const {
	if (pos + 1 < tokens.size()) return tokens[pos + 1];
	return tokens.back();
}

const Token& Parser::advance() {
	const Token& tok = tokens[pos];
	if (pos < tokens.size() - 1) pos++;
	return tok;
}

bool Parser::check(TokenType type) const {
	return current().type == type;
}

bool Parser::match(TokenType type) {
	if (check(type)) {
		advance();
		return true;
	}
	return false;
}

const Token& Parser::expect(TokenType type, const std::string& msg) {
	if (check(type)) {
		return advance();
	}
	error(msg + " (got '" + current().value + "' [" + tokenTypeToString(current().type) + "])");
}

void Parser::error(const std::string& msg) const {
	throw ParseError(msg, current().line, current().column);
}

bool Parser::isAtEnd() const {
	return current().type == TokenType::EOF_TOKEN;
}

bool Parser::isAssignOp(TokenType type) const {
	return type == TokenType::ASSIGN_TOKEN ||
		   type == TokenType::PLUS_ASSIGN_TOKEN ||
		   type == TokenType::MINUS_ASSIGN_TOKEN ||
		   type == TokenType::MULTIPLY_ASSIGN_TOKEN ||
		   type == TokenType::DIVIDE_ASSIGN_TOKEN ||
		   type == TokenType::MODULO_ASSIGN_TOKEN;
}

// ==========================
// Program
// ==========================

Program Parser::parseProgram() {
	Program program;

	while (!isAtEnd()) {
		if (check(TokenType::STRUCT_TOKEN)) {
			program.structs.push_back(parseStructDecl());
		} else {
			program.functions.push_back(parseFunctionDecl());
		}
	}

	return program;
}

// ==========================
// Top-level declarations
// ==========================

// function_declaration = identifier "(" parameter_list? ")" ":" type block ;
std::unique_ptr<FunctionDecl> Parser::parseFunctionDecl() {
	auto fn = std::make_unique<FunctionDecl>();
	fn->line = current().line;
	fn->column = current().column;

	const Token& name = expect(TokenType::IDENTIFIER_TOKEN, "Expected function name");
	fn->name = name.value;

	expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after function name");

	// Parse parameters
	if (!check(TokenType::RIGHT_PAREN_TOKEN)) {
		do {
			Parameter param;
			const Token& pname = expect(TokenType::IDENTIFIER_TOKEN, "Expected parameter name");
			param.name = pname.value;
			expect(TokenType::COLON_TOKEN, "Expected ':' after parameter name");
			param.type = parseType();
			fn->params.push_back(std::move(param));
		} while (match(TokenType::COMMA_TOKEN));
	}

	expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after parameters");
	expect(TokenType::COLON_TOKEN, "Expected ':' before return type");
	fn->returnType = parseType();
	fn->body = parseBlock();

	return fn;
}

// struct_declaration = "struct" identifier "{" { struct_member } "}" ;
std::unique_ptr<StructDecl> Parser::parseStructDecl() {
	auto sd = std::make_unique<StructDecl>();
	sd->line = current().line;
	sd->column = current().column;

	expect(TokenType::STRUCT_TOKEN, "Expected 'struct'");
	const Token& name = expect(TokenType::IDENTIFIER_TOKEN, "Expected struct name");
	sd->name = name.value;
	expect(TokenType::LEFT_BRACE_TOKEN, "Expected '{' after struct name");

	while (!check(TokenType::RIGHT_BRACE_TOKEN) && !isAtEnd()) {
		// Peek ahead to see if this is a method (identifier "(" ) or a field (identifier ":")
		if (check(TokenType::IDENTIFIER_TOKEN) && peek().type == TokenType::LEFT_PAREN_TOKEN) {
			// Method
			StructMember m;
			m.kind = StructMember::METHOD;
			m.method = parseFunctionDecl();
			sd->members.push_back(std::move(m));
		} else {
			// Field: identifier ":" type ";"
			StructMember m;
			m.kind = StructMember::FIELD;
			const Token& fname = expect(TokenType::IDENTIFIER_TOKEN, "Expected field name");
			m.fieldName = fname.value;
			expect(TokenType::COLON_TOKEN, "Expected ':' after field name");
			m.fieldType = parseType();
			expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after field declaration");
			sd->members.push_back(std::move(m));
		}
	}

	expect(TokenType::RIGHT_BRACE_TOKEN, "Expected '}' after struct body");
	return sd;
}

// ==========================
// Types
// ==========================

std::unique_ptr<TypeNode> Parser::parseType() {
	auto typeNode = std::make_unique<TypeNode>();

	// Check for parameterized types: array<T>, slice<T>, map<K,V>
	if (check(TokenType::ARRAY_TOKEN) || check(TokenType::SLICE_TOKEN) || check(TokenType::MAP_TOKEN)) {
		typeNode->name = current().value;
		advance();
		expect(TokenType::LESS_TOKEN, "Expected '<' after container type");
		typeNode->typeParams.push_back(parseType());
		if (typeNode->name == "map") {
			expect(TokenType::COMMA_TOKEN, "Expected ',' in map type");
			typeNode->typeParams.push_back(parseType());
		}
		expect(TokenType::GREATER_TOKEN, "Expected '>' after type parameter");
	} else if (check(TokenType::INT_TOKEN) || check(TokenType::FLOAT_TOKEN) ||
			   check(TokenType::BOOL_TOKEN) || check(TokenType::CHAR_TOKEN) ||
			   check(TokenType::STRING_TOKEN) || check(TokenType::VOID_TOKEN)) {
		typeNode->name = current().value;
		advance();
	} else if (check(TokenType::IDENTIFIER_TOKEN)) {
		typeNode->name = current().value;
		advance();
	} else {
		error("Expected type");
	}

	// Optional type: type?
	// We don't have a QUESTION_TOKEN, so skip for now

	return typeNode;
}

// ==========================
// Blocks & Statements
// ==========================

Block Parser::parseBlock() {
	Block block;
	expect(TokenType::LEFT_BRACE_TOKEN, "Expected '{'");

	while (!check(TokenType::RIGHT_BRACE_TOKEN) && !isAtEnd()) {
		block.statements.push_back(parseStatement());
	}

	expect(TokenType::RIGHT_BRACE_TOKEN, "Expected '}'");
	return block;
}

StmtPtr Parser::parseStatement() {
	if (check(TokenType::IF_TOKEN)) return parseIfStmt();
	if (check(TokenType::WHILE_TOKEN)) return parseWhileStmt();
	if (check(TokenType::FOR_TOKEN)) return parseForStmt();
	if (check(TokenType::MATCH_TOKEN)) return parseMatchStmt();
	if (check(TokenType::RETURN_TOKEN)) return parseReturnStmt();
	if (check(TokenType::BREAK_TOKEN)) {
		auto stmt = std::make_unique<BreakStmt>();
		stmt->line = current().line;
		stmt->column = current().column;
		advance();
		expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after 'break'");
		return stmt;
	}
	if (check(TokenType::CONTINUE_TOKEN)) {
		auto stmt = std::make_unique<ContinueStmt>();
		stmt->line = current().line;
		stmt->column = current().column;
		advance();
		expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after 'continue'");
		return stmt;
	}

	return parseVarDeclOrExprStmt();
}

// Parse either a variable declaration or an expression statement.
// Variable declaration forms:
//   identifier ":" type [ "=" expression ] ";"
//   identifier ":=" expression ";"
// Otherwise it's an expression (possibly with assignment) followed by ";"
StmtPtr Parser::parseVarDeclOrExprStmt() {
	// Check for variable declaration: identifier ":" or identifier ":="
	if (check(TokenType::IDENTIFIER_TOKEN)) {
		// Look ahead for ":" or ":="
		if (peek().type == TokenType::COLON_TOKEN || peek().type == TokenType::WALRUS_TOKEN) {
			auto decl = std::make_unique<VarDeclStmt>();
			decl->line = current().line;
			decl->column = current().column;
			decl->name = current().value;
			advance(); // consume identifier

			if (match(TokenType::WALRUS_TOKEN)) {
				// x := expr;
				decl->isWalrus = true;
				decl->initializer = parseExpression();
			} else {
				// x: type [= expr];
				expect(TokenType::COLON_TOKEN, "Expected ':'");
				decl->type = parseType();
				if (match(TokenType::ASSIGN_TOKEN)) {
					decl->initializer = parseExpression();
				}
			}

			expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after variable declaration");
			return decl;
		}
	}

	// Expression statement (which may include assignment like x = 5; or x += 1; or x++; or fn();)
	auto expr = parseExpression();
	int stmtLine = expr->line;
	int stmtCol = expr->column;

	// Check for assignment operators
	if (isAssignOp(current().type)) {
		std::string op = current().value;
		advance();
		auto value = parseExpression();
		expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after assignment");
		auto stmt = std::make_unique<AssignStmt>(op, std::move(expr), std::move(value));
		stmt->line = stmtLine;
		stmt->column = stmtCol;
		return stmt;
	}

	expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after expression");
	auto stmt = std::make_unique<ExprStmt>(std::move(expr));
	stmt->line = stmtLine;
	stmt->column = stmtCol;
	return stmt;
}

// ==========================
// Control Flow
// ==========================

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
	auto stmt = std::make_unique<IfStmt>();
	stmt->line = current().line;
	stmt->column = current().column;

	expect(TokenType::IF_TOKEN, "Expected 'if'");
	expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after 'if'");
	stmt->condition = parseExpression();
	expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after if condition");
	stmt->thenBlock = parseBlock();

	while (check(TokenType::ELSEIF_TOKEN)) {
		advance();
		expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after 'elseif'");
		auto cond = parseExpression();
		expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after elseif condition");
		auto block = parseBlock();
		stmt->elseIfBlocks.push_back({std::move(cond), std::move(block)});
	}

	if (match(TokenType::ELSE_TOKEN)) {
		stmt->elseBlock = std::make_unique<Block>(parseBlock());
	}

	return stmt;
}

std::unique_ptr<WhileStmt> Parser::parseWhileStmt() {
	auto stmt = std::make_unique<WhileStmt>();
	stmt->line = current().line;
	stmt->column = current().column;

	expect(TokenType::WHILE_TOKEN, "Expected 'while'");
	expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after 'while'");
	stmt->condition = parseExpression();
	expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after while condition");
	stmt->body = parseBlock();

	return stmt;
}

StmtPtr Parser::parseForStmt() {
	int startLine = current().line;
	int startCol = current().column;

	expect(TokenType::FOR_TOKEN, "Expected 'for'");
	expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after 'for'");

	// Check for for-in: identifier ":" type "in" "[" expr ".." expr "]"
	// Lookahead: IDENTIFIER COLON TYPE IN
	if (check(TokenType::IDENTIFIER_TOKEN)) {
		size_t saved = pos;
		std::string varName = current().value;
		advance();
		if (check(TokenType::COLON_TOKEN)) {
			advance();
			// Try to parse type, then check for "in"
			auto varType = parseType();
			if (check(TokenType::IN_TOKEN)) {
				advance();
				expect(TokenType::LEFT_BRACKET_TOKEN, "Expected '[' after 'in'");
				auto rangeStart = parseExpression();
				expect(TokenType::DOTDOT_TOKEN, "Expected '..' in range");
				auto rangeEnd = parseExpression();
				expect(TokenType::RIGHT_BRACKET_TOKEN, "Expected ']' after range");
				expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after for-in header");

				auto stmt = std::make_unique<ForInStmt>();
				stmt->line = startLine;
				stmt->column = startCol;
				stmt->varName = varName;
				stmt->varType = std::move(varType);
				stmt->rangeStart = std::move(rangeStart);
				stmt->rangeEnd = std::move(rangeEnd);
				stmt->body = parseBlock();
				return stmt;
			}
			// Not a for-in, backtrack
			pos = saved;
		} else {
			pos = saved;
		}
	}

	// C-style for loop: for (init; cond; update) { ... }
	auto stmt = std::make_unique<ForStmt>();
	stmt->line = startLine;
	stmt->column = startCol;

	// Init (variable declaration or expression, or empty)
	if (!check(TokenType::SEMICOLON_TOKEN)) {
		stmt->init = parseVarDeclOrExprStmt();
		// parseVarDeclOrExprStmt already consumed the semicolon
	} else {
		advance(); // consume ;
	}

	// Condition
	if (!check(TokenType::SEMICOLON_TOKEN)) {
		stmt->condition = parseExpression();
	}
	expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after for condition");

	// Update expression
	if (!check(TokenType::RIGHT_PAREN_TOKEN)) {
		stmt->update = parseExpression();
	}
	expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after for header");

	stmt->body = parseBlock();
	return stmt;
}

std::unique_ptr<MatchStmt> Parser::parseMatchStmt() {
	auto stmt = std::make_unique<MatchStmt>();
	stmt->line = current().line;
	stmt->column = current().column;

	expect(TokenType::MATCH_TOKEN, "Expected 'match'");
	expect(TokenType::LEFT_PAREN_TOKEN, "Expected '(' after 'match'");
	stmt->subject = parseExpression();
	expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after match subject");
	expect(TokenType::LEFT_BRACE_TOKEN, "Expected '{' after match header");

	while (!check(TokenType::RIGHT_BRACE_TOKEN) && !isAtEnd()) {
		MatchCase mc;

		if (check(TokenType::UNDERSCORE_TOKEN)) {
			// Default case
			mc.isDefault = true;
			advance();
		} else {
			// Pattern: expr { "|" expr }
			// Use parseBitwiseXor to avoid consuming | as bitwise OR in the expression
			mc.patterns.push_back(parseBitwiseXor());
			while (match(TokenType::BIT_OR_TOKEN)) {
				mc.patterns.push_back(parseBitwiseXor());
			}
		}

		expect(TokenType::ARROW_TOKEN, "Expected '=>' after match pattern");
		mc.body = parseBlock();
		stmt->cases.push_back(std::move(mc));
	}

	expect(TokenType::RIGHT_BRACE_TOKEN, "Expected '}' after match body");
	return stmt;
}

std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
	auto stmt = std::make_unique<ReturnStmt>();
	stmt->line = current().line;
	stmt->column = current().column;
	advance(); // consume 'return'

	if (!check(TokenType::SEMICOLON_TOKEN)) {
		stmt->value = parseExpression();
	}

	expect(TokenType::SEMICOLON_TOKEN, "Expected ';' after return statement");
	return stmt;
}

// ==========================
// Expressions
// ==========================

ExprPtr Parser::parseExpression() {
	return parseLogicalOr();
}

ExprPtr Parser::parseLogicalOr() {
	auto left = parseLogicalXor();
	while (check(TokenType::OR_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseLogicalXor();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseLogicalXor() {
	auto left = parseLogicalAnd();
	while (check(TokenType::XOR_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseLogicalAnd();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseLogicalAnd() {
	auto left = parseBitwiseOr();
	while (check(TokenType::AND_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseBitwiseOr();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseBitwiseOr() {
	auto left = parseBitwiseXor();
	while (check(TokenType::BIT_OR_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseBitwiseXor();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseBitwiseXor() {
	auto left = parseBitwiseAnd();
	while (check(TokenType::BIT_XOR_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseBitwiseAnd();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseBitwiseAnd() {
	auto left = parseEquality();
	while (check(TokenType::BIT_AND_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseEquality();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseEquality() {
	auto left = parseRelational();
	while (check(TokenType::EQUAL_TOKEN) || check(TokenType::NOT_EQUAL_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseRelational();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseRelational() {
	auto left = parseShift();
	while (check(TokenType::LESS_TOKEN) || check(TokenType::GREATER_TOKEN) ||
		   check(TokenType::LESS_EQUAL_TOKEN) || check(TokenType::GREATER_EQUAL_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseShift();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseShift() {
	auto left = parseAdditive();
	while (check(TokenType::LEFT_SHIFT_TOKEN) || check(TokenType::RIGHT_SHIFT_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseAdditive();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseAdditive() {
	auto left = parseMultiplicative();
	while (check(TokenType::PLUS_TOKEN) || check(TokenType::MINUS_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseMultiplicative();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseMultiplicative() {
	auto left = parseUnary();
	while (check(TokenType::MULTIPLY_TOKEN) || check(TokenType::DIVIDE_TOKEN) || check(TokenType::MODULO_TOKEN)) {
		std::string op = current().value;
		advance();
		auto right = parseUnary();
		left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
	}
	return left;
}

ExprPtr Parser::parseUnary() {
	if (check(TokenType::MINUS_TOKEN) || check(TokenType::NOT_TOKEN) || check(TokenType::BIT_NOT_TOKEN)) {
		std::string op = current().value;
		int opLine = current().line;
		int opCol = current().column;
		advance();
		auto operand = parseUnary();
		auto expr = std::make_unique<UnaryExpr>(op, std::move(operand), true);
		expr->line = opLine;
		expr->column = opCol;
		return expr;
	}
	return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
	auto expr = parsePrimary();

	while (true) {
		if (check(TokenType::LEFT_PAREN_TOKEN)) {
			// Function call
			advance();
			auto args = parseArgumentList();
			expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after arguments");
			expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
		} else if (check(TokenType::LEFT_BRACKET_TOKEN)) {
			// Index access
			advance();
			auto index = parseExpression();
			expect(TokenType::RIGHT_BRACKET_TOKEN, "Expected ']' after index");
			expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
		} else if (check(TokenType::DOT_TOKEN)) {
			// Member access
			advance();
			const Token& member = expect(TokenType::IDENTIFIER_TOKEN, "Expected member name after '.'");
			expr = std::make_unique<MemberAccessExpr>(std::move(expr), member.value);
		} else if (check(TokenType::INCREMENT_TOKEN)) {
			// Post-increment
			std::string op = current().value;
			advance();
			expr = std::make_unique<UnaryExpr>(op, std::move(expr), false);
		} else if (check(TokenType::DECREMENT_TOKEN)) {
			// Post-decrement
			std::string op = current().value;
			advance();
			expr = std::make_unique<UnaryExpr>(op, std::move(expr), false);
		} else {
			break;
		}
	}

	return expr;
}

ExprPtr Parser::parsePrimary() {
	int startLine = current().line;
	int startCol = current().column;

	// Integer literal
	if (check(TokenType::INT_LITERAL_TOKEN)) {
		const Token& tok = advance();
		int64_t val = 0;
		if (tok.value.size() > 2 && tok.value[0] == '0' && tok.value[1] == 'x') {
			val = std::stoll(tok.value, nullptr, 16);
		} else if (tok.value.size() > 2 && tok.value[0] == '0' && tok.value[1] == 'b') {
			val = std::stoll(tok.value.substr(2), nullptr, 2);
		} else {
			val = std::stoll(tok.value);
		}
		auto expr = std::make_unique<IntLiteralExpr>(val, tok.value);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Float literal
	if (check(TokenType::FLOAT_LITERAL_TOKEN)) {
		const Token& tok = advance();
		double val = std::stod(tok.value);
		auto expr = std::make_unique<FloatLiteralExpr>(val, tok.value);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// String literal
	if (check(TokenType::STRING_LITERAL_TOKEN)) {
		const Token& tok = advance();
		auto expr = std::make_unique<StringLiteralExpr>(tok.value);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Char literal
	if (check(TokenType::CHAR_LITERAL_TOKEN)) {
		const Token& tok = advance();
		auto expr = std::make_unique<CharLiteralExpr>(tok.value);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Boolean literals
	if (check(TokenType::TRUE_TOKEN)) {
		advance();
		auto expr = std::make_unique<BoolLiteralExpr>(true);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}
	if (check(TokenType::FALSE_TOKEN)) {
		advance();
		auto expr = std::make_unique<BoolLiteralExpr>(false);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// this
	if (check(TokenType::THIS_TOKEN)) {
		advance();
		auto expr = std::make_unique<ThisExpr>();
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Identifier
	if (check(TokenType::IDENTIFIER_TOKEN)) {
		const Token& tok = advance();
		auto expr = std::make_unique<IdentifierExpr>(tok.value);
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Parenthesized expression
	if (check(TokenType::LEFT_PAREN_TOKEN)) {
		advance();
		auto expr = parseExpression();
		expect(TokenType::RIGHT_PAREN_TOKEN, "Expected ')' after expression");
		return expr;
	}

	// Array literal: [expr, expr, ...]
	if (check(TokenType::LEFT_BRACKET_TOKEN)) {
		advance();
		std::vector<ExprPtr> elements;
		if (!check(TokenType::RIGHT_BRACKET_TOKEN)) {
			elements.push_back(parseExpression());
			while (match(TokenType::COMMA_TOKEN)) {
				elements.push_back(parseExpression());
			}
		}
		expect(TokenType::RIGHT_BRACKET_TOKEN, "Expected ']' after array literal");
		auto expr = std::make_unique<ArrayLiteralExpr>(std::move(elements));
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	// Struct init literal: { field: expr, field: expr }
	if (check(TokenType::LEFT_BRACE_TOKEN)) {
		advance();
		std::vector<std::pair<std::string, ExprPtr>> fields;
		if (!check(TokenType::RIGHT_BRACE_TOKEN)) {
			do {
				const Token& fname = expect(TokenType::IDENTIFIER_TOKEN, "Expected field name in struct literal");
				expect(TokenType::COLON_TOKEN, "Expected ':' after field name");
				auto value = parseExpression();
				fields.push_back({fname.value, std::move(value)});
			} while (match(TokenType::COMMA_TOKEN));
		}
		expect(TokenType::RIGHT_BRACE_TOKEN, "Expected '}' after struct literal");
		auto expr = std::make_unique<StructInitExpr>(std::move(fields));
		expr->line = startLine;
		expr->column = startCol;
		return expr;
	}

	error("Unexpected token: '" + current().value + "'");
}

std::vector<ExprPtr> Parser::parseArgumentList() {
	std::vector<ExprPtr> args;
	if (!check(TokenType::RIGHT_PAREN_TOKEN)) {
		args.push_back(parseExpression());
		while (match(TokenType::COMMA_TOKEN)) {
			args.push_back(parseExpression());
		}
	}
	return args;
}
