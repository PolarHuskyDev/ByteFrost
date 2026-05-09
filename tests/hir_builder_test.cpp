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
