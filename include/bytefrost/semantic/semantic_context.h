#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "bytefrost/diagnostics/diagnostic_engine.h"
#include "bytefrost/semantic/bf_type.h"
#include "bytefrost/semantic/symbol_table.h"
#include "bytefrost/semantic/type_checker.h"
#include "bytefrost/semantic/type_resolver.h"
#include "parser/ast.h"

namespace bytefrost {

/// SemanticContext orchestrates all Phase 1 semantic analysis passes over an
/// AST Program.  It is the single entry-point that CodeGen (and future
/// pipeline stages) should invoke after parsing.
///
/// Usage:
///   SemanticContext sem;
///   bool ok = sem.analyze(program, sourceFile);
///   if (!ok) {
///       sem.diagnostics().emitToStderr();
///       return;
///   }
///
/// The analysis is deliberately conservative: when a type cannot be resolved
/// (e.g. inside a walrus assignment whose RHS is complex) it emits Unknown
/// and suppresses cascading errors so the first real mistake is the one the
/// user sees.
class SemanticContext {
public:
	SemanticContext();

	/// Run the full semantic analysis pipeline on `program`.
	/// `sourceFile` is used in diagnostic location messages.
	/// Returns true if no errors were emitted.
	bool analyze(const Program& program, const std::string& sourceFile = "<input>");

	DiagnosticEngine&       diagnostics()       { return diag_; }
	const DiagnosticEngine& diagnostics() const { return diag_; }

	const TypeResolver& typeResolver() const { return resolver_; }

private:
	DiagnosticEngine diag_;
	TypeResolver     resolver_;
	TypeChecker      checker_;
	ScopeManager     scopes_;

	std::string sourceFile_;
	std::string currentFunctionName_;
	std::string currentThisTypeName_;
	/// Local names brought in via import statements.  Used to suppress false-
	/// positive "undefined variable" / "unknown type" diagnostics for symbols
	/// that are resolved by the cross-module linker, not the single-file AST.
	std::unordered_set<std::string> importedNames_;

	struct FieldSemInfo {
		bool isReadonly = false;
		bool isConstant = false;
		bool isNullable = false;
		BFType type = BFType::makeUnknown();
	};

	// Struct name -> field name -> field semantic flags.
	std::unordered_map<std::string, std::unordered_map<std::string, FieldSemInfo>> structFieldInfo_;

	// Top-level function name -> parameter count (for call-site arity checking).
	std::unordered_map<std::string, std::size_t> funcParamCounts_;

	// ------------------------------------------------------------------
	// Pass 1: collect top-level declarations
	// ------------------------------------------------------------------
	void collectTopLevelDecls(const Program& program);
	void checkDuplicateDecls(const Program& program);
	void checkImportConflicts(const Program& program);
	void checkMathOverrides(const Program& program);
	void checkStructCycles(const Program& program);
	void checkUninitializedStructFields(const Program& program);

	// ------------------------------------------------------------------
	// Pass 2: analyse function bodies
	// ------------------------------------------------------------------
	void analyzeFunctions(const Program& program);
	void analyzeStructMethods(const Program& program);
	void analyzeFunction(const FunctionDecl& fn,
	                     const std::string& thisTypeName = "");
	void analyzeBlock(const Block& block);
	void analyzeStatement(const Statement& stmt);
	BFType analyzeExpression(const Expression& expr);

	// ------------------------------------------------------------------
	// Expression sub-analysers
	// ------------------------------------------------------------------
	BFType analyzeCall(const CallExpr& expr);
	BFType analyzeBinary(const BinaryExpr& expr);
	BFType analyzeUnary(const UnaryExpr& expr);
	BFType analyzeMemberAccess(const MemberAccessExpr& expr);
	BFType analyzeIndex(const IndexExpr& expr);
	BFType analyzeNullSafeAccess(const NullSafeAccessExpr& expr);

	bool isCompileTimeConstantExpr(const Expression& expr) const;

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------

	/// C.1: Validate that all non-nullable fields are present in a struct
	/// literal and no unknown field names are used.
	void checkStructInit(const std::string& structName,
	                     const StructInitExpr& initExpr,
	                     int line, int col);

	/// D.4: Analyse a block with one variable shadowed by its non-nullable
	/// type (null-narrowing after an `x != null` / truthy check).
	void analyzeBlockWithNarrowing(const Block& block,
	                               const std::string& varName,
	                               const BFType& narrowedType);

	SourceLocation loc(int line, int col) const;
	static const std::set<std::string>& stdlibMathNames();
};

}  // namespace bytefrost
