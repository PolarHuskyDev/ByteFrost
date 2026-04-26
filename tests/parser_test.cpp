#include <gtest/gtest.h>

#include "parser/ast.h"
#include "parser/parser.h"
#include "tokenizer/lexer.h"

// Helper: lex + parse
static Program parse(const std::string& source) {
	Lexer lexer(source);
	auto tokens = lexer.tokenize();
	Parser parser(tokens);
	return parser.parseProgram();
}

// ============================================================
// Hello World
// ============================================================

TEST(ParserPrograms, HelloWorld) {
	auto program = parse(R"(
main(): int {
	print("Hello, World!");
	return 0;
}
)");
	ASSERT_EQ(program.functions.size(), 1u);
	ASSERT_EQ(program.structs.size(), 0u);

	auto& fn = program.functions[0];
	EXPECT_EQ(fn->name, "main");
	EXPECT_EQ(fn->params.size(), 0u);
	EXPECT_EQ(fn->returnType->name, "int");
	EXPECT_EQ(fn->body.statements.size(), 2u);

	// print("Hello, World!");
	auto* exprStmt = dynamic_cast<ExprStmt*>(fn->body.statements[0].get());
	ASSERT_NE(exprStmt, nullptr);
	auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
	ASSERT_NE(call, nullptr);
	auto* callee = dynamic_cast<IdentifierExpr*>(call->callee.get());
	ASSERT_NE(callee, nullptr);
	EXPECT_EQ(callee->name, "print");
	ASSERT_EQ(call->arguments.size(), 1u);
	auto* strArg = dynamic_cast<StringLiteralExpr*>(call->arguments[0].get());
	ASSERT_NE(strArg, nullptr);
	EXPECT_EQ(strArg->value, "Hello, World!");

	// return 0;
	auto* ret = dynamic_cast<ReturnStmt*>(fn->body.statements[1].get());
	ASSERT_NE(ret, nullptr);
	auto* retVal = dynamic_cast<IntLiteralExpr*>(ret->value.get());
	ASSERT_NE(retVal, nullptr);
	EXPECT_EQ(retVal->value, 0);
}

// ============================================================
// Function with parameters
// ============================================================

TEST(ParserPrograms, FunctionWithParams) {
	auto program = parse(R"(
fib(n: int): int {
	if (n <= 1) {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}
)");
	ASSERT_EQ(program.functions.size(), 1u);
	auto& fn = program.functions[0];
	EXPECT_EQ(fn->name, "fib");
	ASSERT_EQ(fn->params.size(), 1u);
	EXPECT_EQ(fn->params[0].name, "n");
	EXPECT_EQ(fn->params[0].type->name, "int");
	EXPECT_EQ(fn->returnType->name, "int");
	EXPECT_EQ(fn->body.statements.size(), 2u);

	// if statement
	auto* ifStmt = dynamic_cast<IfStmt*>(fn->body.statements[0].get());
	ASSERT_NE(ifStmt, nullptr);
	auto* cond = dynamic_cast<BinaryExpr*>(ifStmt->condition.get());
	ASSERT_NE(cond, nullptr);
	EXPECT_EQ(cond->op, "<=");
}

// ============================================================
// Variable declarations
// ============================================================

TEST(ParserStatements, VarDeclTyped) {
	auto program = parse(R"(
main(): int {
	x: int = 3;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 2u);

	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->name, "x");
	EXPECT_EQ(decl->type->name, "int");
	EXPECT_FALSE(decl->isWalrus);
	auto* init = dynamic_cast<IntLiteralExpr*>(decl->initializer.get());
	ASSERT_NE(init, nullptr);
	EXPECT_EQ(init->value, 3);
}

TEST(ParserStatements, VarDeclWalrus) {
	auto program = parse(R"(
main(): int {
	f := a & 0xFF;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->name, "f");
	EXPECT_TRUE(decl->isWalrus);
	EXPECT_EQ(decl->type, nullptr);
}

TEST(ParserStatements, VarDeclNoInit) {
	auto program = parse(R"(
main(): int {
	squared: array<int>;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->name, "squared");
	EXPECT_EQ(decl->type->name, "array");
	EXPECT_EQ(decl->initializer, nullptr);
}

// ============================================================
// If/Else
// ============================================================

TEST(ParserStatements, IfElse) {
	auto program = parse(R"(
main(): int {
	x: int = 3;
	if (x % 2 == 0) {
		print("Even");
	} else {
		print("Odd");
	}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 3u);

	auto* ifStmt = dynamic_cast<IfStmt*>(stmts[1].get());
	ASSERT_NE(ifStmt, nullptr);
	EXPECT_NE(ifStmt->condition, nullptr);
	EXPECT_EQ(ifStmt->thenBlock.statements.size(), 1u);
	EXPECT_NE(ifStmt->elseBlock, nullptr);
	EXPECT_EQ(ifStmt->elseBlock->statements.size(), 1u);
	EXPECT_EQ(ifStmt->elseIfBlocks.size(), 0u);
}

// ============================================================
// While loop
// ============================================================

TEST(ParserStatements, WhileLoop) {
	auto program = parse(R"(
main(): int {
	x: int = 0;
	while (x <= 10) {
		print(x);
		x++;
	}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 3u);

	auto* whileStmt = dynamic_cast<WhileStmt*>(stmts[1].get());
	ASSERT_NE(whileStmt, nullptr);

	auto* cond = dynamic_cast<BinaryExpr*>(whileStmt->condition.get());
	ASSERT_NE(cond, nullptr);
	EXPECT_EQ(cond->op, "<=");

	EXPECT_EQ(whileStmt->body.statements.size(), 2u);

	// x++ should be an ExprStmt containing a UnaryExpr(postfix ++)
	auto* incrStmt = dynamic_cast<ExprStmt*>(whileStmt->body.statements[1].get());
	ASSERT_NE(incrStmt, nullptr);
	auto* incr = dynamic_cast<UnaryExpr*>(incrStmt->expression.get());
	ASSERT_NE(incr, nullptr);
	EXPECT_EQ(incr->op, "++");
	EXPECT_FALSE(incr->prefix);
}

// ============================================================
// For loop (C-style)
// ============================================================

TEST(ParserStatements, ForLoop) {
	auto program = parse(R"(
main(): int {
	for (i: int = 0 ; i < 10 ; i++) {
		print(i);
	}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 2u);

	auto* forStmt = dynamic_cast<ForStmt*>(stmts[0].get());
	ASSERT_NE(forStmt, nullptr);

	// Init: i: int = 0
	auto* init = dynamic_cast<VarDeclStmt*>(forStmt->init.get());
	ASSERT_NE(init, nullptr);
	EXPECT_EQ(init->name, "i");

	// Condition: i < 10
	auto* cond = dynamic_cast<BinaryExpr*>(forStmt->condition.get());
	ASSERT_NE(cond, nullptr);
	EXPECT_EQ(cond->op, "<");

	// Update: i++
	auto* update = dynamic_cast<UnaryExpr*>(forStmt->update.get());
	ASSERT_NE(update, nullptr);
	EXPECT_EQ(update->op, "++");

	EXPECT_EQ(forStmt->body.statements.size(), 1u);
}

// ============================================================
// Break and continue
// ============================================================

TEST(ParserStatements, BreakContinue) {
	auto program = parse(R"(
main(): int {
	x: int = 0;
	while (x <= 10) {
		if (x == 2) {
			continue;
		}
		if (x == 7) {
			break;
		}
		print(x);
	}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 3u);

	auto* whileStmt = dynamic_cast<WhileStmt*>(stmts[1].get());
	ASSERT_NE(whileStmt, nullptr);
	EXPECT_EQ(whileStmt->body.statements.size(), 3u);

	// First if contains continue
	auto* if1 = dynamic_cast<IfStmt*>(whileStmt->body.statements[0].get());
	ASSERT_NE(if1, nullptr);
	auto* cont = dynamic_cast<ContinueStmt*>(if1->thenBlock.statements[0].get());
	ASSERT_NE(cont, nullptr);

	// Second if contains break
	auto* if2 = dynamic_cast<IfStmt*>(whileStmt->body.statements[1].get());
	ASSERT_NE(if2, nullptr);
	auto* brk = dynamic_cast<BreakStmt*>(if2->thenBlock.statements[0].get());
	ASSERT_NE(brk, nullptr);
}

// ============================================================
// Match statement
// ============================================================

TEST(ParserStatements, MatchStatement) {
	auto program = parse(R"(
main(): int {
	state: string = "HIGH";
	match(state) {
		"HIGH" => {
			print("High state matched");
		}
		"LOW" => {
			print("Low state matched");
		}
		"MIDDLE" | "INTERMEDIATE" => {
			print("Middle or intermediate state matched");
		}
		_ => {
			print("None matched");
		}
	}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 3u);

	auto* matchStmt = dynamic_cast<MatchStmt*>(stmts[1].get());
	ASSERT_NE(matchStmt, nullptr);

	auto* subject = dynamic_cast<IdentifierExpr*>(matchStmt->subject.get());
	ASSERT_NE(subject, nullptr);
	EXPECT_EQ(subject->name, "state");

	ASSERT_EQ(matchStmt->cases.size(), 4u);

	// Case 1: "HIGH"
	EXPECT_FALSE(matchStmt->cases[0].isDefault);
	ASSERT_EQ(matchStmt->cases[0].patterns.size(), 1u);
	auto* p0 = dynamic_cast<StringLiteralExpr*>(matchStmt->cases[0].patterns[0].get());
	ASSERT_NE(p0, nullptr);
	EXPECT_EQ(p0->value, "HIGH");

	// Case 3: "MIDDLE" | "INTERMEDIATE" (multi-pattern)
	EXPECT_FALSE(matchStmt->cases[2].isDefault);
	ASSERT_EQ(matchStmt->cases[2].patterns.size(), 2u);

	// Case 4: default _
	EXPECT_TRUE(matchStmt->cases[3].isDefault);
}

// ============================================================
// Struct declaration
// ============================================================

TEST(ParserTopLevel, StructDeclaration) {
	auto program = parse(R"(
struct Person {
	name: string;
	age: int;

	salute(): void {
		print("Hi");
	}
}
)");
	ASSERT_EQ(program.structs.size(), 1u);
	auto& sd = program.structs[0];
	EXPECT_EQ(sd->name, "Person");
	ASSERT_EQ(sd->members.size(), 3u);

	EXPECT_EQ(sd->members[0].kind, StructMember::FIELD);
	EXPECT_EQ(sd->members[0].fieldName, "name");
	EXPECT_EQ(sd->members[0].fieldType->name, "string");

	EXPECT_EQ(sd->members[1].kind, StructMember::FIELD);
	EXPECT_EQ(sd->members[1].fieldName, "age");
	EXPECT_EQ(sd->members[1].fieldType->name, "int");

	EXPECT_EQ(sd->members[2].kind, StructMember::METHOD);
	EXPECT_EQ(sd->members[2].method->name, "salute");
	EXPECT_EQ(sd->members[2].method->returnType->name, "void");
}

TEST(ParserTopLevel, StructWithMain) {
	auto program = parse(R"(
struct Person {
	name: string;
	age: int;

	salute(): void {
		print("Hi");
	}
}

main(): int {
	p: Person = {
		name: "Peter",
		age: 21
	};
	p.salute();
	return 0;
}
)");
	ASSERT_EQ(program.structs.size(), 1u);
	ASSERT_EQ(program.functions.size(), 1u);

	auto& fn = program.functions[0];
	EXPECT_EQ(fn->name, "main");
	ASSERT_EQ(fn->body.statements.size(), 3u);

	// p: Person = { name: "Peter", age: 21 };
	auto* decl = dynamic_cast<VarDeclStmt*>(fn->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->name, "p");
	EXPECT_EQ(decl->type->name, "Person");
	auto* structInit = dynamic_cast<StructInitExpr*>(decl->initializer.get());
	ASSERT_NE(structInit, nullptr);
	ASSERT_EQ(structInit->fields.size(), 2u);
	EXPECT_EQ(structInit->fields[0].first, "name");
	EXPECT_EQ(structInit->fields[1].first, "age");

	// p.salute();
	auto* callStmt = dynamic_cast<ExprStmt*>(fn->body.statements[1].get());
	ASSERT_NE(callStmt, nullptr);
	auto* call = dynamic_cast<CallExpr*>(callStmt->expression.get());
	ASSERT_NE(call, nullptr);
	auto* memberAccess = dynamic_cast<MemberAccessExpr*>(call->callee.get());
	ASSERT_NE(memberAccess, nullptr);
	EXPECT_EQ(memberAccess->member, "salute");
}

// ============================================================
// Operators
// ============================================================

TEST(ParserExpressions, ArithmeticOperators) {
	auto program = parse(R"(
main(): int {
	a: int = 1 + 2;
	b: int = 5 - 3;
	c: int = 2 * 4;
	d: int = 10 / 2;
	e: int = 10 % 3;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 6u);

	// a: int = 1 + 2
	auto* declA = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(declA, nullptr);
	auto* binA = dynamic_cast<BinaryExpr*>(declA->initializer.get());
	ASSERT_NE(binA, nullptr);
	EXPECT_EQ(binA->op, "+");

	// e: int = 10 % 3
	auto* declE = dynamic_cast<VarDeclStmt*>(stmts[4].get());
	ASSERT_NE(declE, nullptr);
	auto* binE = dynamic_cast<BinaryExpr*>(declE->initializer.get());
	ASSERT_NE(binE, nullptr);
	EXPECT_EQ(binE->op, "%");
}

TEST(ParserExpressions, CompoundAssignment) {
	auto program = parse(R"(
main(): int {
	a: int = 0;
	a += 5;
	a -= 3;
	a *= 2;
	a /= 4;
	a %= 3;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 7u);

	auto* assign1 = dynamic_cast<AssignStmt*>(stmts[1].get());
	ASSERT_NE(assign1, nullptr);
	EXPECT_EQ(assign1->op, "+=");

	auto* assign5 = dynamic_cast<AssignStmt*>(stmts[5].get());
	ASSERT_NE(assign5, nullptr);
	EXPECT_EQ(assign5->op, "%=");
}

TEST(ParserExpressions, IncrementDecrement) {
	auto program = parse(R"(
main(): int {
	a: int = 0;
	a++;
	a--;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 4u);

	auto* incrStmt = dynamic_cast<ExprStmt*>(stmts[1].get());
	ASSERT_NE(incrStmt, nullptr);
	auto* incr = dynamic_cast<UnaryExpr*>(incrStmt->expression.get());
	ASSERT_NE(incr, nullptr);
	EXPECT_EQ(incr->op, "++");
	EXPECT_FALSE(incr->prefix);

	auto* decrStmt = dynamic_cast<ExprStmt*>(stmts[2].get());
	ASSERT_NE(decrStmt, nullptr);
	auto* decr = dynamic_cast<UnaryExpr*>(decrStmt->expression.get());
	ASSERT_NE(decr, nullptr);
	EXPECT_EQ(decr->op, "--");
	EXPECT_FALSE(decr->prefix);
}

TEST(ParserExpressions, LogicalOperators) {
	auto program = parse(R"(
main(): int {
	if (a > 0 && b > 0) {}
	if (a > 0 || b > 0) {}
	if (!true) {}
	if (a > 0 ^^ b > 0) {}
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 5u);

	// && 
	auto* ifAnd = dynamic_cast<IfStmt*>(stmts[0].get());
	ASSERT_NE(ifAnd, nullptr);
	auto* binAnd = dynamic_cast<BinaryExpr*>(ifAnd->condition.get());
	ASSERT_NE(binAnd, nullptr);
	EXPECT_EQ(binAnd->op, "&&");

	// ||
	auto* ifOr = dynamic_cast<IfStmt*>(stmts[1].get());
	ASSERT_NE(ifOr, nullptr);
	auto* binOr = dynamic_cast<BinaryExpr*>(ifOr->condition.get());
	ASSERT_NE(binOr, nullptr);
	EXPECT_EQ(binOr->op, "||");

	// !
	auto* ifNot = dynamic_cast<IfStmt*>(stmts[2].get());
	ASSERT_NE(ifNot, nullptr);
	auto* unNot = dynamic_cast<UnaryExpr*>(ifNot->condition.get());
	ASSERT_NE(unNot, nullptr);
	EXPECT_EQ(unNot->op, "!");
	EXPECT_TRUE(unNot->prefix);

	// ^^
	auto* ifXor = dynamic_cast<IfStmt*>(stmts[3].get());
	ASSERT_NE(ifXor, nullptr);
	auto* binXor = dynamic_cast<BinaryExpr*>(ifXor->condition.get());
	ASSERT_NE(binXor, nullptr);
	EXPECT_EQ(binXor->op, "^^");
}

TEST(ParserExpressions, BitwiseOperators) {
	auto program = parse(R"(
main(): int {
	f := a & 0xFF;
	g := a | 0x0F;
	h := a ^ b;
	i := ~a;
	j := a << 2;
	k := a >> 1;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 7u);

	auto* declF = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(declF, nullptr);
	auto* binF = dynamic_cast<BinaryExpr*>(declF->initializer.get());
	ASSERT_NE(binF, nullptr);
	EXPECT_EQ(binF->op, "&");

	auto* declI = dynamic_cast<VarDeclStmt*>(stmts[3].get());
	ASSERT_NE(declI, nullptr);
	auto* unI = dynamic_cast<UnaryExpr*>(declI->initializer.get());
	ASSERT_NE(unI, nullptr);
	EXPECT_EQ(unI->op, "~");

	auto* declJ = dynamic_cast<VarDeclStmt*>(stmts[4].get());
	ASSERT_NE(declJ, nullptr);
	auto* binJ = dynamic_cast<BinaryExpr*>(declJ->initializer.get());
	ASSERT_NE(binJ, nullptr);
	EXPECT_EQ(binJ->op, "<<");
}

// ============================================================
// Containers
// ============================================================

TEST(ParserPrograms, ArrayDeclarationWithInit) {
	auto program = parse(R"(
main(): int {
	list: array<int> = [0, 1, 2, 3, 4, 5];
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	ASSERT_EQ(stmts.size(), 2u);

	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->name, "list");
	EXPECT_EQ(decl->type->name, "array");
	ASSERT_EQ(decl->type->typeParams.size(), 1u);
	EXPECT_EQ(decl->type->typeParams[0]->name, "int");

	auto* arr = dynamic_cast<ArrayLiteralExpr*>(decl->initializer.get());
	ASSERT_NE(arr, nullptr);
	ASSERT_EQ(arr->elements.size(), 6u);
}

TEST(ParserPrograms, MapDeclaration) {
	auto program = parse(R"(
main(): int {
	httpCodes: map<int, string>;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->type->name, "map");
	ASSERT_EQ(decl->type->typeParams.size(), 2u);
	EXPECT_EQ(decl->type->typeParams[0]->name, "int");
	EXPECT_EQ(decl->type->typeParams[1]->name, "string");
}

TEST(ParserPrograms, IndexAccess) {
	auto program = parse(R"(
main(): int {
	a: int = list[0];
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(decl, nullptr);
	auto* idx = dynamic_cast<IndexExpr*>(decl->initializer.get());
	ASSERT_NE(idx, nullptr);
	auto* obj = dynamic_cast<IdentifierExpr*>(idx->object.get());
	ASSERT_NE(obj, nullptr);
	EXPECT_EQ(obj->name, "list");
	auto* index = dynamic_cast<IntLiteralExpr*>(idx->index.get());
	ASSERT_NE(index, nullptr);
	EXPECT_EQ(index->value, 0);
}

// ============================================================
// Full .bf file examples
// ============================================================

TEST(ParserPrograms, FibProgram) {
	auto program = parse(R"(
fib(n: int): int {
	if (n <= 1) {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}

main(): int {
	for (i: int = 0 ; i < 10 ; i++) {
		print(fib(i));
	}
	return 0;
}
)");
	ASSERT_EQ(program.functions.size(), 2u);
	EXPECT_EQ(program.functions[0]->name, "fib");
	EXPECT_EQ(program.functions[1]->name, "main");
}

TEST(ParserPrograms, OperatorsProgram) {
	auto program = parse(R"(
main(): int {
	a: int = 1 + 2;
	b: int = 5 - 3;
	c: int = 2 * 4;
	d: int = 10 / 2;
	e: int = 10 % 3;

	if (a == 3) {}
	if (a != 0) {}
	if (a < 5) {}
	if (a > 1) {}
	if (a <= 3) {}
	if (a >= 3) {}

	if (a > 0 && b > 0) {}
	if (a > 0 || b > 0) {}
	if (!true) {}
	if (a > 0 ^^ b > 0) {}

	f := a & 0xFF;
	g := a | 0x0F;
	h := a ^ b;
	i := ~a;
	j := a << 2;
	k := a >> 1;

	a += 5;
	a -= 3;
	a *= 2;
	a /= 4;
	a %= 3;

	a++;
	a--;

	return 0;
}
)");
	ASSERT_EQ(program.functions.size(), 1u);
	// All statements parsed without error
	EXPECT_GE(program.functions[0]->body.statements.size(), 25u);
}

TEST(ParserPrograms, ContainersProgram) {
	auto program = parse(R"(
main(): int {
	list: array<int> = [0, 1, 2, 3, 4, 5];
	squared: array<int>;
	for (i: int = 0 ; i < list.length() ; i++) {
		squared.push(list[i] * list[i]);
	}

	print(list);
	print(squared);

	httpCodes: map<int, string>;
	httpCodes[200] = "Ok";
	httpCodes[201] = "Created";
	httpCodes[404] = "Not Found";

	print(httpCodes);

	return 0;
}
)");
	ASSERT_EQ(program.functions.size(), 1u);
	auto& stmts = program.functions[0]->body.statements;
	EXPECT_GE(stmts.size(), 10u);
}

// ============================================================
// Expression precedence
// ============================================================

TEST(ParserExpressions, Precedence) {
	// 1 + 2 * 3 should be 1 + (2 * 3)
	auto program = parse(R"(
main(): int {
	x: int = 1 + 2 * 3;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* add = dynamic_cast<BinaryExpr*>(decl->initializer.get());
	ASSERT_NE(add, nullptr);
	EXPECT_EQ(add->op, "+");
	auto* mul = dynamic_cast<BinaryExpr*>(add->right.get());
	ASSERT_NE(mul, nullptr);
	EXPECT_EQ(mul->op, "*");
}

TEST(ParserExpressions, ParenthesizedExpression) {
	// (1 + 2) * 3 should be (1 + 2) * 3
	auto program = parse(R"(
main(): int {
	x: int = (1 + 2) * 3;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* mul = dynamic_cast<BinaryExpr*>(decl->initializer.get());
	ASSERT_NE(mul, nullptr);
	EXPECT_EQ(mul->op, "*");
	auto* add = dynamic_cast<BinaryExpr*>(mul->left.get());
	ASSERT_NE(add, nullptr);
	EXPECT_EQ(add->op, "+");
}

TEST(ParserExpressions, MemberAndCall) {
	auto program = parse(R"(
main(): int {
	p.salute();
	return 0;
}
)");
	auto* stmt = dynamic_cast<ExprStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(stmt, nullptr);
	auto* call = dynamic_cast<CallExpr*>(stmt->expression.get());
	ASSERT_NE(call, nullptr);
	auto* member = dynamic_cast<MemberAccessExpr*>(call->callee.get());
	ASSERT_NE(member, nullptr);
	EXPECT_EQ(member->member, "salute");
}

TEST(ParserExpressions, ChainedIndex) {
	auto program = parse(R"(
main(): int {
	x: int = list[i] * list[i];
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* mul = dynamic_cast<BinaryExpr*>(decl->initializer.get());
	ASSERT_NE(mul, nullptr);
	EXPECT_EQ(mul->op, "*");
	auto* left = dynamic_cast<IndexExpr*>(mul->left.get());
	ASSERT_NE(left, nullptr);
	auto* right = dynamic_cast<IndexExpr*>(mul->right.get());
	ASSERT_NE(right, nullptr);
}

// ============================================================
// Error handling
// ============================================================

TEST(ParserErrors, MissingSemicolon) {
	EXPECT_THROW(parse("main(): int { return 0 }"), ParseError);
}

TEST(ParserErrors, MissingClosingBrace) {
	EXPECT_THROW(parse("main(): int { return 0;"), ParseError);
}

TEST(ParserErrors, MissingReturnType) {
	EXPECT_THROW(parse("main() { return 0; }"), ParseError);
}

TEST(ParserErrors, InvalidExpression) {
	EXPECT_THROW(parse("main(): int { x: int = ; }"), ParseError);
}

// ============================================================
// This expression
// ============================================================

TEST(ParserExpressions, ThisExpression) {
	auto program = parse(R"(
struct Foo {
	val: int;
	get(): int {
		return this.val;
	}
}
)");
	ASSERT_EQ(program.structs.size(), 1u);
	auto& method = program.structs[0]->members[1].method;
	ASSERT_EQ(method->body.statements.size(), 1u);
	auto* ret = dynamic_cast<ReturnStmt*>(method->body.statements[0].get());
	ASSERT_NE(ret, nullptr);
	auto* member = dynamic_cast<MemberAccessExpr*>(ret->value.get());
	ASSERT_NE(member, nullptr);
	auto* thisExpr = dynamic_cast<ThisExpr*>(member->object.get());
	ASSERT_NE(thisExpr, nullptr);
	EXPECT_EQ(member->member, "val");
}

// ============================================================
// Boolean literals
// ============================================================

TEST(ParserExpressions, BoolLiterals) {
	auto program = parse(R"(
main(): int {
	a: bool = true;
	b: bool = false;
	return 0;
}
)");
	auto& stmts = program.functions[0]->body.statements;
	auto* declA = dynamic_cast<VarDeclStmt*>(stmts[0].get());
	ASSERT_NE(declA, nullptr);
	auto* boolA = dynamic_cast<BoolLiteralExpr*>(declA->initializer.get());
	ASSERT_NE(boolA, nullptr);
	EXPECT_TRUE(boolA->value);

	auto* declB = dynamic_cast<VarDeclStmt*>(stmts[1].get());
	ASSERT_NE(declB, nullptr);
	auto* boolB = dynamic_cast<BoolLiteralExpr*>(declB->initializer.get());
	ASSERT_NE(boolB, nullptr);
	EXPECT_FALSE(boolB->value);
}

// ============================================================
// Multiple functions
// ============================================================

TEST(ParserTopLevel, MultipleFunctions) {
	auto program = parse(R"(
add(a: int, b: int): int {
	return a + b;
}

mul(a: int, b: int): int {
	return a * b;
}

main(): int {
	return add(1, mul(2, 3));
}
)");
	ASSERT_EQ(program.functions.size(), 3u);
	EXPECT_EQ(program.functions[0]->name, "add");
	EXPECT_EQ(program.functions[1]->name, "mul");
	EXPECT_EQ(program.functions[2]->name, "main");

	ASSERT_EQ(program.functions[0]->params.size(), 2u);
	EXPECT_EQ(program.functions[0]->params[0].name, "a");
	EXPECT_EQ(program.functions[0]->params[1].name, "b");
}

// ============================================================
// Nested expressions
// ============================================================

TEST(ParserExpressions, NestedFunctionCall) {
	auto program = parse(R"(
main(): int {
	return add(1, mul(2, 3));
}
)");
	auto* ret = dynamic_cast<ReturnStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(ret, nullptr);
	auto* call = dynamic_cast<CallExpr*>(ret->value.get());
	ASSERT_NE(call, nullptr);
	auto* callee = dynamic_cast<IdentifierExpr*>(call->callee.get());
	ASSERT_NE(callee, nullptr);
	EXPECT_EQ(callee->name, "add");
	ASSERT_EQ(call->arguments.size(), 2u);

	auto* nested = dynamic_cast<CallExpr*>(call->arguments[1].get());
	ASSERT_NE(nested, nullptr);
	auto* nestedCallee = dynamic_cast<IdentifierExpr*>(nested->callee.get());
	ASSERT_NE(nestedCallee, nullptr);
	EXPECT_EQ(nestedCallee->name, "mul");
}

TEST(ParserExpressions, UnaryNegation) {
	auto program = parse(R"(
main(): int {
	x: int = -y;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* un = dynamic_cast<UnaryExpr*>(decl->initializer.get());
	ASSERT_NE(un, nullptr);
	EXPECT_EQ(un->op, "-");
	EXPECT_TRUE(un->prefix);
}

// ============================================================
// Struct parsing
// ============================================================

TEST(ParserStructs, StructDecl) {
	auto program = parse(R"(
struct Person {
	name: string;
	age: int;
}
main(): int { return 0; }
)");
	ASSERT_EQ(program.structs.size(), 1u);
	EXPECT_EQ(program.structs[0]->name, "Person");
	// Two field members
	int fieldCount = 0;
	for (const auto& m : program.structs[0]->members) {
		if (m.kind == StructMember::FIELD) fieldCount++;
	}
	EXPECT_EQ(fieldCount, 2);
}

TEST(ParserStructs, StructWithMethod) {
	auto program = parse(R"(
struct Greeter {
	msg: string;
	greet(): void {
		print(this.msg);
	}
}
main(): int { return 0; }
)");
	ASSERT_EQ(program.structs.size(), 1u);
	int methodCount = 0;
	for (const auto& m : program.structs[0]->members) {
		if (m.kind == StructMember::METHOD) methodCount++;
	}
	EXPECT_EQ(methodCount, 1);
}

TEST(ParserStructs, StructInit) {
	auto program = parse(R"(
struct Point { x: int; y: int; }
main(): int {
	p: Point = { x: 1, y: 2 };
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* init = dynamic_cast<StructInitExpr*>(decl->initializer.get());
	ASSERT_NE(init, nullptr);
	EXPECT_EQ(init->fields.size(), 2u);
	EXPECT_EQ(init->fields[0].first, "x");
	EXPECT_EQ(init->fields[1].first, "y");
}

TEST(ParserStructs, MemberAccess) {
	auto program = parse(R"(
main(): int {
	x := p.name;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* member = dynamic_cast<MemberAccessExpr*>(decl->initializer.get());
	ASSERT_NE(member, nullptr);
	EXPECT_EQ(member->member, "name");
}

TEST(ParserStructs, MethodCall) {
	auto program = parse(R"(
main(): int {
	p.salute();
	return 0;
}
)");
	auto* exprStmt = dynamic_cast<ExprStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(exprStmt, nullptr);
	auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
	ASSERT_NE(call, nullptr);
	auto* callee = dynamic_cast<MemberAccessExpr*>(call->callee.get());
	ASSERT_NE(callee, nullptr);
	EXPECT_EQ(callee->member, "salute");
}

TEST(ParserStructs, ConstructorDecl) {
	auto program = parse(R"(
struct Point {
	x: int;
	y: int;
	constructor(x: int, y: int) {
		this.x = x;
		this.y = y;
	}
}
main(): int { return 0; }
)");
	ASSERT_EQ(program.structs.size(), 1u);
	int methodCount = 0;
	for (const auto& m : program.structs[0]->members) {
		if (m.kind == StructMember::METHOD && m.method->name == "constructor") methodCount++;
	}
	EXPECT_EQ(methodCount, 1);
}

TEST(ParserStructs, ConstructorCall) {
	auto program = parse(R"(
struct Point { x: int; y: int; constructor(x: int, y: int) { this.x = x; this.y = y; } }
main(): int {
	p: Point = Point(1, 2);
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* call = dynamic_cast<CallExpr*>(decl->initializer.get());
	ASSERT_NE(call, nullptr);
	ASSERT_EQ(call->arguments.size(), 2u);
	auto* callee = dynamic_cast<IdentifierExpr*>(call->callee.get());
	ASSERT_NE(callee, nullptr);
	EXPECT_EQ(callee->name, "Point");
}

// ============================================================
// Array parsing
// ============================================================

TEST(ParserArrays, ArrayLiteral) {
	auto program = parse(R"(
main(): int {
	list: array<int> = [1, 2, 3];
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->type->name, "array");
	ASSERT_EQ(decl->type->typeParams.size(), 1u);
	EXPECT_EQ(decl->type->typeParams[0]->name, "int");
	auto* arr = dynamic_cast<ArrayLiteralExpr*>(decl->initializer.get());
	ASSERT_NE(arr, nullptr);
	EXPECT_EQ(arr->elements.size(), 3u);
}

TEST(ParserArrays, EmptyArray) {
	auto program = parse(R"(
main(): int {
	list: array<int>;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->type->name, "array");
	EXPECT_EQ(decl->initializer, nullptr);
}

TEST(ParserArrays, IndexAccess) {
	auto program = parse(R"(
main(): int {
	x := arr[0];
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* idx = dynamic_cast<IndexExpr*>(decl->initializer.get());
	ASSERT_NE(idx, nullptr);
}

// ============================================================
// Map parsing
// ============================================================

TEST(ParserMaps, MapDeclaration) {
	auto program = parse(R"(
main(): int {
	codes: map<int, string>;
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	EXPECT_EQ(decl->type->name, "map");
	ASSERT_EQ(decl->type->typeParams.size(), 2u);
	EXPECT_EQ(decl->type->typeParams[0]->name, "int");
	EXPECT_EQ(decl->type->typeParams[1]->name, "string");
}

TEST(ParserMaps, MapIndexAssign) {
	auto program = parse(R"(
main(): int {
	codes[200] = "Ok";
	return 0;
}
)");
	auto* assign = dynamic_cast<AssignStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(assign, nullptr);
	auto* idx = dynamic_cast<IndexExpr*>(assign->target.get());
	ASSERT_NE(idx, nullptr);
}

// ============================================================
// Interpolated string parsing
// ============================================================

TEST(ParserInterpolation, BasicInterpolation) {
	auto program = parse(R"(
main(): int {
	print("Hello {name}!");
	return 0;
}
)");
	auto* exprStmt = dynamic_cast<ExprStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(exprStmt, nullptr);
	auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
	ASSERT_NE(call, nullptr);
	ASSERT_EQ(call->arguments.size(), 1u);
	auto* interp = dynamic_cast<InterpolatedStringExpr*>(call->arguments[0].get());
	ASSERT_NE(interp, nullptr);
	EXPECT_EQ(interp->fragments.size(), 2u);  // "Hello " and "!"
	EXPECT_EQ(interp->expressions.size(), 1u);
	EXPECT_EQ(interp->fragments[0], "Hello ");
	EXPECT_EQ(interp->fragments[1], "!");
}

TEST(ParserInterpolation, MultipleExpressions) {
	auto program = parse(R"(
main(): int {
	msg := "{a} and {b}";
	return 0;
}
)");
	auto* decl = dynamic_cast<VarDeclStmt*>(program.functions[0]->body.statements[0].get());
	ASSERT_NE(decl, nullptr);
	auto* interp = dynamic_cast<InterpolatedStringExpr*>(decl->initializer.get());
	ASSERT_NE(interp, nullptr);
	EXPECT_EQ(interp->fragments.size(), 3u);  // "", " and ", ""
	EXPECT_EQ(interp->expressions.size(), 2u);
}

// ============================================================
// Module system: import statements
// ============================================================

TEST(ParserImport, NamespaceImport) {
	auto program = parse("import math.trig;");
	ASSERT_EQ(program.imports.size(), 1u);
	auto& imp = *program.imports[0];
	ASSERT_EQ(imp.modulePath.size(), 2u);
	EXPECT_EQ(imp.modulePath[0], "math");
	EXPECT_EQ(imp.modulePath[1], "trig");
	EXPECT_TRUE(imp.isNamespaceImport);
	EXPECT_TRUE(imp.items.empty());
}

TEST(ParserImport, SelectiveSingleItem) {
	auto program = parse("import Foo from math;");
	ASSERT_EQ(program.imports.size(), 1u);
	auto& imp = *program.imports[0];
	ASSERT_EQ(imp.modulePath.size(), 1u);
	EXPECT_EQ(imp.modulePath[0], "math");
	EXPECT_FALSE(imp.isNamespaceImport);
	ASSERT_EQ(imp.items.size(), 1u);
	EXPECT_EQ(imp.items[0].name, "Foo");
	EXPECT_TRUE(imp.items[0].alias.empty());
}

TEST(ParserImport, SelectiveWithAlias) {
	auto program = parse("import Foo as F from math;");
	ASSERT_EQ(program.imports.size(), 1u);
	auto& imp = *program.imports[0];
	ASSERT_EQ(imp.items.size(), 1u);
	EXPECT_EQ(imp.items[0].name, "Foo");
	EXPECT_EQ(imp.items[0].alias, "F");
}

TEST(ParserImport, SelectiveMultipleItems) {
	auto program = parse("import Foo, Bar from math;");
	ASSERT_EQ(program.imports.size(), 1u);
	auto& imp = *program.imports[0];
	ASSERT_EQ(imp.items.size(), 2u);
	EXPECT_EQ(imp.items[0].name, "Foo");
	EXPECT_EQ(imp.items[1].name, "Bar");
}

TEST(ParserImport, SelectiveMultipleItemsWithAlias) {
	auto program = parse("import Foo, Bar as B from math.utils;");
	ASSERT_EQ(program.imports.size(), 1u);
	auto& imp = *program.imports[0];
	ASSERT_EQ(imp.modulePath.size(), 2u);
	EXPECT_EQ(imp.modulePath[0], "math");
	EXPECT_EQ(imp.modulePath[1], "utils");
	ASSERT_EQ(imp.items.size(), 2u);
	EXPECT_EQ(imp.items[0].name, "Foo");
	EXPECT_TRUE(imp.items[0].alias.empty());
	EXPECT_EQ(imp.items[1].name, "Bar");
	EXPECT_EQ(imp.items[1].alias, "B");
}

TEST(ParserImport, MultipleImportStatements) {
	auto program = parse(R"(
import foo.bar;
import Baz from foo.baz;
main(): int { return 0; }
)");
	ASSERT_EQ(program.imports.size(), 2u);
	EXPECT_TRUE(program.imports[0]->isNamespaceImport);
	EXPECT_FALSE(program.imports[1]->isNamespaceImport);
	ASSERT_EQ(program.functions.size(), 1u);
}

// ============================================================
// Module system: export modifier
// ============================================================

TEST(ParserExport, ExportedFunction) {
	auto program = parse("export foo(): int { return 0; }");
	ASSERT_EQ(program.functions.size(), 1u);
	EXPECT_TRUE(program.functions[0]->isExported);
	EXPECT_FALSE(program.functions[0]->isOverridden);
	EXPECT_EQ(program.functions[0]->name, "foo");
}

TEST(ParserExport, NonExportedFunction) {
	auto program = parse("foo(): int { return 0; }");
	ASSERT_EQ(program.functions.size(), 1u);
	EXPECT_FALSE(program.functions[0]->isExported);
}

TEST(ParserExport, ExportedStruct) {
	auto program = parse("export struct Point { x: int; y: int; }");
	ASSERT_EQ(program.structs.size(), 1u);
	EXPECT_TRUE(program.structs[0]->isExported);
	EXPECT_EQ(program.structs[0]->name, "Point");
}

TEST(ParserExport, NonExportedStruct) {
	auto program = parse("struct Box { w: int; }");
	ASSERT_EQ(program.structs.size(), 1u);
	EXPECT_FALSE(program.structs[0]->isExported);
}

// ============================================================
// Module system: overridden modifier
// ============================================================

TEST(ParserOverridden, OverriddenAlone) {
	// overridden without export is still valid syntax
	auto program = parse("overridden floor(x: float): float { return x; }");
	ASSERT_EQ(program.functions.size(), 1u);
	EXPECT_FALSE(program.functions[0]->isExported);
	EXPECT_TRUE(program.functions[0]->isOverridden);
	EXPECT_EQ(program.functions[0]->name, "floor");
}

TEST(ParserOverridden, ExportOverridden) {
	auto program = parse("export overridden sin(x: float): float { return x; }");
	ASSERT_EQ(program.functions.size(), 1u);
	EXPECT_TRUE(program.functions[0]->isExported);
	EXPECT_TRUE(program.functions[0]->isOverridden);
	EXPECT_EQ(program.functions[0]->name, "sin");
}

TEST(ParserOverridden, RegularFunctionNotOverridden) {
	auto program = parse("compute(x: int): int { return x; }");
	ASSERT_EQ(program.functions.size(), 1u);
	EXPECT_FALSE(program.functions[0]->isOverridden);
}
