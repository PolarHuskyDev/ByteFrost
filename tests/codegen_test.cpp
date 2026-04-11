#include <gtest/gtest.h>

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
