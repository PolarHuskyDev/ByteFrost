#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct Expr;
struct Stmt;
struct Block;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ============================================================
// Expressions
// ============================================================

enum class ExprKind {
	NumberLiteral,
	StringLiteral,
	Identifier,
	UnaryExpr,
	BinaryExpr,
	CallExpr,
};

struct Expr {
	ExprKind kind;
	int line;
	int column;

	Expr(ExprKind kind, int line, int column)
		: kind(kind), line(line), column(column) {}
	virtual ~Expr() = default;
};

struct NumberLiteral : Expr {
	std::string value;

	NumberLiteral(std::string value, int line, int col)
		: Expr(ExprKind::NumberLiteral, line, col), value(std::move(value)) {}
};

struct StringLiteral : Expr {
	std::string value;

	StringLiteral(std::string value, int line, int col)
		: Expr(ExprKind::StringLiteral, line, col), value(std::move(value)) {}
};

struct Identifier : Expr {
	std::string name;

	Identifier(std::string name, int line, int col)
		: Expr(ExprKind::Identifier, line, col), name(std::move(name)) {}
};

struct UnaryExpr : Expr {
	std::string op;
	ExprPtr operand;

	UnaryExpr(std::string op, ExprPtr operand, int line, int col)
		: Expr(ExprKind::UnaryExpr, line, col),
		  op(std::move(op)),
		  operand(std::move(operand)) {}
};

struct BinaryExpr : Expr {
	ExprPtr left;
	std::string op;
	ExprPtr right;

	BinaryExpr(ExprPtr left, std::string op, ExprPtr right, int line, int col)
		: Expr(ExprKind::BinaryExpr, line, col),
		  left(std::move(left)),
		  op(std::move(op)),
		  right(std::move(right)) {}
};

struct CallExpr : Expr {
	std::string callee;
	std::vector<ExprPtr> arguments;

	CallExpr(std::string callee, std::vector<ExprPtr> arguments, int line, int col)
		: Expr(ExprKind::CallExpr, line, col),
		  callee(std::move(callee)),
		  arguments(std::move(arguments)) {}
};

// ============================================================
// Statements
// ============================================================

enum class StmtKind {
	Block,
	VarDecl,
	IfStmt,
	ReturnStmt,
	ExprStmt,
};

struct Stmt {
	StmtKind kind;
	int line;
	int column;

	Stmt(StmtKind kind, int line, int column)
		: kind(kind), line(line), column(column) {}
	virtual ~Stmt() = default;
};

struct Block : Stmt {
	std::vector<StmtPtr> statements;

	Block(std::vector<StmtPtr> statements, int line, int col)
		: Stmt(StmtKind::Block, line, col), statements(std::move(statements)) {}
};

struct VarDecl : Stmt {
	std::string name;
	std::string typeName;
	ExprPtr initializer;

	VarDecl(std::string name, std::string typeName, ExprPtr initializer, int line, int col)
		: Stmt(StmtKind::VarDecl, line, col),
		  name(std::move(name)),
		  typeName(std::move(typeName)),
		  initializer(std::move(initializer)) {}
};

struct IfStmt : Stmt {
	ExprPtr condition;
	std::unique_ptr<Block> thenBlock;
	std::unique_ptr<Block> elseBlock; // may be null

	IfStmt(ExprPtr condition, std::unique_ptr<Block> thenBlock,
		   std::unique_ptr<Block> elseBlock, int line, int col)
		: Stmt(StmtKind::IfStmt, line, col),
		  condition(std::move(condition)),
		  thenBlock(std::move(thenBlock)),
		  elseBlock(std::move(elseBlock)) {}
};

struct ReturnStmt : Stmt {
	ExprPtr value; // may be null for bare `return;`

	ReturnStmt(ExprPtr value, int line, int col)
		: Stmt(StmtKind::ReturnStmt, line, col), value(std::move(value)) {}
};

struct ExprStmt : Stmt {
	ExprPtr expression;

	ExprStmt(ExprPtr expression, int line, int col)
		: Stmt(StmtKind::ExprStmt, line, col), expression(std::move(expression)) {}
};

// ============================================================
// Top-level declarations
// ============================================================

struct Parameter {
	std::string name;
	std::string typeName;
};

struct FunctionDecl {
	std::string name;
	std::vector<Parameter> parameters;
	std::string returnType;
	std::unique_ptr<Block> body;
	int line;
	int column;
};

struct Program {
	std::vector<FunctionDecl> functions;
};
