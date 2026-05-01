#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct ASTNode;
struct Expression;
struct Statement;

// Type representation
struct TypeNode {
	std::string name;  // "int", "float", "bool", "char", "string", "void", or user-defined
	std::vector<std::unique_ptr<TypeNode>> typeParams;  // for array<T>, map<K,V>, slice<T>

	TypeNode() = default;
	TypeNode(const std::string& name) : name(name) {}
	virtual ~TypeNode() = default;
};

// ========================
// Expressions
// ========================

struct Expression {
	int line = 0;
	int column = 0;
	virtual ~Expression() = default;
};

using ExprPtr = std::unique_ptr<Expression>;

struct IntLiteralExpr : Expression {
	int64_t value;
	std::string raw;  // original text (e.g., "0xFF")
	IntLiteralExpr(int64_t value, const std::string& raw) : value(value), raw(raw) {}
};

struct FloatLiteralExpr : Expression {
	double value;
	std::string raw;
	FloatLiteralExpr(double value, const std::string& raw) : value(value), raw(raw) {}
};

struct StringLiteralExpr : Expression {
	std::string value;
	StringLiteralExpr(const std::string& value) : value(value) {}
};

struct CharLiteralExpr : Expression {
	std::string value;
	CharLiteralExpr(const std::string& value) : value(value) {}
};

struct BoolLiteralExpr : Expression {
	bool value;
	BoolLiteralExpr(bool value) : value(value) {}
};

struct NullLiteralExpr : Expression {};

struct IdentifierExpr : Expression {
	std::string name;
	IdentifierExpr(const std::string& name) : name(name) {}
};

struct ThisExpr : Expression {};

struct BinaryExpr : Expression {
	std::string op;
	ExprPtr left;
	ExprPtr right;
	BinaryExpr(const std::string& op, ExprPtr left, ExprPtr right)
		: op(op), left(std::move(left)), right(std::move(right)) {}
};

struct UnaryExpr : Expression {
	std::string op;
	ExprPtr operand;
	bool prefix;  // true for prefix (-, !, ~), false for postfix (++, --)
	UnaryExpr(const std::string& op, ExprPtr operand, bool prefix)
		: op(op), operand(std::move(operand)), prefix(prefix) {}
};

struct CallExpr : Expression {
	ExprPtr callee;
	std::vector<ExprPtr> arguments;
	CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
		: callee(std::move(callee)), arguments(std::move(args)) {}
};

struct IndexExpr : Expression {
	ExprPtr object;
	ExprPtr index;
	IndexExpr(ExprPtr object, ExprPtr index)
		: object(std::move(object)), index(std::move(index)) {}
};

struct MemberAccessExpr : Expression {
	ExprPtr object;
	std::string member;
	MemberAccessExpr(ExprPtr object, const std::string& member)
		: object(std::move(object)), member(member) {}
};

struct AssignExpr : Expression {
	std::string op;  // =, +=, -=, *=, /=, %=
	ExprPtr target;
	ExprPtr value;
	AssignExpr(const std::string& op, ExprPtr target, ExprPtr value)
		: op(op), target(std::move(target)), value(std::move(value)) {}
};

// Array literal: [1, 2, 3]
struct ArrayLiteralExpr : Expression {
	std::vector<ExprPtr> elements;
	ArrayLiteralExpr(std::vector<ExprPtr> elements) : elements(std::move(elements)) {}
};

// Struct init literal: { name: "Peter", age: 21 }
struct StructInitExpr : Expression {
	std::vector<std::pair<std::string, ExprPtr>> fields;
	StructInitExpr(std::vector<std::pair<std::string, ExprPtr>> fields) : fields(std::move(fields)) {}
};

// String interpolation: "Hello {name}, age {age}"
struct InterpolatedStringExpr : Expression {
	std::vector<std::string> fragments;   // N+1 string fragments
	std::vector<ExprPtr> expressions;     // N embedded expressions
	InterpolatedStringExpr(std::vector<std::string> fragments, std::vector<ExprPtr> expressions)
		: fragments(std::move(fragments)), expressions(std::move(expressions)) {}
};

// ========================
// Statements
// ========================

struct Statement {
	int line = 0;
	int column = 0;
	virtual ~Statement() = default;
};

using StmtPtr = std::unique_ptr<Statement>;

struct Block {
	std::vector<StmtPtr> statements;
};

// Variable declaration: x: int = 5; or x := 5;
struct VarDeclStmt : Statement {
	std::string name;
	std::unique_ptr<TypeNode> type;  // null for walrus := (inferred)
	ExprPtr initializer;  // may be null
	bool isWalrus = false;
	VarDeclStmt() = default;
};

// Expression statement: foo(); x++;
struct ExprStmt : Statement {
	ExprPtr expression;
	ExprStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

// Return statement
struct ReturnStmt : Statement {
	ExprPtr value;  // may be null
	ReturnStmt(ExprPtr value = nullptr) : value(std::move(value)) {}
};

// Break statement
struct BreakStmt : Statement {};

// Continue statement
struct ContinueStmt : Statement {};

// If statement with optional elseif / else
struct IfStmt : Statement {
	ExprPtr condition;
	Block thenBlock;
	std::vector<std::pair<ExprPtr, Block>> elseIfBlocks;
	std::unique_ptr<Block> elseBlock;  // may be null
};

// While statement
struct WhileStmt : Statement {
	ExprPtr condition;
	Block body;
};

// For statement (C-style): for (init; cond; update) { ... }
struct ForStmt : Statement {
	StmtPtr init;	  // may be null (VarDeclStmt or ExprStmt)
	ExprPtr condition;	// may be null
	ExprPtr update;	   // may be null
	Block body;
};

// For-in statement: for (i: int in [0..10]) { ... }
struct ForInStmt : Statement {
	std::string varName;
	std::unique_ptr<TypeNode> varType;
	ExprPtr rangeStart;
	ExprPtr rangeEnd;
	Block body;
};

// Match statement
struct MatchCase {
	std::vector<ExprPtr> patterns;	// multiple patterns joined by |
	bool isDefault = false;			// _ => { ... }
	Block body;
};

struct MatchStmt : Statement {
	ExprPtr subject;
	std::vector<MatchCase> cases;
};

// Assignment statement: x = 5; x += 1; a.b[0] = 3;
struct AssignStmt : Statement {
	std::string op;
	ExprPtr target;
	ExprPtr value;
	AssignStmt(const std::string& op, ExprPtr target, ExprPtr value)
		: op(op), target(std::move(target)), value(std::move(value)) {}
};

// ========================
// Top-Level Declarations
// ========================

// A single imported symbol, optionally aliased: Foo, Bar as B
struct ImportItem {
	std::string name;    // original symbol name
	std::string alias;   // local alias (empty means no alias — use original name)
};

// import Foo, Bar as B from linker.linker;
// import linker.linker;
struct ImportDecl {
	std::vector<std::string> modulePath;  // e.g. ["linker", "linker"]
	std::vector<ImportItem> items;        // empty = namespace import (qualified access)
	bool isNamespaceImport = false;       // true when no item list (bare module import)
	int line = 0;
	int column = 0;
};

struct Parameter {
	std::string name;
	std::unique_ptr<TypeNode> type;
};

struct FunctionDecl {
	std::string name;
	std::vector<Parameter> params;
	std::unique_ptr<TypeNode> returnType;
	Block body;
	bool isExported = false;
	bool isOverridden = false;
	int line = 0;
	int column = 0;
};

struct StructMember {
	enum Kind { FIELD, METHOD };
	Kind kind;
	// For fields
	std::string fieldName;
	std::unique_ptr<TypeNode> fieldType;
	// For methods
	std::unique_ptr<FunctionDecl> method;
};

struct StructDecl {
	std::string name;
	std::vector<StructMember> members;
	bool isExported = false;
	int line = 0;
	int column = 0;
};

// ========================
// Program (root node)
// ========================

struct Program {
	std::vector<std::unique_ptr<ImportDecl>> imports;
	std::vector<std::unique_ptr<FunctionDecl>> functions;
	std::vector<std::unique_ptr<StructDecl>> structs;
};
