#pragma once

/// @file hir.h
/// ByteFrost High-level Intermediate Representation (HIR)
///
/// The HIR is a fully-typed, name-resolved intermediate representation that
/// sits between the parsed AST and LLVM lowering.  It is produced by
/// HIRBuilder after SemanticContext has validated the program.
///
/// Key properties:
///   • Every HIRExpr carries a resolved BFType (no re-inference needed by lowering).
///   • Control-flow is explicit: no parser syntax-sugar nodes survive into HIR.
///   • All import aliases and namespace qualifications are resolved.
///   • Struct field order is normalised (fields only, no method members).
///   • The LLVM lowering layer only consumes HIR — it has no dependency on the
///     raw AST types from parser/ast.h.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "bytefrost/semantic/bf_type.h"
#include "parser/ast.h"  // for ImportItem (re-used as-is in HIRImport)

namespace bytefrost {

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

struct HIRExpr;
struct HIRStmt;
struct HIRBlock;
struct HIRFunction;

using HIRExprPtr = std::unique_ptr<HIRExpr>;
using HIRStmtPtr = std::unique_ptr<HIRStmt>;

// ===========================================================================
// HIR Expressions
// ===========================================================================

/// Base class for all HIR expression nodes.
/// Every node carries its resolved type and source location.
struct HIRExpr {
    BFType type;
    int  line   = 0;
    int  column = 0;
    /// E.1: true when this expression evaluates to a compile-time constant
    /// (literal, unary/binary of constants, or reference to a const variable).
    bool isCompileTimeConstant = false;
    virtual ~HIRExpr() = default;
};

// ---------------------------------------------------------------------------

struct HIRIntLit : HIRExpr {
    int64_t     value;
    std::string raw;
    HIRIntLit(int64_t v, std::string r) : value(v), raw(std::move(r)) {
        type = BFType::makeInt();
        isCompileTimeConstant = true;
    }
};

struct HIRFloatLit : HIRExpr {
    double      value;
    std::string raw;
    HIRFloatLit(double v, std::string r) : value(v), raw(std::move(r)) {
        type = BFType::makeFloat();
        isCompileTimeConstant = true;
    }
};

struct HIRStringLit : HIRExpr {
    std::string value;
    explicit HIRStringLit(std::string v) : value(std::move(v)) {
        type = BFType::makeString();
        isCompileTimeConstant = true;
    }
};

struct HIRCharLit : HIRExpr {
    std::string value;
    explicit HIRCharLit(std::string v) : value(std::move(v)) {
        type = BFType::makeChar();
        isCompileTimeConstant = true;
    }
};

struct HIRBoolLit : HIRExpr {
    bool value;
    explicit HIRBoolLit(bool v) : value(v) {
        type = BFType::makeBool();
        isCompileTimeConstant = true;
    }
};

/// E.3: Null literal.  Carries the nullable type from context when known,
/// otherwise falls back to Unknown (compatible with any nullable).
struct HIRNullLit : HIRExpr {
    HIRNullLit() { type = BFType::makeUnknown(); isCompileTimeConstant = true; }
    explicit HIRNullLit(BFType t) { type = std::move(t); isCompileTimeConstant = true; }
};

/// A variable reference: resolves to its declared type.
struct HIRVar : HIRExpr {
    std::string name;
    explicit HIRVar(std::string n, BFType t) : name(std::move(n)) { type = std::move(t); }
};

/// `this` reference inside a struct method body.
struct HIRThis : HIRExpr {
    HIRThis() { type = BFType::makeUnknown(); /* filled in by HIRBuilder */ }
};

struct HIRBinaryExpr : HIRExpr {
    std::string  op;
    HIRExprPtr   left;
    HIRExprPtr   right;
    HIRBinaryExpr(std::string op, HIRExprPtr l, HIRExprPtr r, BFType t)
        : op(std::move(op)), left(std::move(l)), right(std::move(r)) {
        type = std::move(t);
    }
};

struct HIRUnaryExpr : HIRExpr {
    std::string op;
    HIRExprPtr  operand;
    bool        prefix;
    HIRUnaryExpr(std::string op, HIRExprPtr o, bool pre, BFType t)
        : op(std::move(op)), operand(std::move(o)), prefix(pre) {
        type = std::move(t);
    }
};

struct HIRCallExpr : HIRExpr {
    HIRExprPtr              callee;
    std::vector<HIRExprPtr> args;
    HIRCallExpr(HIRExprPtr callee, std::vector<HIRExprPtr> args, BFType retType)
        : callee(std::move(callee)), args(std::move(args)) {
        type = std::move(retType);
    }
};

struct HIRMemberAccess : HIRExpr {
    HIRExprPtr  object;
    std::string member;
    HIRMemberAccess(HIRExprPtr obj, std::string mem, BFType t)
        : object(std::move(obj)), member(std::move(mem)) {
        type = std::move(t);
    }
};

struct HIRIndexExpr : HIRExpr {
    HIRExprPtr object;
    HIRExprPtr index;
    HIRIndexExpr(HIRExprPtr obj, HIRExprPtr idx, BFType t)
        : object(std::move(obj)), index(std::move(idx)) {
        type = std::move(t);
    }
};

struct HIRArrayLit : HIRExpr {
    std::vector<HIRExprPtr> elements;
    HIRArrayLit(std::vector<HIRExprPtr> elems, BFType t)
        : elements(std::move(elems)) {
        type = std::move(t);
    }
};

struct HIRStructInit : HIRExpr {
    std::string                                  structName;
    std::vector<std::pair<std::string, HIRExprPtr>> fields;
    HIRStructInit(std::string sn, std::vector<std::pair<std::string, HIRExprPtr>> f, BFType t)
        : structName(std::move(sn)), fields(std::move(f)) {
        type = std::move(t);
    }
};

struct HIRInterpolatedString : HIRExpr {
    std::vector<std::string> fragments;
    std::vector<HIRExprPtr>  expressions;
    HIRInterpolatedString(std::vector<std::string> frags, std::vector<HIRExprPtr> exprs)
        : fragments(std::move(frags)), expressions(std::move(exprs)) {
        type = BFType::makeString();
    }
};

// ===========================================================================
// HIR Statements
// ===========================================================================

struct HIRStmt {
    int line   = 0;
    int column = 0;
    virtual ~HIRStmt() = default;
};

// A block is a sequential list of statements.
struct HIRBlock {
    std::vector<HIRStmtPtr> stmts;
};

// ---------------------------------------------------------------------------

struct HIRVarDecl : HIRStmt {
    std::string name;
    BFType      bfType;
    HIRExprPtr  init;          // may be null for default-initialised vars
    bool        isConstant = false;  // E.2: propagated from VarDeclStmt
    HIRVarDecl(std::string n, BFType t, HIRExprPtr i)
        : name(std::move(n)), bfType(std::move(t)), init(std::move(i)) {}
};

struct HIRExprStmt : HIRStmt {
    HIRExprPtr expr;
    explicit HIRExprStmt(HIRExprPtr e) : expr(std::move(e)) {}
};

struct HIRAssign : HIRStmt {
    std::string op;
    HIRExprPtr  target;
    HIRExprPtr  value;
    HIRAssign(std::string op, HIRExprPtr t, HIRExprPtr v)
        : op(std::move(op)), target(std::move(t)), value(std::move(v)) {}
};

struct HIRReturn : HIRStmt {
    HIRExprPtr value;  // null → void return
    explicit HIRReturn(HIRExprPtr v = nullptr) : value(std::move(v)) {}
};

struct HIRBreak    : HIRStmt {};
struct HIRContinue : HIRStmt {};

struct HIRIf : HIRStmt {
    HIRExprPtr                              condition;
    HIRBlock                                thenBlock;
    std::vector<std::pair<HIRExprPtr, HIRBlock>> elseIfs;
    std::optional<HIRBlock>                 elseBlock;
};

struct HIRWhile : HIRStmt {
    HIRExprPtr condition;
    HIRBlock   body;
};

struct HIRFor : HIRStmt {
    HIRStmtPtr init;       // null for bare-for
    HIRExprPtr condition;  // null → infinite loop
    HIRExprPtr update;     // null if absent
    HIRBlock   body;
};

struct HIRForIn : HIRStmt {
    std::string varName;
    BFType      varType;
    HIRExprPtr  range;  // Phase 3: Single range field instead of rangeStart/rangeEnd
    HIRBlock    body;
};

struct HIRMatchCase {
    std::vector<HIRExprPtr> patterns;
    bool                    isDefault = false;
    HIRBlock                body;
};

struct HIRMatch : HIRStmt {
    HIRExprPtr              subject;
    std::vector<HIRMatchCase> cases;
};

// ===========================================================================
// HIR Top-Level Declarations
// ===========================================================================

struct HIRParam {
    std::string name;
    BFType      type;
};

struct HIRFunction {
    std::string           name;
    std::vector<HIRParam> params;
    BFType                returnType;
    HIRBlock              body;
    bool                  isExported   = false;
    bool                  isOverridden = false;
    /// Non-empty when the function is a struct method. Holds the struct name.
    std::string           ownerStruct;
    int line   = 0;
    int column = 0;
};

struct HIRStructField {
    std::string name;
    BFType      type;
    bool        isReadonly = false;  // E.3: propagated from StructMember
    bool        isConstant = false;  // E.3: propagated from StructMember
};

struct HIRStruct {
    std::string                  name;
    std::vector<HIRStructField>  fields;
    std::vector<HIRFunction>     methods;
    bool                         isExported = false;
    int line   = 0;
    int column = 0;
};

struct HIREnumVariant {
    std::string name;
    int32_t     value = 0;
};

struct HIREnum {
    std::string               name;
    std::vector<HIREnumVariant> variants;
    bool                      isExported = false;
    int line   = 0;
    int column = 0;
};

struct HIRImport {
    std::vector<std::string> modulePath;
    std::vector<ImportItem>  items;          // ImportItem from parser/ast.h
    bool                     isNamespaceImport = false;
    int line   = 0;
    int column = 0;
};

// ===========================================================================
// HIR Program (root)
// ===========================================================================

struct HIRProgram {
    std::vector<HIRImport>   imports;
    std::vector<HIREnum>     enums;
    std::vector<HIRStruct>   structs;
    std::vector<HIRFunction> functions;
};

}  // namespace bytefrost
