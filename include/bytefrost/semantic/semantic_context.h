#pragma once

#include <set>
#include <string>
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
	/// Local names brought in via import statements.  Used to suppress false-
	/// positive "undefined variable" / "unknown type" diagnostics for symbols
	/// that are resolved by the cross-module linker, not the single-file AST.
	std::unordered_set<std::string> importedNames_;

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

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	SourceLocation loc(int line, int col) const;
	static const std::set<std::string>& stdlibMathNames();
};

}  // namespace bytefrost
