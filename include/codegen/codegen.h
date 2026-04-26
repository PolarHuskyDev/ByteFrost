#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Target/TargetMachine.h"

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

	/// Compile the program and write a native object file to disk.
	void emitObjectFile(const Program& program, const std::string& outputPath);

	/// Inject an extern (no-body) function declaration into the current module.
	/// Used by orca to satisfy cross-module call sites before codegen.
	void declareExternFunction(const FunctionDecl& fn);

	/// Access the module (for unit testing).
	llvm::Module& getModule() { return *module; }
	llvm::LLVMContext& getContext() { return *context; }

   private:
	std::unique_ptr<llvm::LLVMContext> context;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> builder;
	std::unique_ptr<llvm::TargetMachine> targetMachine;

	// Scope management: stack of variable maps.
	struct Scope {
		std::map<std::string, llvm::AllocaInst*> variables;
		std::map<std::string, llvm::Type*> varTypes;
		std::map<std::string, std::string> varBFTypeNames;  // ByteFrost type name ("Person", "array", "map", etc.)
		std::vector<std::string> heapOwned;  // names of vars that own heap data (need cleanup)
	};
	std::vector<Scope> scopes;

	// Break/continue targets for loops.
	std::vector<llvm::BasicBlock*> breakTargets;
	std::vector<llvm::BasicBlock*> continueTargets;

	// Built-in function declarations.
	llvm::Function* printfFunc = nullptr;
	llvm::Function* strcmpFunc = nullptr;
	llvm::Function* mallocFunc = nullptr;
	llvm::Function* reallocFunc = nullptr;
	llvm::Function* freeFunc = nullptr;
	llvm::Function* snprintfFunc = nullptr;

	// Names of stdlib math functions overridden by the current program.
	std::set<std::string> overriddenMathFuncs_;

	// Struct type registry.
	struct StructInfo {
		llvm::StructType* llvmType;
		std::vector<std::string> fieldNames;
		std::vector<llvm::Type*> fieldLLVMTypes;
		std::map<std::string, size_t> fieldIndices;
		std::map<std::string, std::string> methods;  // method name -> mangled LLVM function name
		bool hasConstructor = false;
	};
	std::map<std::string, StructInfo> structRegistry;

	// Target setup.
	void initializeTarget();

	// Shared IR building (used by generate() and emitObjectFile()).
	void buildIR(const Program& program);

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
	llvm::Value* generateInterpolatedString(const InterpolatedStringExpr& expr);

	// Struct support.
	void registerStructTypes(const Program& program);
	void generateStructMethods(const Program& program);
	void generateStructInit(const StructInitExpr& expr, llvm::Value* basePtr,
							const std::string& structName);
	std::pair<llvm::Value*, std::string> resolveStructBase(const Expression& expr);

	// Array support.
	llvm::StructType* getOrCreateArrayType(llvm::Type* elemType);
	llvm::Value* generateArrayLiteral(const ArrayLiteralExpr& expr, llvm::Type* elemType);
	llvm::Value* generateEmptyArray(llvm::Type* elemType);
	void generateArrayPush(llvm::AllocaInst* arrAlloca, llvm::Type* elemType, llvm::Value* value);

	// Map support.
	llvm::StructType* getOrCreateMapType(llvm::Type* keyType, llvm::Type* valType);
	llvm::Value* generateEmptyMap(llvm::Type* keyType, llvm::Type* valType);
	void generateMapSet(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType,
						llvm::Value* key, llvm::Value* val);
	llvm::Value* generateMapGet(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType,
								llvm::Value* key);

	// Built-in print handling.
	llvm::Value* generatePrintCall(const std::vector<ExprPtr>& args);

	// Math stdlib dispatch.
	llvm::Value* generateMathCall(const std::string& name, const std::vector<ExprPtr>& args);

	// The set of stdlib math function names (populated once, used everywhere).
	static const std::set<std::string>& stdlibMathNames();
	void generatePrintArray(llvm::AllocaInst* arrAlloca, llvm::Type* elemType);
	void generatePrintMap(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType);

	// Helpers.
	void declareBuiltins();
	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
											 const std::string& name,
											 llvm::Type* type);
	llvm::AllocaInst* lookupVariable(const std::string& name);
	llvm::Type* lookupVarType(const std::string& name);
	std::string lookupVarBFTypeName(const std::string& name);
	void pushScope();
	void popScope();
	void emitScopeCleanup();  // emit ref decrements / frees for current scope
	void emitRefIncrement(llvm::Value* structAlloca, bool isMap);
	void emitRefDecrement(llvm::Value* structAlloca, bool isMap);
	void declareVariable(const std::string& name, llvm::AllocaInst* alloca,
						 llvm::Type* type, const std::string& bfTypeName = "");

	/// Get a store-able pointer for an lvalue expression (for assignment).
	llvm::Value* generateLValue(const Expression& expr);

	/// Check if an LLVM type is a string type (i8*).
	bool isStringType(llvm::Type* type) const;

	/// Check if an LLVM type is float/double.
	bool isFloatType(llvm::Type* type) const;

	/// Get printf format specifier for a type.
	std::string getFormatSpecifier(llvm::Type* type) const;
};
