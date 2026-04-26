#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "codegen/codegen.h"
#include "parser/parser.h"
#include "tokenizer/lexer.h"

// Helper: compile source to IR string.
static std::string compileToIR(const std::string& source) {
	Lexer lexer(source);
	auto tokens = lexer.tokenize();
	Parser parser(tokens);
	Program program = parser.parseProgram();
	CodeGen codegen;
	return codegen.generate(program);
}

// Helper: verify IR contains a substring.
static void assertIRContains(const std::string& ir, const std::string& expected) {
	ASSERT_NE(ir.find(expected), std::string::npos)
		<< "Expected IR to contain: " << expected << "\n\nActual IR:\n"
		<< ir;
}

// =====================
// Basic function generation
// =====================

TEST(CodeGenBasic, EmptyMainFunction) {
	std::string ir = compileToIR("main(): int { return 0; }");
	assertIRContains(ir, "define i64 @main()");
	assertIRContains(ir, "ret i64 0");
}

TEST(CodeGenBasic, FunctionWithIntParam) {
	std::string ir = compileToIR("add(a: int, b: int): int { return a + b; }");
	assertIRContains(ir, "define i64 @add(i64 %a, i64 %b)");
	assertIRContains(ir, "add i64");
}

TEST(CodeGenBasic, VoidFunction) {
	std::string ir = compileToIR("noop(): void { return; }");
	assertIRContains(ir, "define void @noop()");
	assertIRContains(ir, "ret void");
}

TEST(CodeGenBasic, FloatFunction) {
	std::string ir = compileToIR("half(x: float): float { return x / 2.0; }");
	assertIRContains(ir, "define double @half(double %x)");
	assertIRContains(ir, "fdiv double");
}

TEST(CodeGenBasic, BoolFunction) {
	std::string ir = compileToIR("isZero(x: int): bool { return x == 0; }");
	assertIRContains(ir, "define i1 @isZero(i64 %x)");
	assertIRContains(ir, "icmp eq i64");
}

// =====================
// Variables
// =====================

TEST(CodeGenVariables, IntVarDecl) {
	std::string ir = compileToIR("main(): int { x: int = 42; return x; }");
	assertIRContains(ir, "alloca i64");
	assertIRContains(ir, "store i64 42");
}

TEST(CodeGenVariables, WalrusDecl) {
	std::string ir = compileToIR("main(): int { x := 10; return x; }");
	assertIRContains(ir, "store i64 10");
}

TEST(CodeGenVariables, StringVar) {
	std::string ir = compileToIR(R"(main(): int { s: string = "hello"; return 0; })");
	assertIRContains(ir, "@str");
	assertIRContains(ir, "hello");
}

TEST(CodeGenVariables, BoolVar) {
	std::string ir = compileToIR("main(): int { b: bool = true; return 0; }");
	assertIRContains(ir, "alloca i1");
	assertIRContains(ir, "store i1 true");
}

TEST(CodeGenVariables, UninitializedVar) {
	std::string ir = compileToIR("main(): int { x: int; return x; }");
	assertIRContains(ir, "alloca i64");
	// Should be zero-initialized.
	assertIRContains(ir, "store i64 0");
}

// =====================
// Arithmetic operators
// =====================

TEST(CodeGenArithmetic, AddSubMulDivMod) {
	// Use function params to prevent LLVM constant folding.
	std::string ir = compileToIR(R"(
		compute(x: int, y: int): int {
			a: int = x + y;
			b: int = x - y;
			c: int = x * y;
			d: int = x / y;
			e: int = x % y;
			return a + b + c + d + e;
		}
	)");
	assertIRContains(ir, "add i64");
	assertIRContains(ir, "sub i64");
	assertIRContains(ir, "mul i64");
	assertIRContains(ir, "sdiv i64");
	assertIRContains(ir, "srem i64");
}

TEST(CodeGenArithmetic, FloatArithmetic) {
	std::string ir = compileToIR(R"(
		compute(x: float, y: float): float {
			return x + y;
		}
	)");
	assertIRContains(ir, "fadd double");
}

// =====================
// Comparison operators
// =====================

TEST(CodeGenComparison, AllIntComparisons) {
	std::string ir = compileToIR(R"(
		main(): int {
			a: int = 5;
			b: bool = a == 5;
			c: bool = a != 0;
			d: bool = a < 10;
			e: bool = a > 1;
			f: bool = a <= 5;
			g: bool = a >= 5;
			return 0;
		}
	)");
	assertIRContains(ir, "icmp eq i64");
	assertIRContains(ir, "icmp ne i64");
	assertIRContains(ir, "icmp slt i64");
	assertIRContains(ir, "icmp sgt i64");
	assertIRContains(ir, "icmp sle i64");
	assertIRContains(ir, "icmp sge i64");
}

// =====================
// Logical operators
// =====================

TEST(CodeGenLogical, AndOrXor) {
	std::string ir = compileToIR(R"(
		logic(a: bool, b: bool): bool {
			x: bool = a && b;
			y: bool = a || b;
			z: bool = a ^^ b;
			return z;
		}
	)");
	assertIRContains(ir, "and i1");
	assertIRContains(ir, "or i1");
	assertIRContains(ir, "xor i1");
}

TEST(CodeGenLogical, NotOperator) {
	std::string ir = compileToIR(R"(
		neg(a: bool): bool {
			return !a;
		}
	)");
	// LLVM's not is implemented as xor with true.
	assertIRContains(ir, "xor i1");
}

// =====================
// Bitwise operators
// =====================

TEST(CodeGenBitwise, AllBitwiseOps) {
	std::string ir = compileToIR(R"(
		main(): int {
			a: int = 5;
			b := a & 0xFF;
			c := a | 0x0F;
			d := a ^ 3;
			e := ~a;
			f := a << 2;
			g := a >> 1;
			return 0;
		}
	)");
	assertIRContains(ir, "and i64");
	assertIRContains(ir, "or i64");
	assertIRContains(ir, "xor i64");
	assertIRContains(ir, "shl i64");
	assertIRContains(ir, "ashr i64");
}

// =====================
// Unary operators
// =====================

TEST(CodeGenUnary, Negation) {
	std::string ir = compileToIR(R"(
		negate(x: int): int {
			return -x;
		}
	)");
	assertIRContains(ir, "sub i64 0");
}

TEST(CodeGenUnary, IncrementDecrement) {
	std::string ir = compileToIR(R"(
		main(): int {
			a: int = 0;
			a++;
			a--;
			return a;
		}
	)");
	// Post-increment adds 1.
	assertIRContains(ir, "add i64");
	// Post-decrement subtracts 1.
	assertIRContains(ir, "sub i64");
}

// =====================
// Compound assignment
// =====================

TEST(CodeGenCompound, AllCompoundOps) {
	std::string ir = compileToIR(R"(
		main(): int {
			a: int = 10;
			a += 5;
			a -= 3;
			a *= 2;
			a /= 4;
			a %= 3;
			return a;
		}
	)");
	assertIRContains(ir, "add i64");
	assertIRContains(ir, "sub i64");
	assertIRContains(ir, "mul i64");
	assertIRContains(ir, "sdiv i64");
	assertIRContains(ir, "srem i64");
}

// =====================
// Control flow
// =====================

TEST(CodeGenControlFlow, IfElse) {
	std::string ir = compileToIR(R"(
		main(): int {
			x: int = 3;
			if (x == 3) {
				return 1;
			} else {
				return 0;
			}
		}
	)");
	assertIRContains(ir, "br i1");
	assertIRContains(ir, "if.then");
	assertIRContains(ir, "if.else");
}

TEST(CodeGenControlFlow, WhileLoop) {
	std::string ir = compileToIR(R"(
		main(): int {
			x: int = 0;
			while (x < 10) {
				x++;
			}
			return x;
		}
	)");
	assertIRContains(ir, "while.cond");
	assertIRContains(ir, "while.body");
	assertIRContains(ir, "while.end");
}

TEST(CodeGenControlFlow, ForLoop) {
	std::string ir = compileToIR(R"(
		main(): int {
			sum: int = 0;
			for (i: int = 0; i < 10; i++) {
				sum += i;
			}
			return sum;
		}
	)");
	assertIRContains(ir, "for.cond");
	assertIRContains(ir, "for.body");
	assertIRContains(ir, "for.update");
	assertIRContains(ir, "for.end");
}

TEST(CodeGenControlFlow, BreakContinue) {
	std::string ir = compileToIR(R"(
		main(): int {
			x: int = 0;
			while (x < 100) {
				if (x == 50) { break; }
				x++;
			}
			return x;
		}
	)");
	assertIRContains(ir, "while.end");
	// Break should branch to while.end.
}

TEST(CodeGenControlFlow, MatchInt) {
	std::string ir = compileToIR(R"(
		main(): int {
			x: int = 2;
			match(x) {
				1 => { return 10; }
				2 => { return 20; }
				_ => { return 0; }
			}
			return 0;
		}
	)");
	assertIRContains(ir, "match.case");
	assertIRContains(ir, "icmp eq i64");
}

TEST(CodeGenControlFlow, MatchString) {
	std::string ir = compileToIR(R"(
		main(): int {
			s: string = "hello";
			match(s) {
				"hello" => { return 1; }
				"world" => { return 2; }
				_ => { return 0; }
			}
			return 0;
		}
	)");
	assertIRContains(ir, "strcmp");
}

// =====================
// Function calls
// =====================

TEST(CodeGenFunctions, RecursiveFunction) {
	std::string ir = compileToIR(R"(
		fib(n: int): int {
			if (n <= 1) { return n; }
			return fib(n - 1) + fib(n - 2);
		}
		main(): int { return fib(10); }
	)");
	assertIRContains(ir, "define i64 @fib(i64 %n)");
	assertIRContains(ir, "call i64 @fib(");
}

TEST(CodeGenFunctions, MultipleFunctions) {
	std::string ir = compileToIR(R"(
		square(x: int): int { return x * x; }
		main(): int { return square(5); }
	)");
	assertIRContains(ir, "define i64 @square(i64 %x)");
	assertIRContains(ir, "call i64 @square(");
}

// =====================
// Print built-in
// =====================

TEST(CodeGenPrint, PrintString) {
	std::string ir = compileToIR(R"(
		main(): int {
			print("Hello, World!");
			return 0;
		}
	)");
	assertIRContains(ir, "call i32 (ptr, ...) @printf");
	assertIRContains(ir, "Hello, World!");
}

TEST(CodeGenPrint, PrintInt) {
	std::string ir = compileToIR(R"(
		main(): int {
			print(42);
			return 0;
		}
	)");
	assertIRContains(ir, "call i32 (ptr, ...) @printf");
	assertIRContains(ir, "%ld");
}

TEST(CodeGenPrint, PrintVariable) {
	std::string ir = compileToIR(R"(
		main(): int {
			x: int = 99;
			print(x);
			return 0;
		}
	)");
	assertIRContains(ir, "call i32 (ptr, ...) @printf");
}

// =====================
// Edge cases / error handling
// =====================

TEST(CodeGenErrors, UndefinedVariable) {
	EXPECT_THROW(compileToIR("main(): int { return x; }"), CodeGenError);
}

TEST(CodeGenErrors, UndefinedFunction) {
	EXPECT_THROW(compileToIR("main(): int { return foo(1); }"), CodeGenError);
}

TEST(CodeGenErrors, BreakOutsideLoop) {
	EXPECT_THROW(compileToIR("main(): int { break; return 0; }"), CodeGenError);
}

// =====================
// Full .bf programs (IR generation success)
// =====================

TEST(CodeGenPrograms, HelloWorld) {
	std::string src = R"(
		main(): int {
			print("Hello, World!");
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "define i64 @main()");
	assertIRContains(ir, "Hello, World!");
}

TEST(CodeGenPrograms, Fibonacci) {
	std::string src = R"(
		fib(n: int): int {
			if (n <= 1) { return n; }
			return fib(n - 1) + fib(n - 2);
		}
		main(): int {
			for (i: int = 0; i < 10; i++) {
				print(fib(i));
			}
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "define i64 @fib(i64 %n)");
	assertIRContains(ir, "define i64 @main()");
}

TEST(CodeGenPrograms, IfElseProgram) {
	std::string src = R"(
		main(): int {
			x: int = 3;
			if (x % 2 == 0) {
				print("Even");
			} else {
				print("Odd");
			}
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "srem i64");
	assertIRContains(ir, "Even");
	assertIRContains(ir, "Odd");
}

TEST(CodeGenPrograms, WhileLoopProgram) {
	std::string src = R"(
		main(): int {
			x: int = 0;
			while (x <= 10) {
				print(x);
				x++;
			}
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "while.cond");
	assertIRContains(ir, "while.body");
}

TEST(CodeGenPrograms, MatchStringProgram) {
	std::string src = R"(
		main(): int {
			state: string = "HIGH";
			match(state) {
				"HIGH" => { print("High state matched"); }
				"LOW" => { print("Low state matched"); }
				"MIDDLE" | "INTERMEDIATE" => { print("Middle or intermediate state matched"); }
				_ => { print("None matched"); }
			}
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "strcmp");
	assertIRContains(ir, "High state matched");
}

TEST(CodeGenPrograms, OperatorsProgram) {
	std::string src = R"(
		main(): int {
			a: int = 1 + 2;
			b: int = 5 - 3;
			c: int = 2 * 4;
			d: int = 10 / 2;
			e: int = 10 % 3;
			a += 5;
			a -= 3;
			a *= 2;
			a /= 4;
			a %= 3;
			a++;
			a--;
			f := a & 0xFF;
			g := a | 0x0F;
			h := a ^ b;
			i := ~a;
			j := a << 2;
			k := a >> 1;
			return 0;
		}
	)";
	std::string ir = compileToIR(src);
	assertIRContains(ir, "define i64 @main()");
	assertIRContains(ir, "ret i64 0");
}

// ============================================================
// Struct codegen
// ============================================================

TEST(CodeGenStructs, StructTypeRegistered) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		main(): int {
			p: Point = { x: 1, y: 2 };
			return 0;
		}
	)");
	assertIRContains(ir, "%Point = type { i64, i64 }");
}

TEST(CodeGenStructs, StructInit) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		main(): int {
			p: Point = { x: 10, y: 20 };
			return 0;
		}
	)");
	assertIRContains(ir, "%Point = type { i64, i64 }");
	// Structs are heap-allocated: variable slot is a ptr, struct data is malloc'd.
	assertIRContains(ir, "alloca ptr");
	assertIRContains(ir, "call ptr @malloc");
	assertIRContains(ir, "getelementptr inbounds %Point");
}

TEST(CodeGenStructs, StructMemberAccess) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		main(): int {
			p: Point = { x: 5, y: 10 };
			return p.x;
		}
	)");
	assertIRContains(ir, "getelementptr inbounds %Point");
}

TEST(CodeGenStructs, StructMethod) {
	std::string ir = compileToIR(R"(
		struct Greeter {
			msg: string;
			greet(): void {
				print(this.msg);
			}
		}
		main(): int { return 0; }
	)");
	assertIRContains(ir, "define void @Greeter.greet(ptr %this)");
}

TEST(CodeGenStructs, StructMethodCall) {
	std::string ir = compileToIR(R"(
		struct Greeter {
			msg: string;
			greet(): void {
				print(this.msg);
			}
		}
		main(): int {
			g: Greeter = { msg: "hello" };
			g.greet();
			return 0;
		}
	)");
	assertIRContains(ir, "call void @Greeter.greet(ptr");
}

TEST(CodeGenStructs, ConstructorCall) {
	std::string ir = compileToIR(R"(
		struct Point {
			x: int;
			y: int;
			constructor(x: int, y: int) {
				this.x = x;
				this.y = y;
			}
		}
		main(): int {
			p: Point = Point(1, 2);
			return p.x;
		}
	)");
	assertIRContains(ir, "define void @Point.constructor(ptr %this, i64 %x, i64 %y)");
	assertIRContains(ir, "call void @Point.constructor(ptr");
}

TEST(CodeGenStructs, ConstructorWithWalrus) {
	std::string ir = compileToIR(R"(
		struct Vec2 {
			x: int;
			y: int;
			constructor(x: int, y: int) {
				this.x = x;
				this.y = y;
			}
		}
		main(): int {
			v := Vec2(3, 4);
			return v.x;
		}
	)");
	assertIRContains(ir, "call void @Vec2.constructor(ptr");
}

// ============================================================
// Array codegen
// ============================================================

TEST(CodeGenArrays, ArrayLiteral) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int> = [1, 2, 3];
			return 0;
		}
	)");
	assertIRContains(ir, "call ptr @malloc");
	assertIRContains(ir, "store i64 3");  // length = 3
}

TEST(CodeGenArrays, EmptyArray) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int>;
			return 0;
		}
	)");
	// length 0, capacity 8
	assertIRContains(ir, "store i64 0");
	assertIRContains(ir, "store i64 8");
}

TEST(CodeGenArrays, ArrayPush) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int>;
			nums.push(42);
			return 0;
		}
	)");
	// push involves storing the element and updating length
	assertIRContains(ir, "store i64 42");
}

TEST(CodeGenArrays, ArrayLength) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int> = [10, 20];
			return nums.length();
		}
	)");
	// length is loaded from index 1 of the array struct
	assertIRContains(ir, "getelementptr inbounds");
}

TEST(CodeGenArrays, ArrayIndex) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int> = [10, 20, 30];
			return nums[1];
		}
	)");
	assertIRContains(ir, "getelementptr inbounds");
}

// ============================================================
// Map codegen
// ============================================================

TEST(CodeGenMaps, EmptyMap) {
	std::string ir = compileToIR(R"(
		main(): int {
			codes: map<int, string>;
			return 0;
		}
	)");
	assertIRContains(ir, "call ptr @malloc");
	assertIRContains(ir, "store i64 0");  // length = 0
}

TEST(CodeGenMaps, MapSet) {
	std::string ir = compileToIR(R"(
		main(): int {
			codes: map<int, string>;
			codes[200] = "Ok";
			return 0;
		}
	)");
	// Map set generates a search loop
	assertIRContains(ir, "icmp slt");
}

TEST(CodeGenMaps, MapGet) {
	std::string ir = compileToIR(R"(
		main(): int {
			ages: map<string, int>;
			ages["Alice"] = 30;
			x := ages["Alice"];
			return 0;
		}
	)");
	assertIRContains(ir, "call ptr @malloc");
}

// ============================================================
// Interpolated string codegen
// ============================================================

TEST(CodeGenInterpolation, PrintInterpolation) {
	std::string ir = compileToIR(R"(
		main(): int {
			name: string = "World";
			print("Hello {name}!");
			return 0;
		}
	)");
	// Optimized path: direct printf for print(interpolated)
	assertIRContains(ir, "call i32 (ptr, ...) @printf");
}

TEST(CodeGenInterpolation, InterpolatedStringVar) {
	std::string ir = compileToIR(R"(
		main(): int {
			name: string = "World";
			msg: string = "Hello {name}!";
			return 0;
		}
	)");
	// General path: snprintf for measuring + malloc + snprintf
	assertIRContains(ir, "call i32 (ptr, i64, ptr, ...) @snprintf");
	assertIRContains(ir, "call ptr @malloc");
}

// ============================================================
// Struct composition codegen
// ============================================================

TEST(CodeGenComposition, NestedStructType) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		struct Circle { center: Point; radius: float; }
		main(): int {
			c: Circle = { center: { x: 1, y: 2 }, radius: 3.0 };
			return 0;
		}
	)");
	assertIRContains(ir, "%Circle = type { %Point, double }");
	assertIRContains(ir, "%Point = type { i64, i64 }");
}

TEST(CodeGenComposition, NestedStructInit) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		struct Circle { center: Point; radius: float; }
		main(): int {
			c: Circle = { center: { x: 10, y: 20 }, radius: 5.5 };
			return 0;
		}
	)");
	// Nested GEP into the embedded Point struct.
	assertIRContains(ir, "getelementptr inbounds %Circle");
	assertIRContains(ir, "getelementptr inbounds %Point");
	assertIRContains(ir, "store i64 10");
	assertIRContains(ir, "store i64 20");
	assertIRContains(ir, "store double 5.5");
}

TEST(CodeGenComposition, ChainedMemberAccess) {
	std::string ir = compileToIR(R"(
		struct Point { x: int; y: int; }
		struct Circle { center: Point; radius: float; }
		main(): int {
			c: Circle = { center: { x: 10, y: 20 }, radius: 5.5 };
			val := c.center.x;
			return 0;
		}
	)");
	// Chained access: first GEP into Circle for center, then GEP into Point for x.
	assertIRContains(ir, "center.ptr");
	assertIRContains(ir, "x.ptr");
	assertIRContains(ir, "load i64");
}

// ============================================================
// Reference counting codegen
// ============================================================

TEST(CodeGenRefCount, ArrayHasRefCountField) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int> = [1, 2, 3];
			return 0;
		}
	)");
	// Array struct is now { ptr, i64, i64, ptr } with rc field.
	assertIRContains(ir, "{ ptr, i64, i64, ptr }");
	// RC allocation: malloc(8) for the refcount.
	assertIRContains(ir, "arr.rc");
}

TEST(CodeGenRefCount, MapHasRefCountField) {
	std::string ir = compileToIR(R"(
		main(): int {
			m: map<int, string>;
			return 0;
		}
	)");
	// Map struct is now { ptr, ptr, i64, i64, ptr } with rc field.
	assertIRContains(ir, "{ ptr, ptr, i64, i64, ptr }");
	assertIRContains(ir, "map.rc");
}

TEST(CodeGenRefCount, ArrayCleanupOnReturn) {
	std::string ir = compileToIR(R"(
		main(): int {
			nums: array<int> = [1, 2, 3];
			return 0;
		}
	)");
	// Should emit rc decrement and conditional free before return.
	assertIRContains(ir, "rc.dec");
	assertIRContains(ir, "rc.iszero");
	assertIRContains(ir, "rc.free");
	assertIRContains(ir, "call void @free");
}

TEST(CodeGenRefCount, MapCleanupOnReturn) {
	std::string ir = compileToIR(R"(
		main(): int {
			m: map<string, int>;
			return 0;
		}
	)");
	// Map cleanup frees both keys and values buffers.
	assertIRContains(ir, "rc.dec");
	assertIRContains(ir, "call void @free");
}

TEST(CodeGenRefCount, MultipleContainersCleanup) {
	std::string ir = compileToIR(R"(
		main(): int {
			a: array<int> = [1, 2];
			b: array<int>;
			m: map<int, int>;
			return 0;
		}
	)");
	// Each container should have its own rc.free block.
	// Count occurrence of rc.free labels (at least 3).
	size_t count = 0;
	size_t pos = 0;
	while ((pos = ir.find("rc.free", pos)) != std::string::npos) {
		count++;
		pos += 7;
	}
	ASSERT_GE(count, 3) << "Expected at least 3 rc.free references for 3 containers";
}

// ============================================================
// Cyclic struct rejection
// ============================================================

TEST(CodeGenStructErrors, DirectSelfReferenceRejected) {
	EXPECT_THROW({
		compileToIR(R"(
			struct Node {
				value: int;
				next: Node;
			}
			main(): int { return 0; }
		)");
	}, CodeGenError);
}

TEST(CodeGenStructErrors, DirectSelfReferenceErrorMessage) {
	try {
		compileToIR(R"(
			struct Node {
				value: int;
				next: Node;
			}
			main(): int { return 0; }
		)");
		FAIL() << "Expected CodeGenError";
	} catch (const CodeGenError& e) {
		std::string msg = e.what();
		EXPECT_NE(msg.find("Cyclic struct dependency"), std::string::npos);
		EXPECT_NE(msg.find("Node -> Node"), std::string::npos);
		EXPECT_NE(msg.find("Box<T>"), std::string::npos);
	}
}

TEST(CodeGenStructErrors, IndirectCycleRejected) {
	EXPECT_THROW({
		compileToIR(R"(
			struct A { b: B; }
			struct B { a: A; }
			main(): int { return 0; }
		)");
	}, CodeGenError);
}

TEST(CodeGenStructErrors, IndirectCycleErrorMessage) {
	try {
		compileToIR(R"(
			struct A { b: B; }
			struct B { a: A; }
			main(): int { return 0; }
		)");
		FAIL() << "Expected CodeGenError";
	} catch (const CodeGenError& e) {
		std::string msg = e.what();
		EXPECT_NE(msg.find("Cyclic struct dependency"), std::string::npos);
		EXPECT_NE(msg.find("A -> B -> A"), std::string::npos);
	}
}

TEST(CodeGenStructErrors, ThreeWayCycleRejected) {
	try {
		compileToIR(R"(
			struct A { b: B; }
			struct B { c: C; }
			struct C { a: A; }
			main(): int { return 0; }
		)");
		FAIL() << "Expected CodeGenError";
	} catch (const CodeGenError& e) {
		std::string msg = e.what();
		EXPECT_NE(msg.find("A -> B -> C -> A"), std::string::npos);
	}
}

// =====================
// Object file emission
// =====================

TEST(CodeGenEmitObj, EmitsValidObjectFile) {
	std::string source = "main(): int { return 42; }";
	Lexer lexer(source);
	auto tokens = lexer.tokenize();
	Parser parser(tokens);
	Program program = parser.parseProgram();

	CodeGen codegen;
	std::string objPath = "/tmp/bytefrost_test_emit.o";
	codegen.emitObjectFile(program, objPath);

	// Verify the file exists and has content.
	std::ifstream f(objPath, std::ios::binary | std::ios::ate);
	ASSERT_TRUE(f.is_open()) << "Object file was not created";
	auto size = f.tellg();
	ASSERT_GT(size, 0) << "Object file is empty";

	// Verify ELF magic number (0x7f 'E' 'L' 'F').
	f.seekg(0);
	char magic[4];
	f.read(magic, 4);
	EXPECT_EQ(magic[0], 0x7f);
	EXPECT_EQ(magic[1], 'E');
	EXPECT_EQ(magic[2], 'L');
	EXPECT_EQ(magic[3], 'F');

	std::remove(objPath.c_str());
}
// =====================
// Math stdlib functions
// =====================

TEST(CodeGenMath, SinFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return sin(x); }
)");
	assertIRContains(ir, "llvm.sin.f64");
}

TEST(CodeGenMath, CosFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return cos(x); }
)");
	assertIRContains(ir, "llvm.cos.f64");
}

TEST(CodeGenMath, TanFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return tan(x); }
)");
	assertIRContains(ir, "declare double @tan");
	assertIRContains(ir, "call double @tan");
}

TEST(CodeGenMath, SqrtFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return sqrt(x); }
)");
	assertIRContains(ir, "llvm.sqrt.f64");
}

TEST(CodeGenMath, FloorFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return floor(x); }
)");
	assertIRContains(ir, "llvm.floor.f64");
}

TEST(CodeGenMath, CeilFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return ceil(x); }
)");
	assertIRContains(ir, "llvm.ceil.f64");
}

TEST(CodeGenMath, RoundFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return round(x); }
)");
	assertIRContains(ir, "llvm.round.f64");
}

TEST(CodeGenMath, PowFloatFloat) {
	std::string ir = compileToIR(R"(
f(b: float, e: float): float { return pow(b, e); }
)");
	assertIRContains(ir, "llvm.pow.f64");
}

TEST(CodeGenMath, LogFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return log(x); }
)");
	assertIRContains(ir, "llvm.log.f64");
}

TEST(CodeGenMath, Log2Float) {
	std::string ir = compileToIR(R"(
f(x: float): float { return log2(x); }
)");
	assertIRContains(ir, "llvm.log2.f64");
}

TEST(CodeGenMath, Log10Float) {
	std::string ir = compileToIR(R"(
f(x: float): float { return log10(x); }
)");
	assertIRContains(ir, "llvm.log10.f64");
}

TEST(CodeGenMath, ExpFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return exp(x); }
)");
	assertIRContains(ir, "llvm.exp.f64");
}

TEST(CodeGenMath, AbsFloat) {
	std::string ir = compileToIR(R"(
f(x: float): float { return abs(x); }
)");
	assertIRContains(ir, "llvm.fabs.f64");
}

TEST(CodeGenMath, AbsInt) {
	std::string ir = compileToIR(R"(
f(x: int): int { return abs(x); }
)");
	// Integer abs: negate + select
	assertIRContains(ir, "sub i64");
	assertIRContains(ir, "icmp slt i64");
	assertIRContains(ir, "select");
}

TEST(CodeGenMath, MinInt) {
	std::string ir = compileToIR(R"(
f(a: int, b: int): int { return min(a, b); }
)");
	assertIRContains(ir, "icmp slt i64");
	assertIRContains(ir, "select");
}

TEST(CodeGenMath, MaxInt) {
	std::string ir = compileToIR(R"(
f(a: int, b: int): int { return max(a, b); }
)");
	assertIRContains(ir, "icmp sgt i64");
	assertIRContains(ir, "select");
}

TEST(CodeGenMath, MinFloat) {
	std::string ir = compileToIR(R"(
f(a: float, b: float): float { return min(a, b); }
)");
	assertIRContains(ir, "llvm.minnum.f64");
}

TEST(CodeGenMath, MaxFloat) {
	std::string ir = compileToIR(R"(
f(a: float, b: float): float { return max(a, b); }
)");
	assertIRContains(ir, "llvm.maxnum.f64");
}

TEST(CodeGenMath, PowIntInt) {
	// pow() with int args should still emit float pow after int->float conversion
	std::string ir = compileToIR(R"(
f(b: int, e: int): float { return pow(b, e); }
)");
	assertIRContains(ir, "llvm.pow.f64");
	assertIRContains(ir, "sitofp");
}

// ==========================
// CodeGenStdlibConflict: functions that shadow stdlib without 'overridden'
// ==========================

TEST(CodeGenStdlibConflict, ThrowsOnSinWithoutOverridden) {
	// Defining 'sin' without 'overridden' must throw a CodeGenError.
	EXPECT_THROW(
		compileToIR("sin(x: float): float { return x; }"),
		CodeGenError
	);
}

TEST(CodeGenStdlibConflict, ErrorMentionsOverridden) {
	// The error message should guide the user toward the 'overridden' keyword.
	try {
		compileToIR("sin(x: float): float { return x; }");
		FAIL() << "Expected CodeGenError";
	} catch (const CodeGenError& e) {
		EXPECT_NE(std::string(e.what()).find("overridden"), std::string::npos);
	}
}

TEST(CodeGenStdlibConflict, ThrowsOnCosWithoutOverridden) {
	EXPECT_THROW(
		compileToIR("cos(x: float): float { return x; }"),
		CodeGenError
	);
}

TEST(CodeGenStdlibConflict, ThrowsOnAbsWithoutOverridden) {
	EXPECT_THROW(
		compileToIR("abs(x: float): float { return x; }"),
		CodeGenError
	);
}

TEST(CodeGenStdlibConflict, FirstConflictReported) {
	// When multiple conflicting functions exist, the compiler should throw on
	// the first one it encounters (abs precedes sin in source order here).
	try {
		compileToIR(R"(
abs(x: float): float { return x; }
sin(x: float): float { return x; }
)");
		FAIL() << "Expected CodeGenError";
	} catch (const CodeGenError& e) {
		// Should mention 'abs', the first conflicting name encountered.
		EXPECT_NE(std::string(e.what()).find("abs"), std::string::npos);
	}
}

// ==========================
// CodeGenStdlibOverride: functions that correctly use 'overridden'
// ==========================

TEST(CodeGenStdlibOverride, OverriddenSinUsesUserImpl) {
	// 'overridden sin' should emit a user-defined function body, not llvm.sin.
	std::string ir = compileToIR("overridden sin(x: float): float { return x; }");
	assertIRContains(ir, "define double @sin");
	// No LLVM intrinsic call
	EXPECT_EQ(ir.find("llvm.sin"), std::string::npos);
}

TEST(CodeGenStdlibOverride, OverriddenTanHasNoExternDecl) {
	// When tan is overridden, the compiler should NOT emit a C extern declaration
	// for @tan — only the user-defined function body.
	std::string ir = compileToIR("overridden tan(x: float): float { return x; }");
	assertIRContains(ir, "define double @tan");
	// 'declare double @tan' is the C extern signature — must not appear when overriding
	EXPECT_EQ(ir.find("declare double @tan"), std::string::npos);
}

TEST(CodeGenStdlibOverride, OverriddenCosCallUsesUserImpl) {
	// A call to cos() should dispatch to the user's 'overridden cos', not the intrinsic.
	std::string ir = compileToIR(R"(
overridden cos(x: float): float { return x * 2.0; }
f(): float { return cos(1.0); }
)");
	assertIRContains(ir, "define double @cos");
	EXPECT_EQ(ir.find("llvm.cos"), std::string::npos);
}

TEST(CodeGenStdlibOverride, NonOverriddenMathStillUsesIntrinsic) {
	// If only tan is overridden, calls to sin() must still use the LLVM intrinsic.
	std::string ir = compileToIR(R"(
overridden tan(x: float): float { return x; }
f(x: float): float { return sin(x); }
)");
	assertIRContains(ir, "llvm.sin.f64");
}

TEST(CodeGenStdlibOverride, MultipleOverridesAllDispatchToUser) {
	// Overriding sin and cos should route both to user bodies.
	std::string ir = compileToIR(R"(
overridden sin(x: float): float { return x; }
overridden cos(x: float): float { return x; }
f(x: float): float { return sin(x) + cos(x); }
)");
	assertIRContains(ir, "define double @sin");
	assertIRContains(ir, "define double @cos");
	EXPECT_EQ(ir.find("llvm.sin"), std::string::npos);
	EXPECT_EQ(ir.find("llvm.cos"), std::string::npos);
}