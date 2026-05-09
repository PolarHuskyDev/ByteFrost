#include "bytefrost/diagnostics/diagnostic_engine.h"
#include "bytefrost/semantic/bf_type.h"
#include "bytefrost/semantic/semantic_context.h"
#include "bytefrost/semantic/symbol_table.h"
#include "bytefrost/semantic/type_checker.h"
#include "bytefrost/semantic/type_resolver.h"
#include "parser/ast.h"

#include <gtest/gtest.h>
#include <sstream>

using namespace bytefrost;

// ===========================================================================
// Helper: build a minimal Program for testing
// ===========================================================================

namespace {

/// Build a FunctionDecl with the given name and an empty body.
std::unique_ptr<FunctionDecl> makeFunc(const std::string& name,
                                       const std::string& retType = "void") {
	auto fn = std::make_unique<FunctionDecl>();
	fn->name = name;
	fn->returnType = std::make_unique<TypeNode>(retType);
	fn->isExported = false;
	fn->isOverridden = false;
	fn->line = 1;
	fn->column = 1;
	return fn;
}

/// Wrap a statement into a function body.
std::unique_ptr<FunctionDecl> makeFuncWithBody(const std::string& name,
                                                StmtPtr stmt) {
	auto fn = makeFunc(name);
	fn->body.statements.push_back(std::move(stmt));
	return fn;
}

}  // namespace

// ===========================================================================
// DiagnosticEngine
// ===========================================================================

TEST(DiagnosticEngine, NoErrorsInitially) {
	DiagnosticEngine diag;
	EXPECT_FALSE(diag.hasErrors());
	EXPECT_EQ(diag.errorCount(), 0u);
}

TEST(DiagnosticEngine, RecordsErrors) {
	DiagnosticEngine diag;
	diag.error({"test.bf", 1, 1}, "something went wrong");
	EXPECT_TRUE(diag.hasErrors());
	EXPECT_EQ(diag.errorCount(), 1u);
	EXPECT_EQ(diag.all().size(), 1u);
	EXPECT_EQ(diag.all()[0].message, "something went wrong");
}

TEST(DiagnosticEngine, RecordsWarnings) {
	DiagnosticEngine diag;
	diag.warning({"test.bf", 2, 5}, "possible issue");
	EXPECT_FALSE(diag.hasErrors());
	EXPECT_TRUE(diag.hasWarnings());
	EXPECT_EQ(diag.warningCount(), 1u);
}

TEST(DiagnosticEngine, AttachesNotes) {
	DiagnosticEngine diag;
	diag.error({"test.bf", 3, 1}, "duplicate declaration 'foo'")
	    .addNote({"test.bf", 1, 1}, "first declared here");
	ASSERT_EQ(diag.all().size(), 1u);
	ASSERT_EQ(diag.all()[0].notes.size(), 1u);
	EXPECT_EQ(diag.all()[0].notes[0].message, "first declared here");
}

TEST(DiagnosticEngine, EmitsPlainText) {
	DiagnosticEngine diag;
	diag.error({"test.bf", 10, 3}, "test error");
	std::ostringstream oss;
	diag.emit(oss, /*useColor=*/false);
	const std::string out = oss.str();
	EXPECT_NE(out.find("test.bf:10:3"), std::string::npos);
	EXPECT_NE(out.find("error"), std::string::npos);
	EXPECT_NE(out.find("test error"), std::string::npos);
}

TEST(DiagnosticEngine, Clear) {
	DiagnosticEngine diag;
	diag.error("oops");
	diag.clear();
	EXPECT_FALSE(diag.hasErrors());
	EXPECT_EQ(diag.all().size(), 0u);
}

// ===========================================================================
// BFType
// ===========================================================================

TEST(BFType, PrimitiveEquality) {
	EXPECT_EQ(BFType::makeInt(),    BFType::makeInt());
	EXPECT_EQ(BFType::makeFloat(),  BFType::makeFloat());
	EXPECT_EQ(BFType::makeBool(),   BFType::makeBool());
	EXPECT_EQ(BFType::makeString(), BFType::makeString());
	EXPECT_NE(BFType::makeInt(),    BFType::makeFloat());
}

TEST(BFType, NamedTypeEquality) {
	EXPECT_EQ(BFType::makeEnum("CardRanks"),   BFType::makeEnum("CardRanks"));
	EXPECT_NE(BFType::makeEnum("CardRanks"),   BFType::makeEnum("Suit"));
	EXPECT_EQ(BFType::makeStruct("Deck"),      BFType::makeStruct("Deck"));
	EXPECT_NE(BFType::makeStruct("Deck"),      BFType::makeEnum("Deck"));
}

TEST(BFType, ArrayEquality) {
	auto a1 = BFType::makeArray(BFType::makeInt());
	auto a2 = BFType::makeArray(BFType::makeInt());
	auto a3 = BFType::makeArray(BFType::makeFloat());
	EXPECT_EQ(a1, a2);
	EXPECT_NE(a1, a3);
}

TEST(BFType, ToString) {
	EXPECT_EQ(BFType::makeInt().toString(),    "int");
	EXPECT_EQ(BFType::makeVoid().toString(),   "void");
	EXPECT_EQ(BFType::makeEnum("Suit").toString(), "Suit");
	EXPECT_EQ(BFType::makeArray(BFType::makeInt()).toString(), "array<int>");
	EXPECT_EQ(BFType::makeMap(BFType::makeInt(), BFType::makeString()).toString(),
	          "map<int, string>");
	EXPECT_EQ(BFType::makeNullable(BFType::makeStruct("User")).toString(), "User?");
}

TEST(BFType, Predicates) {
	EXPECT_TRUE(BFType::makeInt().isNumeric());
	EXPECT_TRUE(BFType::makeFloat().isNumeric());
	EXPECT_FALSE(BFType::makeBool().isNumeric());
	EXPECT_TRUE(BFType::makeEnum("X").isComparable());
	EXPECT_FALSE(BFType::makeStruct("Y").isComparable());
}

// ===========================================================================
// TypeResolver
// ===========================================================================

TEST(TypeResolver, PrimitiveTypes) {
	TypeResolver res;
	EXPECT_EQ(res.resolve(TypeNode("int")),    BFType::makeInt());
	EXPECT_EQ(res.resolve(TypeNode("float")),  BFType::makeFloat());
	EXPECT_EQ(res.resolve(TypeNode("bool")),   BFType::makeBool());
	EXPECT_EQ(res.resolve(TypeNode("string")), BFType::makeString());
	EXPECT_EQ(res.resolve(TypeNode("void")),   BFType::makeVoid());
}

TEST(TypeResolver, UnknownTypeReturnsUnknown) {
	TypeResolver res;
	EXPECT_TRUE(res.resolve(TypeNode("SomeUndefinedType")).isUnknown());
}

TEST(TypeResolver, RegisterEnum) {
	TypeResolver res;
	res.registerEnum("CardRanks");
	EXPECT_TRUE(res.isKnownEnum("CardRanks"));
	EXPECT_EQ(res.resolve(TypeNode("CardRanks")), BFType::makeEnum("CardRanks"));
}

TEST(TypeResolver, RegisterStruct) {
	TypeResolver res;
	res.registerStruct("Deck");
	EXPECT_TRUE(res.isKnownStruct("Deck"));
	EXPECT_EQ(res.resolve(TypeNode("Deck")), BFType::makeStruct("Deck"));
}

TEST(TypeResolver, ArrayType) {
	TypeResolver res;
	TypeNode arrNode("array");
	arrNode.typeParams.push_back(std::make_unique<TypeNode>("int"));
	auto t = res.resolve(arrNode);
	EXPECT_TRUE(t.isArray());
	ASSERT_NE(t.elemType, nullptr);
	EXPECT_TRUE(t.elemType->isInt());
}

TEST(TypeResolver, MapType) {
	TypeResolver res;
	TypeNode mapNode("map");
	mapNode.typeParams.push_back(std::make_unique<TypeNode>("int"));
	mapNode.typeParams.push_back(std::make_unique<TypeNode>("string"));
	auto t = res.resolve(mapNode);
	EXPECT_TRUE(t.isMap());
	ASSERT_NE(t.keyType,   nullptr);
	ASSERT_NE(t.valueType, nullptr);
	EXPECT_TRUE(t.keyType->isInt());
	EXPECT_TRUE(t.valueType->isString());
}

// ===========================================================================
// TypeChecker
// ===========================================================================

class TypeCheckerTest : public ::testing::Test {
protected:
	TypeResolver res;
	TypeChecker  checker{res};
};

TEST_F(TypeCheckerTest, IntToFloatWidening) {
	EXPECT_TRUE(checker.isAssignable(BFType::makeInt(), BFType::makeFloat()));
}

TEST_F(TypeCheckerTest, SameTypeAssignable) {
	EXPECT_TRUE(checker.isAssignable(BFType::makeString(), BFType::makeString()));
}

TEST_F(TypeCheckerTest, CrossTypeNotAssignable) {
	EXPECT_FALSE(checker.isAssignable(BFType::makeString(), BFType::makeInt()));
	EXPECT_FALSE(checker.isAssignable(BFType::makeFloat(), BFType::makeString()));
}

TEST_F(TypeCheckerTest, ArithmeticOpsValid) {
	for (const auto& op : {"+", "-", "*", "/", "%"}) {
		EXPECT_TRUE(checker.isBinaryOpValid(op, BFType::makeInt(), BFType::makeInt()))
		    << "op = " << op;
		EXPECT_TRUE(checker.isBinaryOpValid(op, BFType::makeFloat(), BFType::makeFloat()))
		    << "op = " << op;
	}
}

TEST_F(TypeCheckerTest, ArithmeticOnStringsInvalid) {
	EXPECT_FALSE(checker.isBinaryOpValid("-", BFType::makeString(), BFType::makeString()));
}

TEST_F(TypeCheckerTest, StringConcatWithPlus) {
	EXPECT_TRUE(checker.isBinaryOpValid("+", BFType::makeString(), BFType::makeString()));
}

TEST_F(TypeCheckerTest, ComparisonResultIsBool) {
	auto result = checker.inferBinaryResult("<", BFType::makeInt(), BFType::makeInt());
	EXPECT_TRUE(result.isBool());
}

TEST_F(TypeCheckerTest, CrossEnumComparisonInvalid) {
	EXPECT_FALSE(checker.isEquatable(BFType::makeEnum("A"), BFType::makeEnum("B")));
}

TEST_F(TypeCheckerTest, SameEnumComparisonValid) {
	EXPECT_TRUE(checker.isEquatable(BFType::makeEnum("A"), BFType::makeEnum("A")));
}

TEST_F(TypeCheckerTest, LogicalOpsRequireBool) {
	EXPECT_TRUE(checker.isBinaryOpValid("&&", BFType::makeBool(), BFType::makeBool()));
	EXPECT_FALSE(checker.isBinaryOpValid("&&", BFType::makeInt(), BFType::makeBool()));
}

TEST_F(TypeCheckerTest, UnaryNegateInt) {
	EXPECT_TRUE(checker.isUnaryOpValid("-", BFType::makeInt()));
	EXPECT_EQ(checker.inferUnaryResult("-", BFType::makeInt()), BFType::makeInt());
}

TEST_F(TypeCheckerTest, UnaryNotRequiresBool) {
	EXPECT_TRUE(checker.isUnaryOpValid("!", BFType::makeBool()));
	EXPECT_FALSE(checker.isUnaryOpValid("!", BFType::makeInt()));
}

// ===========================================================================
// ScopeManager
// ===========================================================================

TEST(ScopeManager, LookupUndeclared) {
	ScopeManager scopes;
	EXPECT_EQ(scopes.lookup("x"), nullptr);
}

TEST(ScopeManager, DeclareAndLookup) {
	ScopeManager scopes;
	scopes.declare("x", {"x", BFType::makeInt()});
	const auto* sym = scopes.lookup("x");
	ASSERT_NE(sym, nullptr);
	EXPECT_EQ(sym->type, BFType::makeInt());
}

TEST(ScopeManager, InnerScopeShadows) {
	ScopeManager scopes;
	scopes.declare("x", {"x", BFType::makeInt()});
	scopes.pushScope();
	scopes.declare("x", {"x", BFType::makeFloat()});
	EXPECT_EQ(scopes.lookup("x")->type, BFType::makeFloat());
	scopes.popScope();
	EXPECT_EQ(scopes.lookup("x")->type, BFType::makeInt());
}

TEST(ScopeManager, IsDeclaredInCurrentScope) {
	ScopeManager scopes;
	scopes.declare("y", {"y", BFType::makeBool()});
	EXPECT_TRUE(scopes.isDeclaredInCurrentScope("y"));
	scopes.pushScope();
	EXPECT_FALSE(scopes.isDeclaredInCurrentScope("y"));
	scopes.popScope();
}

// ===========================================================================
// SemanticContext — full program analysis
// ===========================================================================

/// Build the minimal program structure needed for SemanticContext::analyze.
static Program makeProgram() {
	return Program{};
}

TEST(SemanticContext, EmptyProgramIsValid) {
	SemanticContext sem;
	Program prog = makeProgram();
	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
	EXPECT_FALSE(sem.diagnostics().hasErrors());
}

TEST(SemanticContext, DuplicateFunctionDetected) {
	SemanticContext sem;
	Program prog;
	prog.functions.push_back(makeFunc("main"));
	prog.functions.push_back(makeFunc("main"));
	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	EXPECT_GT(sem.diagnostics().errorCount(), 0u);
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("duplicate"), std::string::npos);
}

TEST(SemanticContext, DuplicateEnumDetected) {
	SemanticContext sem;
	Program prog;
	auto e1 = std::make_unique<EnumDecl>();
	e1->name = "Color";  e1->line = 1;
	auto e2 = std::make_unique<EnumDecl>();
	e2->name = "Color";  e2->line = 5;
	prog.enums.push_back(std::move(e1));
	prog.enums.push_back(std::move(e2));
	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, MathOverrideConflict) {
	SemanticContext sem;
	Program prog;
	auto fn = makeFunc("sin");
	fn->isOverridden = false;
	prog.functions.push_back(std::move(fn));
	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("overridden"), std::string::npos);
}

TEST(SemanticContext, MathOverrideAllowedWithKeyword) {
	SemanticContext sem;
	Program prog;
	auto fn = makeFunc("sin");
	fn->isOverridden = true;
	prog.functions.push_back(std::move(fn));
	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, UndefinedVariableDetected) {
	SemanticContext sem;
	Program prog;

	// Build:  main(): void { x; }
	auto id   = std::make_unique<IdentifierExpr>("x");
	id->line  = 2; id->column = 5;
	auto stmt = std::make_unique<ExprStmt>(std::move(id));
	stmt->line = 2;
	prog.functions.push_back(makeFuncWithBody("main", std::move(stmt)));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("undefined variable 'x'"), std::string::npos);
}

TEST(SemanticContext, ImportConflictWithStdlibMath) {
	SemanticContext sem;
	Program prog;

	auto imp = std::make_unique<ImportDecl>();
	imp->line = 1; imp->column = 1;
	imp->items.push_back({"sin", ""});  // no alias — conflicts
	prog.imports.push_back(std::move(imp));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("sin"), std::string::npos);
}

TEST(SemanticContext, ImportWithAliasIsOk) {
	SemanticContext sem;
	Program prog;

	auto imp = std::make_unique<ImportDecl>();
	imp->line = 1; imp->column = 1;
	imp->items.push_back({"sin", "mySin"});  // alias — no conflict
	prog.imports.push_back(std::move(imp));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, StructCycleDetected) {
	SemanticContext sem;
	Program prog;

	// struct A { b: B; }   struct B { a: A; }  — cycle
	auto sa = std::make_unique<StructDecl>();
	sa->name = "A"; sa->line = 1;
	StructMember mA;
	mA.kind = StructMember::FIELD;
	mA.fieldName = "b";
	mA.fieldType = std::make_unique<TypeNode>("B");
	sa->members.push_back(std::move(mA));

	auto sb = std::make_unique<StructDecl>();
	sb->name = "B"; sb->line = 5;
	StructMember mB;
	mB.kind = StructMember::FIELD;
	mB.fieldName = "a";
	mB.fieldType = std::make_unique<TypeNode>("A");
	sb->members.push_back(std::move(mB));

	prog.structs.push_back(std::move(sa));
	prog.structs.push_back(std::move(sb));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("Cyclic"), std::string::npos);
}

TEST(SemanticContext, ValidStructIsOk) {
	SemanticContext sem;
	Program prog;

	auto sd = std::make_unique<StructDecl>();
	sd->name = "Point"; sd->line = 1;
	StructMember fx, fy;
	fx.kind = StructMember::FIELD; fx.fieldName = "x";
	fx.fieldType = std::make_unique<TypeNode>("int");
	fy.kind = StructMember::FIELD; fy.fieldName = "y";
	fy.fieldType = std::make_unique<TypeNode>("int");
	sd->members.push_back(std::move(fx));
	sd->members.push_back(std::move(fy));
	prog.structs.push_back(std::move(sd));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

// Helper: build a minimal AssignStmt for `this.fieldName = <rhs-placeholder>`.
static std::unique_ptr<AssignStmt> makeThisAssign(const std::string& fieldName) {
	auto thisExpr = std::make_unique<ThisExpr>();
	auto ma = std::make_unique<MemberAccessExpr>(std::move(thisExpr), fieldName);
	auto rhs = std::make_unique<IntLiteralExpr>(0, "0");
	return std::make_unique<AssignStmt>("=", std::move(ma), std::move(rhs));
}

TEST(SemanticContext, UninitializedStructFieldNoConstructor) {
	// Outer has a field of type Inner but no constructor.
	// This is allowed — the struct can be initialized via struct-literal syntax.
	SemanticContext sem;
	Program prog;

	auto inner = std::make_unique<StructDecl>();
	inner->name = "Inner"; inner->line = 1;
	prog.structs.push_back(std::move(inner));

	auto outer = std::make_unique<StructDecl>();
	outer->name = "Outer"; outer->line = 2;
	StructMember fm;
	fm.kind = StructMember::FIELD; fm.fieldName = "inner";
	fm.fieldType = std::make_unique<TypeNode>("Inner");
	outer->members.push_back(std::move(fm));
	prog.structs.push_back(std::move(outer));

	// No constructor → no error (struct-literal init is the expected path).
	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, UninitializedStructFieldConstructorMissesField) {
	// Outer has constructor but doesn't assign this.inner → error.
	SemanticContext sem;
	Program prog;

	auto inner = std::make_unique<StructDecl>();
	inner->name = "Inner"; inner->line = 1;
	prog.structs.push_back(std::move(inner));

	auto ctor = std::make_unique<FunctionDecl>();
	ctor->name = "constructor"; ctor->line = 3;
	// constructor body is empty — doesn't assign this.inner

	auto outer = std::make_unique<StructDecl>();
	outer->name = "Outer"; outer->line = 2;
	StructMember fm;
	fm.kind = StructMember::FIELD; fm.fieldName = "inner";
	fm.fieldType = std::make_unique<TypeNode>("Inner");
	outer->members.push_back(std::move(fm));
	StructMember cm;
	cm.kind = StructMember::METHOD; cm.method = std::move(ctor);
	outer->members.push_back(std::move(cm));
	prog.structs.push_back(std::move(outer));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("does not initialize"), std::string::npos);
}

TEST(SemanticContext, InitializedStructFieldIsOk) {
	// Outer has constructor that assigns this.inner → no error.
	SemanticContext sem;
	Program prog;

	auto inner = std::make_unique<StructDecl>();
	inner->name = "Inner"; inner->line = 1;
	prog.structs.push_back(std::move(inner));

	auto ctor = std::make_unique<FunctionDecl>();
	ctor->name = "constructor"; ctor->line = 3;
	ctor->body.statements.push_back(makeThisAssign("inner"));

	auto outer = std::make_unique<StructDecl>();
	outer->name = "Outer"; outer->line = 2;
	StructMember fm;
	fm.kind = StructMember::FIELD; fm.fieldName = "inner";
	fm.fieldType = std::make_unique<TypeNode>("Inner");
	outer->members.push_back(std::move(fm));
	StructMember cm;
	cm.kind = StructMember::METHOD; cm.method = std::move(ctor);
	outer->members.push_back(std::move(cm));
	prog.structs.push_back(std::move(outer));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}
