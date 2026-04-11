#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include "parser/ast.h"

class CodeGenError : public std::runtime_error {
   public:
	CodeGenError(const std::string& msg) : std::runtime_error(msg) {}
};

class CodeGen {
   public:
	CodeGen();

	/// Generate LLVM IR for the entire program. Returns the IR as a string.
	std::string generate(const Program& program);

	/// Access the module (for unit testing).
	llvm::Module& getModule() { return *module; }
	llvm::LLVMContext& getContext() { return *context; }

   private:
	std::unique_ptr<llvm::LLVMContext> context;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> builder;

	// Scope management: stack of variable maps.
	struct Scope {
		std::map<std::string, llvm::AllocaInst*> variables;
		std::map<std::string, llvm::Type*> varTypes;
	};
	std::vector<Scope> scopes;

	// Break/continue targets for loops.
	std::vector<llvm::BasicBlock*> breakTargets;
	std::vector<llvm::BasicBlock*> continueTargets;

	// Built-in function declarations.
	llvm::Function* printfFunc = nullptr;
	llvm::Function* strcmpFunc = nullptr;

	// Core generation.
	void generateFunction(const FunctionDecl& fn);
	void generateStatement(const Statement& stmt);
	llvm::Value* generateExpression(const Expression& expr);
	llvm::Type* getLLVMType(const TypeNode& type);

	// Statements.
	void generateVarDecl(const VarDeclStmt& stmt);
	void generateAssign(const AssignStmt& stmt);
	void generateIf(const IfStmt& stmt);
	void generateWhile(const WhileStmt& stmt);
	void generateFor(const ForStmt& stmt);
	void generateForIn(const ForInStmt& stmt);
	void generateMatch(const MatchStmt& stmt);
	void generateReturn(const ReturnStmt& stmt);
	void generateExprStmt(const ExprStmt& stmt);

	// Expressions.
	llvm::Value* generateBinary(const BinaryExpr& expr);
	llvm::Value* generateUnary(const UnaryExpr& expr);
	llvm::Value* generateCall(const CallExpr& expr);
	llvm::Value* generateIdentifier(const IdentifierExpr& expr);
	llvm::Value* generateIndex(const IndexExpr& expr);
	llvm::Value* generateMemberAccess(const MemberAccessExpr& expr);

	// Built-in print handling.
	llvm::Value* generatePrintCall(const std::vector<ExprPtr>& args);

	// Helpers.
	void declareBuiltins();
	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
											 const std::string& name,
											 llvm::Type* type);
	llvm::AllocaInst* lookupVariable(const std::string& name);
	llvm::Type* lookupVarType(const std::string& name);
	void pushScope();
	void popScope();
	void declareVariable(const std::string& name, llvm::AllocaInst* alloca, llvm::Type* type);

	/// Get a store-able pointer for an lvalue expression (for assignment).
	llvm::Value* generateLValue(const Expression& expr);

	/// Check if an LLVM type is a string type (i8*).
	bool isStringType(llvm::Type* type) const;

	/// Check if an LLVM type is float/double.
	bool isFloatType(llvm::Type* type) const;
};
