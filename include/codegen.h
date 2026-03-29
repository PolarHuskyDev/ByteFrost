#pragma once

#include <map>
#include <stdexcept>
#include <string>

#include "ast.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

class CodeGen {
   private:
	llvm::LLVMContext context;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> module;

	// Symbol table: maps variable names → their alloca in the current function
	std::map<std::string, llvm::AllocaInst*> namedValues;

   public:
	CodeGen(const std::string& moduleName)
		: builder(context),
		  module(std::make_unique<llvm::Module>(moduleName, context)) {}

	void generate(const Program& program) {
		// Forward-declare all functions first so calls between them work
		for (const auto& func : program.functions) {
			declareFunction(func);
		}

		// Then generate bodies
		for (const auto& func : program.functions) {
			generateFunction(func);
		}
	}

	void dumpIR() const { module->print(llvm::outs(), nullptr); }

	bool verify() const {
		std::string errStr;
		llvm::raw_string_ostream errStream(errStr);
		if (llvm::verifyModule(*module, &errStream)) {
			llvm::errs() << "Module verification failed:\n" << errStr << "\n";
			return false;
		}
		return true;
	}

	llvm::Module& getModule() { return *module; }

   private:
	// ---- Type mapping ----

	llvm::Type* getLLVMType(const std::string& typeName) {
		if (typeName == "int") return llvm::Type::getInt32Ty(context);
		if (typeName == "float") return llvm::Type::getDoubleTy(context);
		if (typeName == "bool") return llvm::Type::getInt1Ty(context);
		if (typeName == "void") return llvm::Type::getVoidTy(context);
		throw std::runtime_error("Unknown type: " + typeName);
	}

	// ---- Helper: create alloca at function entry ----

	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* func,
											 const std::string& name,
											 llvm::Type* type) {
		llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(),
									 func->getEntryBlock().begin());
		return tmpBuilder.CreateAlloca(type, nullptr, name);
	}

	// ---- Built-in functions ----

	llvm::Function* getOrDeclarePrintf() {
		if (auto* f = module->getFunction("printf")) return f;

		// int printf(const char*, ...)
		auto* charPtrTy = llvm::PointerType::getUnqual(
			llvm::Type::getInt8Ty(context));
		auto* funcTy = llvm::FunctionType::get(
			llvm::Type::getInt32Ty(context), {charPtrTy}, /*isVarArg=*/true);
		return llvm::Function::Create(
			funcTy, llvm::Function::ExternalLinkage, "printf", module.get());
	}

	// ---- Function declaration & generation ----

	void declareFunction(const FunctionDecl& funcDecl) {
		std::vector<llvm::Type*> paramTypes;
		for (const auto& param : funcDecl.parameters) {
			paramTypes.push_back(getLLVMType(param.typeName));
		}

		llvm::Type* retType = getLLVMType(funcDecl.returnType);
		auto* funcType = llvm::FunctionType::get(retType, paramTypes, false);

		llvm::Function::Create(
			funcType, llvm::Function::ExternalLinkage,
			funcDecl.name, module.get());
	}

	void generateFunction(const FunctionDecl& funcDecl) {
		llvm::Function* func = module->getFunction(funcDecl.name);
		if (!func) {
			throw std::runtime_error("Function not declared: " + funcDecl.name);
		}

		llvm::BasicBlock* entryBB =
			llvm::BasicBlock::Create(context, "entry", func);
		builder.SetInsertPoint(entryBB);

		// Clear previous scope
		namedValues.clear();

		// Allocate space for parameters and store incoming args
		size_t idx = 0;
		for (auto& arg : func->args()) {
			const auto& paramDecl = funcDecl.parameters[idx];
			arg.setName(paramDecl.name);

			auto* alloca = createEntryBlockAlloca(func, paramDecl.name, arg.getType());
			builder.CreateStore(&arg, alloca);
			namedValues[paramDecl.name] = alloca;
			idx++;
		}

		// Generate body
		generateBlock(*funcDecl.body);

		// If the function returns void and the current block has no terminator, add ret void
		if (!builder.GetInsertBlock()->getTerminator()) {
			if (func->getReturnType()->isVoidTy()) {
				builder.CreateRetVoid();
			} else {
				// For non-void functions missing a return: return 0 as safety net
				builder.CreateRet(llvm::ConstantInt::get(func->getReturnType(), 0));
			}
		}

		if (llvm::verifyFunction(*func, &llvm::errs())) {
			throw std::runtime_error("Verification failed for function: " + funcDecl.name);
		}
	}

	// ---- Block / Statement codegen ----

	void generateBlock(const Block& block) {
		for (const auto& stmt : block.statements) {
			generateStmt(*stmt);
			// If we already placed a terminator (return), stop generating
			if (builder.GetInsertBlock()->getTerminator()) break;
		}
	}

	void generateStmt(const Stmt& stmt) {
		switch (stmt.kind) {
			case StmtKind::VarDecl:
				generateVarDecl(static_cast<const VarDecl&>(stmt));
				break;
			case StmtKind::IfStmt:
				generateIfStmt(static_cast<const IfStmt&>(stmt));
				break;
			case StmtKind::ReturnStmt:
				generateReturnStmt(static_cast<const ReturnStmt&>(stmt));
				break;
			case StmtKind::ExprStmt:
				generateExprStmt(static_cast<const ExprStmt&>(stmt));
				break;
			case StmtKind::Block:
				generateBlock(static_cast<const Block&>(stmt));
				break;
		}
	}

	void generateVarDecl(const VarDecl& decl) {
		llvm::Function* func = builder.GetInsertBlock()->getParent();
		llvm::Type* type = getLLVMType(decl.typeName);
		auto* alloca = createEntryBlockAlloca(func, decl.name, type);

		if (decl.initializer) {
			llvm::Value* initVal = generateExpr(*decl.initializer);
			builder.CreateStore(initVal, alloca);
		}

		namedValues[decl.name] = alloca;
	}

	void generateIfStmt(const IfStmt& ifStmt) {
		llvm::Value* condVal = generateExpr(*ifStmt.condition);

		// If the condition is an i32, convert to i1 (non-zero = true)
		if (condVal->getType()->isIntegerTy(32)) {
			condVal = builder.CreateICmpNE(
				condVal, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
				"ifcond");
		}

		llvm::Function* func = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", func);
		llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "else", func);
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont", func);

		builder.CreateCondBr(condVal, thenBB, elseBB);

		// Then
		builder.SetInsertPoint(thenBB);
		generateBlock(*ifStmt.thenBlock);
		if (!builder.GetInsertBlock()->getTerminator()) {
			builder.CreateBr(mergeBB);
		}

		// Else
		builder.SetInsertPoint(elseBB);
		if (ifStmt.elseBlock) {
			generateBlock(*ifStmt.elseBlock);
		}
		if (!builder.GetInsertBlock()->getTerminator()) {
			builder.CreateBr(mergeBB);
		}

		// Continue
		builder.SetInsertPoint(mergeBB);
	}

	void generateReturnStmt(const ReturnStmt& retStmt) {
		if (retStmt.value) {
			llvm::Value* retVal = generateExpr(*retStmt.value);
			builder.CreateRet(retVal);
		} else {
			builder.CreateRetVoid();
		}
	}

	void generateExprStmt(const ExprStmt& exprStmt) {
		generateExpr(*exprStmt.expression);
	}

	// ---- Expression codegen ----

	llvm::Value* generateExpr(const Expr& expr) {
		switch (expr.kind) {
			case ExprKind::NumberLiteral:
				return generateNumberLiteral(static_cast<const NumberLiteral&>(expr));
			case ExprKind::StringLiteral:
				return generateStringLiteral(static_cast<const StringLiteral&>(expr));
			case ExprKind::Identifier:
				return generateIdentifier(static_cast<const Identifier&>(expr));
			case ExprKind::UnaryExpr:
				return generateUnaryExpr(static_cast<const UnaryExpr&>(expr));
			case ExprKind::BinaryExpr:
				return generateBinaryExpr(static_cast<const BinaryExpr&>(expr));
			case ExprKind::CallExpr:
				return generateCallExpr(static_cast<const CallExpr&>(expr));
		}
		throw std::runtime_error("Unknown expression kind");
	}

	llvm::Value* generateNumberLiteral(const NumberLiteral& lit) {
		if (lit.value.find('.') != std::string::npos) {
			return llvm::ConstantFP::get(context, llvm::APFloat(std::stod(lit.value)));
		}
		return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
									  std::stoi(lit.value), true);
	}

	llvm::Value* generateStringLiteral(const StringLiteral& lit) {
		return builder.CreateGlobalStringPtr(lit.value, "str");
	}

	llvm::Value* generateIdentifier(const Identifier& ident) {
		auto it = namedValues.find(ident.name);
		if (it == namedValues.end()) {
			throw std::runtime_error("Unknown variable: " + ident.name);
		}
		return builder.CreateLoad(it->second->getAllocatedType(),
								  it->second, ident.name);
	}

	llvm::Value* generateUnaryExpr(const UnaryExpr& unary) {
		llvm::Value* operand = generateExpr(*unary.operand);
		if (unary.op == "-") {
			if (operand->getType()->isIntegerTy()) {
				return builder.CreateNeg(operand, "neg");
			}
			return builder.CreateFNeg(operand, "fneg");
		}
		throw std::runtime_error("Unknown unary operator: " + unary.op);
	}

	llvm::Value* generateBinaryExpr(const BinaryExpr& binary) {
		llvm::Value* left = generateExpr(*binary.left);
		llvm::Value* right = generateExpr(*binary.right);

		bool isFloat = left->getType()->isDoubleTy();

		if (binary.op == "+") {
			return isFloat ? builder.CreateFAdd(left, right, "addtmp")
						   : builder.CreateAdd(left, right, "addtmp");
		}
		if (binary.op == "-") {
			return isFloat ? builder.CreateFSub(left, right, "subtmp")
						   : builder.CreateSub(left, right, "subtmp");
		}
		if (binary.op == "*") {
			return isFloat ? builder.CreateFMul(left, right, "multmp")
						   : builder.CreateMul(left, right, "multmp");
		}
		if (binary.op == "/") {
			return isFloat ? builder.CreateFDiv(left, right, "divtmp")
						   : builder.CreateSDiv(left, right, "divtmp");
		}

		// Comparison operators — result is i1
		if (isFloat) {
			if (binary.op == "<")  return builder.CreateFCmpOLT(left, right, "cmptmp");
			if (binary.op == ">")  return builder.CreateFCmpOGT(left, right, "cmptmp");
			if (binary.op == "<=") return builder.CreateFCmpOLE(left, right, "cmptmp");
			if (binary.op == ">=") return builder.CreateFCmpOGE(left, right, "cmptmp");
			if (binary.op == "==") return builder.CreateFCmpOEQ(left, right, "cmptmp");
			if (binary.op == "!=") return builder.CreateFCmpONE(left, right, "cmptmp");
		} else {
			if (binary.op == "<")  return builder.CreateICmpSLT(left, right, "cmptmp");
			if (binary.op == ">")  return builder.CreateICmpSGT(left, right, "cmptmp");
			if (binary.op == "<=") return builder.CreateICmpSLE(left, right, "cmptmp");
			if (binary.op == ">=") return builder.CreateICmpSGE(left, right, "cmptmp");
			if (binary.op == "==") return builder.CreateICmpEQ(left, right, "cmptmp");
			if (binary.op == "!=") return builder.CreateICmpNE(left, right, "cmptmp");
		}

		throw std::runtime_error("Unknown binary operator: " + binary.op);
	}

	llvm::Value* generateCallExpr(const CallExpr& call) {
		// Built-in: print(expr) → printf("%d\n", expr) for int
		if (call.callee == "print") {
			return generatePrintCall(call);
		}

		llvm::Function* callee = module->getFunction(call.callee);
		if (!callee) {
			throw std::runtime_error("Unknown function: " + call.callee);
		}

		if (callee->arg_size() != call.arguments.size()) {
			throw std::runtime_error(
				"Argument count mismatch for function: " + call.callee);
		}

		std::vector<llvm::Value*> args;
		for (const auto& arg : call.arguments) {
			args.push_back(generateExpr(*arg));
		}

		if (callee->getReturnType()->isVoidTy()) {
			return builder.CreateCall(callee, args);
		}
		return builder.CreateCall(callee, args, "calltmp");
	}

	llvm::Value* generatePrintCall(const CallExpr& call) {
		if (call.arguments.empty()) {
			throw std::runtime_error("print() requires at least one argument");
		}

		llvm::Function* printfFn = getOrDeclarePrintf();
		llvm::Value* val = generateExpr(*call.arguments[0]);

		llvm::Value* formatStr;
		if (val->getType()->isIntegerTy(32)) {
			formatStr = builder.CreateGlobalStringPtr("%d\n", "fmt_int");
		} else if (val->getType()->isDoubleTy()) {
			formatStr = builder.CreateGlobalStringPtr("%f\n", "fmt_float");
		} else {
			formatStr = builder.CreateGlobalStringPtr("%s\n", "fmt_str");
		}

		return builder.CreateCall(printfFn, {formatStr, val}, "printfcall");
	}
};
