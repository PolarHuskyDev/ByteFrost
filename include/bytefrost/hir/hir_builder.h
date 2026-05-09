#pragma once

/// @file hir_builder.h
/// Converts a validated AST Program into a typed HIRProgram.
///
/// Usage:
///   SemanticContext sem;
///   bool ok = sem.analyze(program, sourceFile);
///   // (abort if !ok)
///   HIRBuilder builder(sem);
///   HIRProgram hir = builder.build(program);
///
/// HIRBuilder assumes the program has already passed semantic validation.
/// It does NOT emit diagnostics — type errors should have been caught earlier.

#include "bytefrost/hir/hir.h"
#include "bytefrost/semantic/semantic_context.h"
#include "bytefrost/semantic/symbol_table.h"
#include "parser/ast.h"

#include <set>
#include <unordered_map>

namespace bytefrost {

class HIRBuilder {
public:
    explicit HIRBuilder(const SemanticContext& sem);

    /// Build a full HIRProgram from a validated AST Program.
    HIRProgram build(const Program& program);

private:
    const SemanticContext& sem_;
    /// Local scope stack — mirrors the semantic context scopes for HIR-time
    /// type resolution of variable identifiers.
    ScopeManager scopes_;
    /// The struct name currently being processed (for method owner tracking).
    std::string currentStructName_;
    /// Struct field type table: structName → (fieldName → BFType).
    /// Populated during buildStruct(), used in buildMemberAccess().
    std::unordered_map<std::string, std::unordered_map<std::string, BFType>> structFieldTypes_;

    // -----------------------------------------------------------------------
    // Top-level builders
    // -----------------------------------------------------------------------
    HIRImport   buildImport(const ImportDecl& d);
    HIREnum     buildEnum(const EnumDecl& d);
    HIRStruct   buildStruct(const StructDecl& d);
    HIRFunction buildFunction(const FunctionDecl& fn,
                              const std::string& ownerStruct = "");

    // -----------------------------------------------------------------------
    // Statement / block builders
    // -----------------------------------------------------------------------
    HIRBlock    buildBlock(const Block& block);
    HIRStmtPtr  buildStatement(const Statement& stmt);
    HIRStmtPtr  buildVarDecl(const VarDeclStmt& s);
    HIRStmtPtr  buildAssign(const AssignStmt& s);
    HIRStmtPtr  buildIf(const IfStmt& s);
    HIRStmtPtr  buildWhile(const WhileStmt& s);
    HIRStmtPtr  buildFor(const ForStmt& s);
    HIRStmtPtr  buildForIn(const ForInStmt& s);
    HIRStmtPtr  buildMatch(const MatchStmt& s);
    HIRStmtPtr  buildReturn(const ReturnStmt& s);
    HIRStmtPtr  buildExprStmt(const ExprStmt& s);

    // -----------------------------------------------------------------------
    // Expression builders
    // -----------------------------------------------------------------------
    HIRExprPtr  buildExpr(const Expression& expr);
    HIRExprPtr  buildBinary(const BinaryExpr& e);
    HIRExprPtr  buildUnary(const UnaryExpr& e);
    HIRExprPtr  buildCall(const CallExpr& e);
    HIRExprPtr  buildIndex(const IndexExpr& e);
    HIRExprPtr  buildMemberAccess(const MemberAccessExpr& e);
    HIRExprPtr  buildArrayLit(const ArrayLiteralExpr& e);
    HIRExprPtr  buildStructInit(const StructInitExpr& e);
    HIRExprPtr  buildInterpolated(const InterpolatedStringExpr& e);

    // -----------------------------------------------------------------------
    // Type helpers
    // -----------------------------------------------------------------------
    /// Convert a TypeNode to its BFType equivalent.
    BFType typeNodeToBFType(const TypeNode& tn) const;

    /// Infer the BFType of an expression from the HIR expression itself.
    /// Used only for the limited cases where the type is deterministic from
    /// the expression kind alone (e.g., binary arithmetic result).
    BFType inferBinaryResultType(const std::string& op,
                                 const BFType& left,
                                 const BFType& right) const;

    /// Resolve an identifier to its BFType using the semantic type resolver.
    BFType resolveIdentifierType(const std::string& name) const;
};

}  // namespace bytefrost
