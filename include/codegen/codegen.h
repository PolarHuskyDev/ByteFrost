#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
#include "bytefrost/hir/hir.h"

class CodeGenError : public std::runtime_error {
   public:
	CodeGenError(const std::string& msg) : std::runtime_error(msg) {
	}
};

class CodeGen {
   public:
	CodeGen();

	/// Optimization level — mirrors Rust/Cargo opt-level (0/1/2/3/"s"/"z").
	enum class OptLevel { O0, O1, O2, O3, Os, Oz };
	void setOptLevel(OptLevel level) {
		optLevel_ = level;
	}

	/// Generate LLVM IR for the entire program. Returns the IR as a string.
	std::string generate(const Program& program);

	/// Compile the program and write a native object file to disk.
	void emitObjectFile(const Program& program, const std::string& outputPath);

	/// Inject an extern (no-body) function declaration into the current module.
	/// Used by orca to satisfy cross-module call sites before codegen.
	void declareExternFunction(const FunctionDecl& fn);

	/// Pre-register an exported enum type from an imported module.
	/// Must be called before declareExternStruct for structs that reference this enum.
	void declareExternEnum(const EnumDecl& ed);

	/// Pre-register an exported struct type from an imported module.
	/// All enum and struct types referenced by its fields must be declared first.
	void declareExternStruct(const StructDecl& sd);

	/// Access the module (for unit testing).
	llvm::Module& getModule() {
		return *module;
	}
	llvm::LLVMContext& getContext() {
		return *context;
	}

   private:
	std::unique_ptr<llvm::LLVMContext> context;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> builder;
	std::unique_ptr<llvm::TargetMachine> targetMachine;
	OptLevel optLevel_ = OptLevel::O0;

	// Scope management: stack of variable maps.
	struct Scope {
		std::map<std::string, llvm::AllocaInst*> variables;
		std::map<std::string, llvm::Type*> varTypes;
		std::map<std::string, std::string> varBFTypeNames;	// ByteFrost type name ("Person", "array", "map", etc.)
		std::vector<std::string> heapOwned;					// names of vars that own heap data (need cleanup)
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
	llvm::Function* scanfFunc = nullptr;
	llvm::Function* fflushFunc = nullptr;

	// Names of stdlib math functions overridden by the current program.
	std::set<std::string> overriddenMathFuncs_;

	// Import alias map: local alias name → original function name.
	// e.g. 'import abs as myAbs from math.utils' → importAliases_["myAbs"] = "abs"
	std::unordered_map<std::string, std::string> importAliases_;

	// Namespace import local names (last segment of the module path).
	// e.g. 'import math.utils' → namespaceNames_.insert("utils")
	std::unordered_set<std::string> namespaceNames_;

	// Struct type registry.
	struct StructInfo {
		llvm::StructType* llvmType;
		std::vector<std::string> fieldNames;
		std::vector<llvm::Type*> fieldLLVMTypes;
		std::map<std::string, size_t> fieldIndices;
		std::map<std::string, std::string> methods;	 // method name -> mangled LLVM function name
		std::map<std::string, std::string> fieldBFTypeNames;  // field name -> BF type name (for enums)
		bool hasConstructor = false;
	};
	std::map<std::string, StructInfo> structRegistry;

	// Enum type registry.
	struct EnumInfo {
		std::map<std::string, int32_t> variants;  // variant name -> integer value
		std::vector<std::string> variantNames;    // ordered list (index == value)
	};
	std::map<std::string, EnumInfo> enumRegistry;

	// Target setup.
	void initializeTarget();

	// Shared IR building (used by generate() and emitObjectFile()).
	void buildIR(const Program& program);

	// Core generation — all lowering methods now consume the typed HIR.
	void generateFunction(const bytefrost::HIRFunction& fn);
	void generateStatement(const bytefrost::HIRStmt& stmt);
	llvm::Value* generateExpression(const bytefrost::HIRExpr& expr);
	llvm::Type* getLLVMType(const TypeNode& type);
	llvm::Type* getLLVMType(const bytefrost::BFType& type);

	// Statements.
	void generateVarDecl(const bytefrost::HIRVarDecl& stmt);
	void generateAssign(const bytefrost::HIRAssign& stmt);
	void generateIf(const bytefrost::HIRIf& stmt);
	void generateWhile(const bytefrost::HIRWhile& stmt);
	void generateFor(const bytefrost::HIRFor& stmt);
	void generateForIn(const bytefrost::HIRForIn& stmt);
	void generateMatch(const bytefrost::HIRMatch& stmt);
	void generateReturn(const bytefrost::HIRReturn& stmt);
	void generateExprStmt(const bytefrost::HIRExprStmt& stmt);

	// Expressions.
	llvm::Value* generateBinary(const bytefrost::HIRBinaryExpr& expr);
	llvm::Value* generateUnary(const bytefrost::HIRUnaryExpr& expr);
	llvm::Value* generateCall(const bytefrost::HIRCallExpr& expr);
	llvm::Value* generateIdentifier(const bytefrost::HIRVar& expr);
	llvm::Value* generateIndex(const bytefrost::HIRIndexExpr& expr);
	llvm::Value* generateMemberAccess(const bytefrost::HIRMemberAccess& expr);
	llvm::Value* generateInterpolatedString(const bytefrost::HIRInterpolatedString& expr);

	// Struct support.
	void registerStructTypes(const bytefrost::HIRProgram& hir);
	void generateStructMethods(const bytefrost::HIRProgram& hir);
	void generateStructInit(const bytefrost::HIRStructInit& expr, llvm::Value* basePtr, const std::string& structName);
	std::pair<llvm::Value*, std::string> resolveStructBase(const bytefrost::HIRExpr& expr);

	// Enum support.
	void registerEnumTypes(const bytefrost::HIRProgram& hir);
	/// Get the ByteFrost type name string for a HIR expression (for scope tracking).
	std::string bfTypeToString(const bytefrost::BFType& t) const;
	/// Given an i32 enum value, return an i8* pointing to the variant's name string.
	llvm::Value* generateEnumToString(llvm::Value* enumVal, const std::string& enumTypeName);

	// Array support.
	llvm::StructType* getOrCreateArrayType(llvm::Type* elemType);
	llvm::Value* generateArrayLiteral(const bytefrost::HIRArrayLit& expr, llvm::Type* elemType);
	llvm::Value* generateEmptyArray(llvm::Type* elemType);
void generateArrayPush(llvm::Value* arrPtr, llvm::Type* elemType, llvm::Value* value);

	// Map support.
	llvm::StructType* getOrCreateMapType(llvm::Type* keyType, llvm::Type* valType);
	llvm::Value* generateEmptyMap(llvm::Type* keyType, llvm::Type* valType);
	void generateMapSet(
		llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType, llvm::Value* key, llvm::Value* val);
	llvm::Value*
	generateMapGet(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType, llvm::Value* key);

	// Built-in print handling.
	llvm::Value* generatePrintCall(const std::vector<bytefrost::HIRExprPtr>& args);

	// Built-in input handling.
	/// Generate an input() call that reads from stdin.
	/// targetTypeName: the BF type name the result is being stored into ("int", "float", "bool", "string", "").
	llvm::Value* generateInputCall(const std::vector<bytefrost::HIRExprPtr>& args, const std::string& targetTypeName);

	// Math stdlib dispatch.
	llvm::Value* generateMathCall(const std::string& name, const std::vector<bytefrost::HIRExprPtr>& args);

	// The set of stdlib math function names (populated once, used everywhere).
	static const std::set<std::string>& stdlibMathNames();
	void generatePrintArray(llvm::AllocaInst* arrAlloca, llvm::Type* elemType);
	void generatePrintMap(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType);

	// Helpers.
	void declareBuiltins();
	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn, const std::string& name, llvm::Type* type);
	llvm::AllocaInst* lookupVariable(const std::string& name);
	llvm::Type* lookupVarType(const std::string& name);
	std::string lookupVarBFTypeName(const std::string& name);
	void pushScope();
	void popScope();
	void emitScopeCleanup();  // emit ref decrements / frees for current scope
	void emitRefIncrement(llvm::Value* structAlloca, bool isMap);
	void emitRefDecrement(llvm::Value* structAlloca, bool isMap);
	void declareVariable(const std::string& name,
						 llvm::AllocaInst* alloca,
						 llvm::Type* type,
						 const std::string& bfTypeName = "");

	/// Get a store-able pointer for an lvalue HIR expression (for assignment).
	llvm::Value* generateLValue(const bytefrost::HIRExpr& expr);

	/// Check if an LLVM type is a string type (i8*).
	bool isStringType(llvm::Type* type) const;

	/// Check if an LLVM type is float/double.
	bool isFloatType(llvm::Type* type) const;

	/// Get printf format specifier for a type.
	std::string getFormatSpecifier(llvm::Type* type) const;
};
