#include "bytefrost/hir/hir_builder.h"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

#include "parser/parser.h"
#include "tokenizer/lexer.h"

using namespace bytefrost;

namespace {

Program parseProgramFromSource(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parseProgram();
}

HIRProgram buildHIRFromSource(const std::string& source) {
    Program program = parseProgramFromSource(source);
    SemanticContext sem;
    if (!sem.analyze(program, "test.bf")) {
        std::ostringstream oss;
        sem.diagnostics().emit(oss, false);
        throw std::runtime_error("Semantic analysis failed: " + oss.str());
    }
    HIRBuilder builder(sem);
    return builder.build(program);
}

}  // namespace

TEST(HIRBuilderTypes, MapIndexCarriesValueType) {
    HIRProgram hir = buildHIRFromSource(R"(
main(): int {
    ages: map<string, int>;
    x := ages["Alice"];
    return 0;
}
)");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& fn = hir.functions[0];
    ASSERT_GE(fn.body.stmts.size(), 2u);

    auto* varDecl = dynamic_cast<HIRVarDecl*>(fn.body.stmts[1].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "x");
    EXPECT_EQ(varDecl->bfType.kind, BFTypeKind::Int);

    auto* idxExpr = dynamic_cast<HIRIndexExpr*>(varDecl->init.get());
    ASSERT_NE(idxExpr, nullptr);
    EXPECT_EQ(idxExpr->type.kind, BFTypeKind::Int);
}

TEST(HIRBuilderTypes, StructMemberAccessCarriesFieldType) {
    HIRProgram hir = buildHIRFromSource(R"(
struct Point { x: int; y: int; }
main(): int {
    p: Point = { x: 10, y: 20 };
    v := p.x;
    return 0;
}
)");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& fn = hir.functions[0];
    ASSERT_GE(fn.body.stmts.size(), 3u);

    auto* varDecl = dynamic_cast<HIRVarDecl*>(fn.body.stmts[1].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "v");
    EXPECT_EQ(varDecl->bfType.kind, BFTypeKind::Int);

    auto* memberExpr = dynamic_cast<HIRMemberAccess*>(varDecl->init.get());
    ASSERT_NE(memberExpr, nullptr);
    EXPECT_EQ(memberExpr->member, "x");
    EXPECT_EQ(memberExpr->type.kind, BFTypeKind::Int);
}

TEST(HIRBuilderTypes, EnumMemberAccessPreservesEnumType) {
    HIRProgram hir = buildHIRFromSource(R"(
enum Color { RED, GREEN }
main(): int {
    c: Color = Color.RED;
    b: bool = c == Color.GREEN;
    return 0;
}
)");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& fn = hir.functions[0];
    ASSERT_GE(fn.body.stmts.size(), 3u);

    auto* boolDecl = dynamic_cast<HIRVarDecl*>(fn.body.stmts[1].get());
    ASSERT_NE(boolDecl, nullptr);
    EXPECT_EQ(boolDecl->bfType.kind, BFTypeKind::Bool);

    auto* cmp = dynamic_cast<HIRBinaryExpr*>(boolDecl->init.get());
    ASSERT_NE(cmp, nullptr);
    EXPECT_EQ(cmp->op, "==");

    auto* rhs = dynamic_cast<HIRMemberAccess*>(cmp->right.get());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->member, "GREEN");
    EXPECT_EQ(rhs->type.kind, BFTypeKind::Enum);
    EXPECT_EQ(rhs->type.name, "Color");
}

// ===========================================================================
// E.1 — isCompileTimeConstant propagation
// ===========================================================================

TEST(HIRBuilderTypes, LiteralsAreCompileTimeConstant) {
    // All literal kinds must carry isCompileTimeConstant = true.
    HIRProgram hir = buildHIRFromSource(R"(
main(): int {
    a := 42;
    b := 3.14;
    c := true;
    d := "hello";
    return 0;
}
)");
    const auto& stmts = hir.functions[0].body.stmts;
    for (size_t i = 0; i < 4u; ++i) {
        auto* vd = dynamic_cast<HIRVarDecl*>(stmts[i].get());
        ASSERT_NE(vd, nullptr) << "stmt " << i << " is not HIRVarDecl";
        ASSERT_NE(vd->init, nullptr) << "stmt " << i << " has no init";
        EXPECT_TRUE(vd->init->isCompileTimeConstant) << "stmt " << i << " literal is not const";
    }
}

TEST(HIRBuilderTypes, ConstVarDecl_FlagPropagated) {
    // E.2: const declaration must set isConstant = true on HIRVarDecl.
    HIRProgram hir = buildHIRFromSource(R"(
main(): int {
    const MAX: int = 100;
    return 0;
}
)");
    const auto& stmts = hir.functions[0].body.stmts;
    auto* vd = dynamic_cast<HIRVarDecl*>(stmts[0].get());
    ASSERT_NE(vd, nullptr);
    EXPECT_TRUE(vd->isConstant);
    ASSERT_NE(vd->init, nullptr);
    EXPECT_TRUE(vd->init->isCompileTimeConstant);
}

TEST(HIRBuilderTypes, NonConstVarDecl_FlagFalse) {
    // Regular (non-const) variable must have isConstant = false.
    HIRProgram hir = buildHIRFromSource(R"(
main(): int {
    x := 5;
    return 0;
}
)");
    auto* vd = dynamic_cast<HIRVarDecl*>(hir.functions[0].body.stmts[0].get());
    ASSERT_NE(vd, nullptr);
    EXPECT_FALSE(vd->isConstant);
}

TEST(HIRBuilderTypes, BinaryExprConstPropagation) {
    // E.1: binary of two compile-time constants is itself a constant.
    HIRProgram hir = buildHIRFromSource(R"(
main(): int {
    x := 1 + 2;
    return 0;
}
)");
    auto* vd  = dynamic_cast<HIRVarDecl*>(hir.functions[0].body.stmts[0].get());
    ASSERT_NE(vd, nullptr);
    auto* bin = dynamic_cast<HIRBinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_TRUE(bin->left->isCompileTimeConstant);
    EXPECT_TRUE(bin->right->isCompileTimeConstant);
    EXPECT_TRUE(bin->isCompileTimeConstant);
}

// ===========================================================================
// E.3 — HIRStructField isReadonly / isConstant flags
// ===========================================================================

TEST(HIRBuilderTypes, StructField_ReadonlyFlagPropagated) {
    // E.3: readonly struct fields must have isReadonly = true in HIR.
    HIRProgram hir = buildHIRFromSource(R"(
struct User {
    readonly age: int;
    name: string;
}
main(): int { return 0; }
)");
    ASSERT_EQ(hir.structs.size(), 1u);
    const auto& s = hir.structs[0];
    ASSERT_EQ(s.fields.size(), 2u);

    const HIRStructField* ageField = nullptr;
    const HIRStructField* nameField = nullptr;
    for (const auto& f : s.fields) {
        if (f.name == "age")  ageField  = &f;
        if (f.name == "name") nameField = &f;
    }
    ASSERT_NE(ageField,  nullptr);
    ASSERT_NE(nameField, nullptr);
    EXPECT_TRUE(ageField->isReadonly);
    EXPECT_FALSE(nameField->isReadonly);
}

TEST(HIRBuilderTypes, StructField_ConstFlagPropagated) {
    // E.3: const struct fields must have isConstant = true in HIR.
    HIRProgram hir = buildHIRFromSource(R"(
struct Config {
    const maxSize: int;
    threshold: float;
}
main(): int { return 0; }
)");
    ASSERT_EQ(hir.structs.size(), 1u);
    const auto& s = hir.structs[0];
    ASSERT_EQ(s.fields.size(), 2u);

    const HIRStructField* maxField = nullptr;
    const HIRStructField* thrField = nullptr;
    for (const auto& f : s.fields) {
        if (f.name == "maxSize")   maxField = &f;
        if (f.name == "threshold") thrField = &f;
    }
    ASSERT_NE(maxField, nullptr);
    ASSERT_NE(thrField, nullptr);
    EXPECT_TRUE(maxField->isConstant);
    EXPECT_FALSE(thrField->isConstant);
}

