#include "bytefrost/hir/hir_builder.h"

#include <stdexcept>

namespace bytefrost {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

HIRBuilder::HIRBuilder(const SemanticContext& sem)
    : sem_(sem) {}

// ---------------------------------------------------------------------------
// Top-level build
// ---------------------------------------------------------------------------

HIRProgram HIRBuilder::build(const Program& program) {
    HIRProgram hir;

    for (const auto& imp : program.imports)
        hir.imports.push_back(buildImport(*imp));
    for (const auto& ed : program.enums)
        hir.enums.push_back(buildEnum(*ed));
    for (const auto& sd : program.structs)
        hir.structs.push_back(buildStruct(*sd));
    for (const auto& fn : program.functions)
        hir.functions.push_back(buildFunction(*fn));

    return hir;
}

// ---------------------------------------------------------------------------
// Top-level builders
// ---------------------------------------------------------------------------

HIRImport HIRBuilder::buildImport(const ImportDecl& d) {
    HIRImport imp;
    imp.modulePath       = d.modulePath;
    imp.items            = d.items;
    imp.isNamespaceImport = d.isNamespaceImport;
    imp.line             = d.line;
    imp.column           = d.column;
    return imp;
}

HIREnum HIRBuilder::buildEnum(const EnumDecl& d) {
    HIREnum e;
    e.name       = d.name;
    e.isExported = d.isExported;
    e.line       = d.line;
    e.column     = d.column;
    for (const auto& v : d.variants) {
        HIREnumVariant hv;
        hv.name  = v.name;
        hv.value = v.value;
        e.variants.push_back(std::move(hv));
    }
    return e;
}

HIRStruct HIRBuilder::buildStruct(const StructDecl& d) {
    HIRStruct hs;
    hs.name       = d.name;
    hs.isExported = d.isExported;
    hs.line       = d.line;
    hs.column     = d.column;

    currentStructName_ = d.name;

    for (const auto& m : d.members) {
        if (m.kind == StructMember::FIELD) {
            HIRStructField hf;
            hf.name = m.fieldName;
            hf.type = m.fieldType ? typeNodeToBFType(*m.fieldType)
                                  : BFType::makeUnknown();
            structFieldTypes_[d.name][m.fieldName] = hf.type;
            hs.fields.push_back(std::move(hf));
        } else if (m.kind == StructMember::METHOD && m.method) {
            hs.methods.push_back(buildFunction(*m.method, d.name));
        }
    }

    currentStructName_.clear();
    return hs;
}

HIRFunction HIRBuilder::buildFunction(const FunctionDecl& fn,
                                      const std::string& ownerStruct) {
    HIRFunction hf;
    hf.name        = fn.name;
    hf.isExported  = fn.isExported;
    hf.isOverridden = fn.isOverridden;
    hf.ownerStruct = ownerStruct;
    hf.line        = fn.line;
    hf.column      = fn.column;

    hf.returnType = fn.returnType ? typeNodeToBFType(*fn.returnType)
                                  : BFType::makeVoid();

    for (const auto& p : fn.params) {
        HIRParam hp;
        hp.name = p.name;
        hp.type = p.type ? typeNodeToBFType(*p.type) : BFType::makeUnknown();
        hf.params.push_back(std::move(hp));
    }

    // Rebuild type scopes for this function so identifiers resolve correctly.
    scopes_.pushScope();

    // Inject 'this' for methods.
    if (!ownerStruct.empty()) {
        scopes_.declare("this", {"this", BFType::makeStruct(ownerStruct),
                                 false, false, {}});
    }

    // Declare parameters in scope.
    for (const auto& p : hf.params) {
        scopes_.declare(p.name, {p.name, p.type, true, false, {}});
    }

    hf.body = buildBlock(fn.body);

    scopes_.popScope();
    return hf;
}

// ---------------------------------------------------------------------------
// Block / statement builders
// ---------------------------------------------------------------------------

HIRBlock HIRBuilder::buildBlock(const Block& block) {
    HIRBlock hb;
    scopes_.pushScope();
    for (const auto& s : block.statements)
        hb.stmts.push_back(buildStatement(*s));
    scopes_.popScope();
    return hb;
}

HIRStmtPtr HIRBuilder::buildStatement(const Statement& stmt) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt))
        return buildVarDecl(*s);
    if (auto* s = dynamic_cast<const AssignStmt*>(&stmt))
        return buildAssign(*s);
    if (auto* s = dynamic_cast<const IfStmt*>(&stmt))
        return buildIf(*s);
    if (auto* s = dynamic_cast<const WhileStmt*>(&stmt))
        return buildWhile(*s);
    if (auto* s = dynamic_cast<const ForStmt*>(&stmt))
        return buildFor(*s);
    if (auto* s = dynamic_cast<const ForInStmt*>(&stmt))
        return buildForIn(*s);
    if (auto* s = dynamic_cast<const MatchStmt*>(&stmt))
        return buildMatch(*s);
    if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt))
        return buildReturn(*s);
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt))
        return buildExprStmt(*s);
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        auto r = std::make_unique<HIRBreak>();
        r->line = stmt.line; r->column = stmt.column;
        return r;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        auto r = std::make_unique<HIRContinue>();
        r->line = stmt.line; r->column = stmt.column;
        return r;
    }
    // Fallback — should not happen after successful semantic analysis.
    throw std::runtime_error("HIRBuilder: unhandled statement type");
}

HIRStmtPtr HIRBuilder::buildVarDecl(const VarDeclStmt& s) {
    auto node = std::make_unique<HIRVarDecl>("", BFType::makeUnknown(), nullptr);
    node->line   = s.line;
    node->column = s.column;
    node->name   = s.name;

    HIRExprPtr init;
    BFType     initType = BFType::makeUnknown();
    if (s.initializer) {
        init     = buildExpr(*s.initializer);
        initType = init->type;
    }

    if (s.type) {
        node->bfType = typeNodeToBFType(*s.type);
    } else {
        // Walrus / inferred.
        node->bfType = initType;
    }
    node->init = std::move(init);

    // Register in current scope so later references resolve.
    scopes_.declare(s.name, {s.name, node->bfType, true, false, {}});
    return node;
}

HIRStmtPtr HIRBuilder::buildAssign(const AssignStmt& s) {
    auto node = std::make_unique<HIRAssign>(
        s.op, buildExpr(*s.target), buildExpr(*s.value));
    node->line   = s.line;
    node->column = s.column;
    return node;
}

HIRStmtPtr HIRBuilder::buildIf(const IfStmt& s) {
    auto node = std::make_unique<HIRIf>();
    node->line      = s.line;
    node->column    = s.column;
    node->condition = buildExpr(*s.condition);
    node->thenBlock = buildBlock(s.thenBlock);
    for (const auto& [cond, body] : s.elseIfBlocks) {
        node->elseIfs.emplace_back(buildExpr(*cond), buildBlock(body));
    }
    if (s.elseBlock)
        node->elseBlock = buildBlock(*s.elseBlock);
    return node;
}

HIRStmtPtr HIRBuilder::buildWhile(const WhileStmt& s) {
    auto node = std::make_unique<HIRWhile>();
    node->line      = s.line;
    node->column    = s.column;
    node->condition = buildExpr(*s.condition);
    node->body      = buildBlock(s.body);
    return node;
}

HIRStmtPtr HIRBuilder::buildFor(const ForStmt& s) {
    auto node = std::make_unique<HIRFor>();
    node->line   = s.line;
    node->column = s.column;
    scopes_.pushScope();
    if (s.init)      node->init      = buildStatement(*s.init);
    if (s.condition) node->condition = buildExpr(*s.condition);
    if (s.update)    node->update    = buildExpr(*s.update);
    node->body = buildBlock(s.body);
    scopes_.popScope();
    return node;
}

HIRStmtPtr HIRBuilder::buildForIn(const ForInStmt& s) {
    auto node = std::make_unique<HIRForIn>();
    node->line       = s.line;
    node->column     = s.column;
    node->varName    = s.varName;
    node->varType    = s.varType ? typeNodeToBFType(*s.varType) : BFType::makeInt();
    node->rangeStart = buildExpr(*s.rangeStart);
    node->rangeEnd   = buildExpr(*s.rangeEnd);
    scopes_.pushScope();
    scopes_.declare(s.varName, {s.varName, node->varType, true, false, {}});
    node->body = buildBlock(s.body);
    scopes_.popScope();
    return node;
}

HIRStmtPtr HIRBuilder::buildMatch(const MatchStmt& s) {
    auto node = std::make_unique<HIRMatch>();
    node->line   = s.line;
    node->column = s.column;
    node->subject = buildExpr(*s.subject);
    for (const auto& c : s.cases) {
        HIRMatchCase hc;
        hc.isDefault = c.isDefault;
        for (const auto& p : c.patterns)
            hc.patterns.push_back(buildExpr(*p));
        hc.body = buildBlock(c.body);
        node->cases.push_back(std::move(hc));
    }
    return node;
}

HIRStmtPtr HIRBuilder::buildReturn(const ReturnStmt& s) {
    auto node = std::make_unique<HIRReturn>(
        s.value ? buildExpr(*s.value) : nullptr);
    node->line   = s.line;
    node->column = s.column;
    return node;
}

HIRStmtPtr HIRBuilder::buildExprStmt(const ExprStmt& s) {
    auto node = std::make_unique<HIRExprStmt>(buildExpr(*s.expression));
    node->line   = s.line;
    node->column = s.column;
    return node;
}

// ---------------------------------------------------------------------------
// Expression builders
// ---------------------------------------------------------------------------

HIRExprPtr HIRBuilder::buildExpr(const Expression& expr) {
    if (auto* e = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRIntLit>(e->value, e->raw);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (auto* e = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRFloatLit>(e->value, e->raw);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (auto* e = dynamic_cast<const StringLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRStringLit>(e->value);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (auto* e = dynamic_cast<const CharLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRCharLit>(e->value);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (auto* e = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRBoolLit>(e->value);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (dynamic_cast<const NullLiteralExpr*>(&expr)) {
        auto n = std::make_unique<HIRNullLit>();
        n->line = expr.line; n->column = expr.column;
        return n;
    }
    if (dynamic_cast<const ThisExpr*>(&expr)) {
        auto n = std::make_unique<HIRThis>();
        n->line = expr.line; n->column = expr.column;
        if (const auto* sym = scopes_.lookup("this"))
            n->type = sym->type;
        else if (!currentStructName_.empty())
            n->type = BFType::makeStruct(currentStructName_);
        return n;
    }
    if (auto* e = dynamic_cast<const IdentifierExpr*>(&expr)) {
        BFType t = resolveIdentifierType(e->name);
        auto n = std::make_unique<HIRVar>(e->name, t);
        n->line = e->line; n->column = e->column;
        return n;
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr))
        return buildBinary(*e);
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr))
        return buildUnary(*e);
    if (auto* e = dynamic_cast<const CallExpr*>(&expr))
        return buildCall(*e);
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr))
        return buildIndex(*e);
    if (auto* e = dynamic_cast<const MemberAccessExpr*>(&expr))
        return buildMemberAccess(*e);
    if (auto* e = dynamic_cast<const ArrayLiteralExpr*>(&expr))
        return buildArrayLit(*e);
    if (auto* e = dynamic_cast<const StructInitExpr*>(&expr))
        return buildStructInit(*e);
    if (auto* e = dynamic_cast<const InterpolatedStringExpr*>(&expr))
        return buildInterpolated(*e);
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
        // Treat assign-expression as a wrapped assign.
        auto lhs = buildExpr(*e->target);
        auto rhs = buildExpr(*e->value);
        BFType t = lhs->type;
        // Re-wrap as a call-expression placeholder: semantics preserved
        // (the codegen already handles AssignExpr in generateExpression).
        // We map it to a HIRBinaryExpr with op="assign_expr" to carry both operands.
        auto n = std::make_unique<HIRBinaryExpr>(
            "assign_expr:" + e->op, std::move(lhs), std::move(rhs), t);
        n->line = e->line; n->column = e->column;
        return n;
    }
    // Fallback.
    auto n = std::make_unique<HIRVar>("__unknown__", BFType::makeUnknown());
    n->line = expr.line; n->column = expr.column;
    return n;
}

HIRExprPtr HIRBuilder::buildBinary(const BinaryExpr& e) {
    auto left  = buildExpr(*e.left);
    auto right = buildExpr(*e.right);
    BFType t = inferBinaryResultType(e.op, left->type, right->type);
    auto n = std::make_unique<HIRBinaryExpr>(e.op, std::move(left), std::move(right), t);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildUnary(const UnaryExpr& e) {
    auto operand = buildExpr(*e.operand);
    BFType t = operand->type;
    auto n = std::make_unique<HIRUnaryExpr>(e.op, std::move(operand), e.prefix, t);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildCall(const CallExpr& e) {
    // Determine return type: try to look up the function in scope.
    BFType retType = BFType::makeUnknown();
    if (auto* id = dynamic_cast<const IdentifierExpr*>(e.callee.get())) {
        if (const auto* sym = scopes_.lookup(id->name))
            retType = sym->type;
        // Stdlib math functions return float.
        static const std::set<std::string> kMath = {
            "sin","cos","tan","sqrt","pow","floor","ceil","round",
            "abs","log","log2","log10","exp","min","max"};
        if (kMath.count(id->name))
            retType = BFType::makeFloat();
        if (id->name == "rand")
            retType = BFType::makeInt();
        if (id->name == "input")
            retType = BFType::makeString();
        if (id->name == "print")
            retType = BFType::makeVoid();
        // Struct constructor call — returns a struct.
        if (sem_.typeResolver().isKnownStruct(id->name))
            retType = BFType::makeStruct(id->name);
    }
    // Method call: a.foo() — return type is unknown without full method table.
    // Leave Unknown; the codegen will resolve it the old way.

    HIRExprPtr callee = buildExpr(*e.callee);
    std::vector<HIRExprPtr> args;
    for (const auto& a : e.arguments)
        args.push_back(buildExpr(*a));

    auto n = std::make_unique<HIRCallExpr>(std::move(callee), std::move(args), retType);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildIndex(const IndexExpr& e) {
    auto obj = buildExpr(*e.object);
    auto idx = buildExpr(*e.index);
    // Element type: if obj is Array<T>, result is T; if obj is Map<K,V>, result is V.
    BFType elemType = BFType::makeUnknown();
    if (obj->type.kind == BFTypeKind::Array && obj->type.elemType)
        elemType = *obj->type.elemType;
    else if (obj->type.kind == BFTypeKind::Map && obj->type.valueType)
        elemType = *obj->type.valueType;
    auto n = std::make_unique<HIRIndexExpr>(std::move(obj), std::move(idx), elemType);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildMemberAccess(const MemberAccessExpr& e) {
    auto obj = buildExpr(*e.object);
    // Resolve the result type where possible.
    BFType t = BFType::makeUnknown();
    if (obj->type.kind == BFTypeKind::Enum) {
        // Color.GREEN → type is Color (same enum)
        t = obj->type;
    } else if (obj->type.kind == BFTypeKind::Struct) {
        // c.field → look up field type in struct table
        auto structIt = structFieldTypes_.find(obj->type.name);
        if (structIt != structFieldTypes_.end()) {
            auto fieldIt = structIt->second.find(e.member);
            if (fieldIt != structIt->second.end())
                t = fieldIt->second;
        }
    }
    auto n = std::make_unique<HIRMemberAccess>(std::move(obj), e.member, t);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildArrayLit(const ArrayLiteralExpr& e) {
    std::vector<HIRExprPtr> elems;
    BFType elemType = BFType::makeUnknown();
    for (const auto& el : e.elements) {
        auto he = buildExpr(*el);
        if (elemType.isUnknown()) elemType = he->type;
        elems.push_back(std::move(he));
    }
    BFType arrType = BFType::makeArray(elemType);
    auto n = std::make_unique<HIRArrayLit>(std::move(elems), arrType);
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildStructInit(const StructInitExpr& e) {
    std::vector<std::pair<std::string, HIRExprPtr>> fields;
    for (const auto& [fname, fval] : e.fields)
        fields.emplace_back(fname, buildExpr(*fval));
    // structName is not carried by StructInitExpr — the codegen resolves it
    // from the variable declaration context.
    auto n = std::make_unique<HIRStructInit>("", std::move(fields),
                                             BFType::makeUnknown());
    n->line = e.line; n->column = e.column;
    return n;
}

HIRExprPtr HIRBuilder::buildInterpolated(const InterpolatedStringExpr& e) {
    std::vector<HIRExprPtr> exprs;
    for (const auto& ex : e.expressions)
        exprs.push_back(buildExpr(*ex));
    auto n = std::make_unique<HIRInterpolatedString>(e.fragments, std::move(exprs));
    n->line = e.line; n->column = e.column;
    return n;
}

// ---------------------------------------------------------------------------
// Type helpers
// ---------------------------------------------------------------------------

BFType HIRBuilder::typeNodeToBFType(const TypeNode& tn) const {
    BFType resolved = sem_.typeResolver().resolve(tn);
    if (!resolved.isUnknown())
        return resolved;

    // Preserve unresolved type names (especially imported module types) so
    // downstream codegen can still map them using extern registries.
    if (tn.name == "array" && tn.typeParams.size() == 1) {
        return BFType::makeArray(typeNodeToBFType(*tn.typeParams[0]));
    }
    if (tn.name == "map" && tn.typeParams.size() == 2) {
        return BFType::makeMap(typeNodeToBFType(*tn.typeParams[0]), typeNodeToBFType(*tn.typeParams[1]));
    }

    BFType unknown = BFType::makeUnknown();
    unknown.name = tn.name;
    return unknown;
}

BFType HIRBuilder::inferBinaryResultType(const std::string& op,
                                          const BFType& left,
                                          const BFType& right) const {
    // Comparison / logical → bool.
    static const std::set<std::string> kCmp = {
        "==","!=","<","<=",">",">=","&&","||","^^"};
    if (kCmp.count(op)) return BFType::makeBool();
    // Arithmetic: if either side is float, result is float.
    if (left.kind == BFTypeKind::Float || right.kind == BFTypeKind::Float)
        return BFType::makeFloat();
    if (left.kind == BFTypeKind::Int || right.kind == BFTypeKind::Int)
        return BFType::makeInt();
    // String concat / other — inherit left.
    return left.isUnknown() ? right : left;
}

BFType HIRBuilder::resolveIdentifierType(const std::string& name) const {
    // Named enum type.
    if (sem_.typeResolver().isKnownEnum(name))
        return BFType::makeEnum(name);
    // Named struct type (used as value or constructor).
    if (sem_.typeResolver().isKnownStruct(name))
        return BFType::makeStruct(name);
    // Unqualified enum variant.
    if (const std::string* e = sem_.typeResolver().enumForVariant(name))
        return BFType::makeEnum(*e);
    // Variable in scope.
    if (const auto* sym = scopes_.lookup(name))
        return sym->type;
    return BFType::makeUnknown();
}

}  // namespace bytefrost
