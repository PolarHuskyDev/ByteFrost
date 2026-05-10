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
	auto rhs = std::make_unique<StructInitExpr>(
		std::vector<std::pair<std::string, ExprPtr>>{});
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

TEST(SemanticContext, ConstCannotBeReassigned) {
	SemanticContext sem;
	Program prog;

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "x";
	decl->type = std::make_unique<TypeNode>("int");
	decl->initializer = std::make_unique<IntLiteralExpr>(1, "1");
	decl->isConstant = true;
	decl->line = 1;
	decl->column = 1;

	auto target = std::make_unique<IdentifierExpr>("x");
	auto value = std::make_unique<IntLiteralExpr>(2, "2");
	auto assign = std::make_unique<AssignStmt>("=", std::move(target), std::move(value));
	assign->line = 2;
	assign->column = 1;

	fn->body.statements.push_back(std::move(decl));
	fn->body.statements.push_back(std::move(assign));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot reassign constant 'x'"), std::string::npos);
}

TEST(SemanticContext, ConstDeferredInitializationOnceIsValid) {
	SemanticContext sem;
	Program prog;

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "x";
	decl->type = std::make_unique<TypeNode>("int");
	decl->isConstant = true;
	decl->line = 1;
	decl->column = 1;

	auto target = std::make_unique<IdentifierExpr>("x");
	auto value = std::make_unique<IntLiteralExpr>(5, "5");
	auto assign = std::make_unique<AssignStmt>("=", std::move(target), std::move(value));
	assign->line = 2;
	assign->column = 1;

	fn->body.statements.push_back(std::move(decl));
	fn->body.statements.push_back(std::move(assign));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, CannotAssignNullToNonNullableVarDecl) {
	SemanticContext sem;
	Program prog;

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "x";
	decl->type = std::make_unique<TypeNode>("int");
	decl->initializer = std::make_unique<NullLiteralExpr>();
	decl->line = 1;
	decl->column = 1;

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot assign null to non-nullable type 'int'"), std::string::npos);
}

TEST(SemanticContext, NullableVarDeclAcceptsNull) {
	SemanticContext sem;
	Program prog;

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "x";
	decl->type = std::make_unique<TypeNode>("int");
	decl->type->isNullable = true;
	decl->initializer = std::make_unique<NullLiteralExpr>();
	decl->line = 1;
	decl->column = 1;

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, ForInRangeMustBeIterable) {
	SemanticContext sem;
	Program prog;

	auto fn = makeFunc("main");
	auto nDecl = std::make_unique<VarDeclStmt>();
	nDecl->name = "n";
	nDecl->type = std::make_unique<TypeNode>("int");
	nDecl->initializer = std::make_unique<IntLiteralExpr>(10, "10");

	auto fi = std::make_unique<ForInStmt>();
	fi->varName = "item";
	fi->range = std::make_unique<IdentifierExpr>("n");

	fn->body.statements.push_back(std::move(nDecl));
	fn->body.statements.push_back(std::move(fi));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("for-in range must be iterable"), std::string::npos);
}

TEST(SemanticContext, ReadonlyFieldCannotBeAssigned) {
	SemanticContext sem;
	Program prog;

	auto user = std::make_unique<StructDecl>();
	user->name = "User";
	StructMember age;
	age.kind = StructMember::FIELD;
	age.fieldName = "age";
	age.fieldType = std::make_unique<TypeNode>("int");
	age.isReadonly = true;
	user->members.push_back(std::move(age));
	prog.structs.push_back(std::move(user));

	auto method = makeFunc("setAge");
	auto thisExpr = std::make_unique<ThisExpr>();
	auto ageTarget = std::make_unique<MemberAccessExpr>(std::move(thisExpr), "age");
	auto ageValue = std::make_unique<IntLiteralExpr>(20, "20");
	auto assign = std::make_unique<AssignStmt>("=", std::move(ageTarget), std::move(ageValue));
	method->body.statements.push_back(std::move(assign));

	StructMember setAge;
	setAge.kind = StructMember::METHOD;
	setAge.method = std::move(method);
	prog.structs[0]->members.push_back(std::move(setAge));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot assign to readonly field 'age'"), std::string::npos);
}

TEST(SemanticContext, ReadonlyFieldCanBeAssignedInConstructor) {
	SemanticContext sem;
	Program prog;

	auto user = std::make_unique<StructDecl>();
	user->name = "User";
	StructMember age;
	age.kind = StructMember::FIELD;
	age.fieldName = "age";
	age.fieldType = std::make_unique<TypeNode>("int");
	age.isReadonly = true;
	user->members.push_back(std::move(age));

	auto ctor = makeFunc("constructor");
	auto thisExpr = std::make_unique<ThisExpr>();
	auto ageTarget = std::make_unique<MemberAccessExpr>(std::move(thisExpr), "age");
	auto ageValue = std::make_unique<IntLiteralExpr>(20, "20");
	auto assign = std::make_unique<AssignStmt>("=", std::move(ageTarget), std::move(ageValue));
	ctor->body.statements.push_back(std::move(assign));

	StructMember ctorMember;
	ctorMember.kind = StructMember::METHOD;
	ctorMember.method = std::move(ctor);
	user->members.push_back(std::move(ctorMember));

	prog.structs.push_back(std::move(user));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, ReadonlyFieldOnOtherInstanceCannotBeAssignedInConstructor) {
	SemanticContext sem;
	Program prog;

	auto user = std::make_unique<StructDecl>();
	user->name = "User";
	StructMember age;
	age.kind = StructMember::FIELD;
	age.fieldName = "age";
	age.fieldType = std::make_unique<TypeNode>("int");
	age.isReadonly = true;
	user->members.push_back(std::move(age));

	auto ctor = makeFunc("constructor");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "other";
	decl->type = std::make_unique<TypeNode>("User");
	ctor->body.statements.push_back(std::move(decl));

	auto otherExpr = std::make_unique<IdentifierExpr>("other");
	auto ageTarget = std::make_unique<MemberAccessExpr>(std::move(otherExpr), "age");
	auto ageValue = std::make_unique<IntLiteralExpr>(20, "20");
	auto assign = std::make_unique<AssignStmt>("=", std::move(ageTarget), std::move(ageValue));
	ctor->body.statements.push_back(std::move(assign));

	StructMember ctorMember;
	ctorMember.kind = StructMember::METHOD;
	ctorMember.method = std::move(ctor);
	user->members.push_back(std::move(ctorMember));

	prog.structs.push_back(std::move(user));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot assign to readonly field 'age'"), std::string::npos);
}

TEST(SemanticContext, NullSafeAccessRequiresNullableObject) {
	SemanticContext sem;
	Program prog;

	auto user = std::make_unique<StructDecl>();
	user->name = "User";
	StructMember age;
	age.kind = StructMember::FIELD;
	age.fieldName = "age";
	age.fieldType = std::make_unique<TypeNode>("int");
	user->members.push_back(std::move(age));
	prog.structs.push_back(std::move(user));

	auto fn = makeFunc("main");
	auto userDecl = std::make_unique<VarDeclStmt>();
	userDecl->name = "u";
	userDecl->type = std::make_unique<TypeNode>("User");

	auto ns = std::make_unique<NullSafeAccessExpr>(
		std::make_unique<IdentifierExpr>("u"), "age", std::make_unique<IntLiteralExpr>(0, "0"));
	auto stmt = std::make_unique<ExprStmt>(std::move(ns));

	fn->body.statements.push_back(std::move(userDecl));
	fn->body.statements.push_back(std::move(stmt));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("null-safe access requires nullable object"), std::string::npos);
}

TEST(SemanticContext, NullSafeAccessFallbackTypeMustMatchFieldType) {
	SemanticContext sem;
	Program prog;

	auto user = std::make_unique<StructDecl>();
	user->name = "User";
	StructMember age;
	age.kind = StructMember::FIELD;
	age.fieldName = "age";
	age.fieldType = std::make_unique<TypeNode>("int");
	user->members.push_back(std::move(age));
	prog.structs.push_back(std::move(user));

	auto fn = makeFunc("main");
	auto userDecl = std::make_unique<VarDeclStmt>();
	userDecl->name = "u";
	userDecl->type = std::make_unique<TypeNode>("User");
	userDecl->type->isNullable = true;

	auto ns = std::make_unique<NullSafeAccessExpr>(
		std::make_unique<IdentifierExpr>("u"), "age", std::make_unique<StringLiteralExpr>("fallback"));
	auto stmt = std::make_unique<ExprStmt>(std::move(ns));

	fn->body.statements.push_back(std::move(userDecl));
	fn->body.statements.push_back(std::move(stmt));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("null-safe fallback type"), std::string::npos);
}

// ===========================================================================
// Phase 3 C.1 — Struct literal field initialization rules
// ===========================================================================

// Helper: make a Point struct with int fields x and y (both non-nullable).
static std::unique_ptr<StructDecl> makePointStruct() {
	auto sd = std::make_unique<StructDecl>();
	sd->name = "Point";
	sd->line = 1;
	for (const auto& name : {"x", "y"}) {
		StructMember m;
		m.kind      = StructMember::FIELD;
		m.fieldName = name;
		m.fieldType = std::make_unique<TypeNode>("int");
		sd->members.push_back(std::move(m));
	}
	return sd;
}

TEST(SemanticContext, StructLiteralWithAllRequiredFields) {
	// p: Point = { x: 1, y: 2 }  — both fields present → valid
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makePointStruct());

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "p";
	decl->type = std::make_unique<TypeNode>("Point");
	decl->line = 2;
	decl->column = 1;

	std::vector<std::pair<std::string, ExprPtr>> fields;
	fields.emplace_back("x", std::make_unique<IntLiteralExpr>(1, "1"));
	fields.emplace_back("y", std::make_unique<IntLiteralExpr>(2, "2"));
	decl->initializer = std::make_unique<StructInitExpr>(std::move(fields));

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, StructLiteralMissingNonNullableField) {
	// p: Point = { x: 1 }  — y is missing → error
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makePointStruct());

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "p";
	decl->type = std::make_unique<TypeNode>("Point");
	decl->line = 2;
	decl->column = 1;

	std::vector<std::pair<std::string, ExprPtr>> fields;
	fields.emplace_back("x", std::make_unique<IntLiteralExpr>(1, "1"));
	decl->initializer = std::make_unique<StructInitExpr>(std::move(fields));

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("non-nullable field 'y'"), std::string::npos);
}

TEST(SemanticContext, StructLiteralUnknownField) {
	// p: Point = { x: 1, y: 2, z: 3 }  — z doesn't exist → error
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makePointStruct());

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "p";
	decl->type = std::make_unique<TypeNode>("Point");
	decl->line = 2;
	decl->column = 1;

	std::vector<std::pair<std::string, ExprPtr>> fields;
	fields.emplace_back("x", std::make_unique<IntLiteralExpr>(1, "1"));
	fields.emplace_back("y", std::make_unique<IntLiteralExpr>(2, "2"));
	fields.emplace_back("z", std::make_unique<IntLiteralExpr>(3, "3"));
	decl->initializer = std::make_unique<StructInitExpr>(std::move(fields));

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("unknown field 'z'"), std::string::npos);
}

TEST(SemanticContext, StructLiteralNullableFieldCanBeOmitted) {
	// Struct with a nullable field: omitting it is allowed.
	SemanticContext sem;
	Program prog;

	auto sd = std::make_unique<StructDecl>();
	sd->name = "Person";
	sd->line = 1;
	// name: string (non-nullable)
	StructMember mName;
	mName.kind      = StructMember::FIELD;
	mName.fieldName = "name";
	mName.fieldType = std::make_unique<TypeNode>("string");
	sd->members.push_back(std::move(mName));
	// nickname: string? (nullable — can be omitted)
	StructMember mNick;
	mNick.kind      = StructMember::FIELD;
	mNick.fieldName = "nickname";
	mNick.fieldType = std::make_unique<TypeNode>("string");
	mNick.fieldType->isNullable = true;
	sd->members.push_back(std::move(mNick));
	prog.structs.push_back(std::move(sd));

	auto fn = makeFunc("main");
	auto decl = std::make_unique<VarDeclStmt>();
	decl->name = "p";
	decl->type = std::make_unique<TypeNode>("Person");
	decl->line = 2;
	decl->column = 1;

	// Only provide `name`; omit nullable `nickname`.
	std::vector<std::pair<std::string, ExprPtr>> fields;
	fields.emplace_back("name", std::make_unique<StringLiteralExpr>("Alice"));
	decl->initializer = std::make_unique<StructInitExpr>(std::move(fields));

	fn->body.statements.push_back(std::move(decl));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

// ===========================================================================
// Phase 3 C.4 — Const struct fields
// ===========================================================================

TEST(SemanticContext, ConstStructFieldCannotBeNullable) {
	// struct Config { const maxSize: int?; }  — const nullable → error
	SemanticContext sem;
	Program prog;

	auto sd = std::make_unique<StructDecl>();
	sd->name = "Config";
	sd->line = 1;
	StructMember m;
	m.kind      = StructMember::FIELD;
	m.fieldName = "maxSize";
	m.fieldType = std::make_unique<TypeNode>("int");
	m.fieldType->isNullable = true;
	m.isConstant = true;
	sd->members.push_back(std::move(m));
	prog.structs.push_back(std::move(sd));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot be nullable"), std::string::npos);
}

TEST(SemanticContext, ConstStructFieldCannotBeAssigned) {
	// Assigning to a const struct field anywhere must fail.
	SemanticContext sem;
	Program prog;

	auto sd = std::make_unique<StructDecl>();
	sd->name = "Config";
	sd->line = 1;
	StructMember m;
	m.kind      = StructMember::FIELD;
	m.fieldName = "maxSize";
	m.fieldType = std::make_unique<TypeNode>("int");
	m.isConstant = true;
	sd->members.push_back(std::move(m));
	prog.structs.push_back(std::move(sd));

	// method that tries to assign the const field
	auto fn = makeFunc("setMax");
	auto thisExpr = std::make_unique<ThisExpr>();
	auto target   = std::make_unique<MemberAccessExpr>(std::move(thisExpr), "maxSize");
	auto value    = std::make_unique<IntLiteralExpr>(42, "42");
	auto assign   = std::make_unique<AssignStmt>("=", std::move(target), std::move(value));
	assign->line  = 3;
	fn->body.statements.push_back(std::move(assign));

	StructMember mm;
	mm.kind   = StructMember::METHOD;
	mm.method = std::move(fn);
	prog.structs[0]->members.push_back(std::move(mm));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot assign to const field 'maxSize'"), std::string::npos);
}

TEST(SemanticContext, ConstStructFieldBlockedEvenInConstructor) {
	// Unlike readonly, const fields cannot be set even in the constructor.
	SemanticContext sem;
	Program prog;

	auto sd = std::make_unique<StructDecl>();
	sd->name = "Config";
	sd->line = 1;
	StructMember m;
	m.kind      = StructMember::FIELD;
	m.fieldName = "maxSize";
	m.fieldType = std::make_unique<TypeNode>("int");
	m.isConstant = true;
	sd->members.push_back(std::move(m));
	prog.structs.push_back(std::move(sd));

	auto ctor = makeFunc("constructor");
	auto thisExpr = std::make_unique<ThisExpr>();
	auto target   = std::make_unique<MemberAccessExpr>(std::move(thisExpr), "maxSize");
	auto value    = std::make_unique<IntLiteralExpr>(100, "100");
	auto assign   = std::make_unique<AssignStmt>("=", std::move(target), std::move(value));
	ctor->body.statements.push_back(std::move(assign));

	StructMember cm;
	cm.kind   = StructMember::METHOD;
	cm.method = std::move(ctor);
	prog.structs[0]->members.push_back(std::move(cm));

	EXPECT_FALSE(sem.analyze(prog, "test.bf"));
	std::ostringstream oss;
	sem.diagnostics().emit(oss, false);
	EXPECT_NE(oss.str().find("cannot assign to const field 'maxSize'"), std::string::npos);
}

// ===========================================================================
// Phase 3 D.4 — Type narrowing for nullable types
// ===========================================================================

// Helper: build a nullable User? struct with an `age: int` field.
static std::unique_ptr<StructDecl> makeUserStruct() {
	auto sd = std::make_unique<StructDecl>();
	sd->name = "User";
	sd->line = 1;
	StructMember m;
	m.kind      = StructMember::FIELD;
	m.fieldName = "age";
	m.fieldType = std::make_unique<TypeNode>("int");
	sd->members.push_back(std::move(m));
	return sd;
}

TEST(SemanticContext, NullNarrowingNotEqualNullInThenBlock) {
	// if (u != null) { u.age; }  — u narrowed to User in then-block → valid
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makeUserStruct());

	auto fn = makeFunc("main");

	// u: User? (nullable)
	auto uDecl = std::make_unique<VarDeclStmt>();
	uDecl->name = "u";
	uDecl->type = std::make_unique<TypeNode>("User");
	uDecl->type->isNullable = true;
	uDecl->line = 2;

	// Condition: u != null
	auto cond = std::make_unique<BinaryExpr>(
		"!=", std::make_unique<IdentifierExpr>("u"), std::make_unique<NullLiteralExpr>());

	// then-body: u.age  (should succeed — u is narrowed to User)
	auto access = std::make_unique<MemberAccessExpr>(std::make_unique<IdentifierExpr>("u"), "age");
	auto bodyStmt = std::make_unique<ExprStmt>(std::move(access));

	auto ifStmt = std::make_unique<IfStmt>();
	ifStmt->condition = std::move(cond);
	ifStmt->thenBlock.statements.push_back(std::move(bodyStmt));
	ifStmt->line = 3;

	fn->body.statements.push_back(std::move(uDecl));
	fn->body.statements.push_back(std::move(ifStmt));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, NullNarrowingEqualNullInElseBlock) {
	// if (u == null) { } else { u.age; }  — u narrowed to User in else → valid
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makeUserStruct());

	auto fn = makeFunc("main");

	auto uDecl = std::make_unique<VarDeclStmt>();
	uDecl->name = "u";
	uDecl->type = std::make_unique<TypeNode>("User");
	uDecl->type->isNullable = true;
	uDecl->line = 2;

	// Condition: u == null
	auto cond = std::make_unique<BinaryExpr>(
		"==", std::make_unique<IdentifierExpr>("u"), std::make_unique<NullLiteralExpr>());

	// else-body: u.age (should succeed — u is narrowed to User)
	auto access   = std::make_unique<MemberAccessExpr>(std::make_unique<IdentifierExpr>("u"), "age");
	auto bodyStmt = std::make_unique<ExprStmt>(std::move(access));

	auto elseBlock = std::make_unique<Block>();
	elseBlock->statements.push_back(std::move(bodyStmt));

	auto ifStmt = std::make_unique<IfStmt>();
	ifStmt->condition = std::move(cond);
	ifStmt->elseBlock = std::move(elseBlock);
	ifStmt->line = 3;

	fn->body.statements.push_back(std::move(uDecl));
	fn->body.statements.push_back(std::move(ifStmt));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

TEST(SemanticContext, NullNarrowingTruthyCheckInThenBlock) {
	// if (u) { u.age; }  — truthy on nullable narrows u to User → valid
	SemanticContext sem;
	Program prog;
	prog.structs.push_back(makeUserStruct());

	auto fn = makeFunc("main");

	auto uDecl = std::make_unique<VarDeclStmt>();
	uDecl->name = "u";
	uDecl->type = std::make_unique<TypeNode>("User");
	uDecl->type->isNullable = true;
	uDecl->line = 2;

	// Condition: just `u` (truthy)
	auto cond = std::make_unique<IdentifierExpr>("u");
	cond->line = 3;

	auto access   = std::make_unique<MemberAccessExpr>(std::make_unique<IdentifierExpr>("u"), "age");
	auto bodyStmt = std::make_unique<ExprStmt>(std::move(access));

	auto ifStmt = std::make_unique<IfStmt>();
	ifStmt->condition = std::move(cond);
	ifStmt->thenBlock.statements.push_back(std::move(bodyStmt));
	ifStmt->line = 3;

	fn->body.statements.push_back(std::move(uDecl));
	fn->body.statements.push_back(std::move(ifStmt));
	prog.functions.push_back(std::move(fn));

	EXPECT_TRUE(sem.analyze(prog, "test.bf"));
}

