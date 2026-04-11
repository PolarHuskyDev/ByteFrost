#include "codegen/codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <sstream>

// ==========================
// Constructor
// ==========================

CodeGen::CodeGen() {
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>("bytefrost", *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

// ==========================
// Public interface
// ==========================

std::string CodeGen::generate(const Program& program) {
	declareBuiltins();

	// Generate all functions.
	for (const auto& fn : program.functions) {
		generateFunction(*fn);
	}

	// Verify the module.
	std::string verifyErr;
	llvm::raw_string_ostream verifyStream(verifyErr);
	if (llvm::verifyModule(*module, &verifyStream)) {
		throw CodeGenError("Module verification failed: " + verifyErr);
	}

	// Print the IR to a string.
	std::string irStr;
	llvm::raw_string_ostream irStream(irStr);
	module->print(irStream, nullptr);
	return irStr;
}

// ==========================
// Built-in declarations
// ==========================

void CodeGen::declareBuiltins() {
	// printf(const char* fmt, ...) -> i32
	auto printfType = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(*context),
		{llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context))},
		true  // variadic
	);
	printfFunc = llvm::Function::Create(
		printfType, llvm::Function::ExternalLinkage, "printf", module.get());

	// strcmp(const char*, const char*) -> i32
	auto strcmpType = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(*context),
		{llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)),
		 llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context))},
		false);
	strcmpFunc = llvm::Function::Create(
		strcmpType, llvm::Function::ExternalLinkage, "strcmp", module.get());
}

// ==========================
// Scope management
// ==========================

void CodeGen::pushScope() {
	scopes.push_back(Scope{});
}

void CodeGen::popScope() {
	scopes.pop_back();
}

void CodeGen::declareVariable(const std::string& name, llvm::AllocaInst* alloca, llvm::Type* type) {
	scopes.back().variables[name] = alloca;
	scopes.back().varTypes[name] = type;
}

llvm::AllocaInst* CodeGen::lookupVariable(const std::string& name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		auto found = it->variables.find(name);
		if (found != it->variables.end()) return found->second;
	}
	throw CodeGenError("Undefined variable: " + name);
}

llvm::Type* CodeGen::lookupVarType(const std::string& name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		auto found = it->varTypes.find(name);
		if (found != it->varTypes.end()) return found->second;
	}
	throw CodeGenError("Undefined variable type: " + name);
}

llvm::AllocaInst* CodeGen::createEntryBlockAlloca(llvm::Function* fn,
												  const std::string& name,
												  llvm::Type* type) {
	llvm::IRBuilder<> tmpB(&fn->getEntryBlock(), fn->getEntryBlock().begin());
	return tmpB.CreateAlloca(type, nullptr, name);
}

// ==========================
// Type mapping
// ==========================

llvm::Type* CodeGen::getLLVMType(const TypeNode& type) {
	if (type.name == "int") return llvm::Type::getInt64Ty(*context);
	if (type.name == "float") return llvm::Type::getDoubleTy(*context);
	if (type.name == "bool") return llvm::Type::getInt1Ty(*context);
	if (type.name == "char") return llvm::Type::getInt8Ty(*context);
	if (type.name == "string") return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
	if (type.name == "void") return llvm::Type::getVoidTy(*context);
	throw CodeGenError("Unsupported type: " + type.name);
}

bool CodeGen::isStringType(llvm::Type* type) const {
	return type->isPointerTy();
}

bool CodeGen::isFloatType(llvm::Type* type) const {
	return type->isDoubleTy() || type->isFloatTy();
}

// ==========================
// Function generation
// ==========================

void CodeGen::generateFunction(const FunctionDecl& fn) {
	// Build the parameter types.
	std::vector<llvm::Type*> paramTypes;
	for (const auto& param : fn.params) {
		paramTypes.push_back(getLLVMType(*param.type));
	}

	llvm::Type* retType = getLLVMType(*fn.returnType);
	auto funcType = llvm::FunctionType::get(retType, paramTypes, false);

	llvm::Function* function = llvm::Function::Create(
		funcType, llvm::Function::ExternalLinkage, fn.name, module.get());

	// Set parameter names.
	size_t idx = 0;
	for (auto& arg : function->args()) {
		arg.setName(fn.params[idx].name);
		idx++;
	}

	// Create the entry basic block.
	auto* entry = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(entry);

	pushScope();

	// Allocate space for parameters and store them.
	idx = 0;
	for (auto& arg : function->args()) {
		llvm::Type* paramType = paramTypes[idx];
		auto alloca = createEntryBlockAlloca(function, fn.params[idx].name, paramType);
		builder->CreateStore(&arg, alloca);
		declareVariable(fn.params[idx].name, alloca, paramType);
		idx++;
	}

	// Generate the function body.
	for (const auto& stmt : fn.body.statements) {
		generateStatement(*stmt);
		// If the current block is already terminated (return/break/continue), stop.
		if (builder->GetInsertBlock()->getTerminator()) break;
	}

	// If the function is void and doesn't end with a return, add one.
	if (!builder->GetInsertBlock()->getTerminator()) {
		if (retType->isVoidTy()) {
			builder->CreateRetVoid();
		} else {
			// Default: return 0 for int, 0.0 for float, etc.
			builder->CreateRet(llvm::Constant::getNullValue(retType));
		}
	}

	popScope();
}

// ==========================
// Statement dispatch
// ==========================

void CodeGen::generateStatement(const Statement& stmt) {
	if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
		generateVarDecl(*s);
	} else if (auto* s = dynamic_cast<const AssignStmt*>(&stmt)) {
		generateAssign(*s);
	} else if (auto* s = dynamic_cast<const IfStmt*>(&stmt)) {
		generateIf(*s);
	} else if (auto* s = dynamic_cast<const WhileStmt*>(&stmt)) {
		generateWhile(*s);
	} else if (auto* s = dynamic_cast<const ForStmt*>(&stmt)) {
		generateFor(*s);
	} else if (auto* s = dynamic_cast<const ForInStmt*>(&stmt)) {
		generateForIn(*s);
	} else if (auto* s = dynamic_cast<const MatchStmt*>(&stmt)) {
		generateMatch(*s);
	} else if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) {
		generateReturn(*s);
	} else if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
		generateExprStmt(*s);
	} else if (dynamic_cast<const BreakStmt*>(&stmt)) {
		if (breakTargets.empty()) throw CodeGenError("break outside of loop");
		builder->CreateBr(breakTargets.back());
		// Create an unreachable block for any code after break.
		auto* fn = builder->GetInsertBlock()->getParent();
		auto* deadBB = llvm::BasicBlock::Create(*context, "after.break", fn);
		builder->SetInsertPoint(deadBB);
	} else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
		if (continueTargets.empty()) throw CodeGenError("continue outside of loop");
		builder->CreateBr(continueTargets.back());
		auto* fn = builder->GetInsertBlock()->getParent();
		auto* deadBB = llvm::BasicBlock::Create(*context, "after.continue", fn);
		builder->SetInsertPoint(deadBB);
	} else {
		throw CodeGenError("Unknown statement type");
	}
}

// ==========================
// Variable declaration
// ==========================

void CodeGen::generateVarDecl(const VarDeclStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();
	llvm::Type* varType = nullptr;

	if (stmt.type) {
		varType = getLLVMType(*stmt.type);
	} else if (stmt.isWalrus && stmt.initializer) {
		// Infer type from initializer.
		llvm::Value* initVal = generateExpression(*stmt.initializer);
		varType = initVal->getType();
		auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);
		builder->CreateStore(initVal, alloca);
		declareVariable(stmt.name, alloca, varType);
		return;
	} else {
		throw CodeGenError("Cannot infer type for variable: " + stmt.name);
	}

	auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);

	if (stmt.initializer) {
		llvm::Value* initVal = generateExpression(*stmt.initializer);
		// Implicit cast: if variable is float and init is int, cast.
		if (isFloatType(varType) && initVal->getType()->isIntegerTy()) {
			initVal = builder->CreateSIToFP(initVal, varType, "cast");
		}
		builder->CreateStore(initVal, alloca);
	} else {
		// Zero-initialize.
		builder->CreateStore(llvm::Constant::getNullValue(varType), alloca);
	}

	declareVariable(stmt.name, alloca, varType);
}

// ==========================
// Assignment
// ==========================

void CodeGen::generateAssign(const AssignStmt& stmt) {
	llvm::Value* ptr = generateLValue(*stmt.target);
	llvm::Value* rhs = generateExpression(*stmt.value);

	llvm::Type* ptrElemType = nullptr;
	// Get the type from the lvalue.
	if (auto* ident = dynamic_cast<const IdentifierExpr*>(stmt.target.get())) {
		ptrElemType = lookupVarType(ident->name);
	} else {
		ptrElemType = rhs->getType();
	}

	if (stmt.op == "=") {
		builder->CreateStore(rhs, ptr);
	} else {
		// Compound assignment: load current value, compute, store.
		llvm::Value* lhs = builder->CreateLoad(ptrElemType, ptr, "load");
		llvm::Value* result = nullptr;

		if (isFloatType(ptrElemType)) {
			if (stmt.op == "+=") result = builder->CreateFAdd(lhs, rhs, "fadd");
			else if (stmt.op == "-=") result = builder->CreateFSub(lhs, rhs, "fsub");
			else if (stmt.op == "*=") result = builder->CreateFMul(lhs, rhs, "fmul");
			else if (stmt.op == "/=") result = builder->CreateFDiv(lhs, rhs, "fdiv");
			else throw CodeGenError("Unsupported float compound assign: " + stmt.op);
		} else {
			if (stmt.op == "+=") result = builder->CreateAdd(lhs, rhs, "add");
			else if (stmt.op == "-=") result = builder->CreateSub(lhs, rhs, "sub");
			else if (stmt.op == "*=") result = builder->CreateMul(lhs, rhs, "mul");
			else if (stmt.op == "/=") result = builder->CreateSDiv(lhs, rhs, "div");
			else if (stmt.op == "%=") result = builder->CreateSRem(lhs, rhs, "rem");
			else throw CodeGenError("Unsupported compound assign: " + stmt.op);
		}
		builder->CreateStore(result, ptr);
	}
}

// ==========================
// LValue generation (for assignment targets)
// ==========================

llvm::Value* CodeGen::generateLValue(const Expression& expr) {
	if (auto* ident = dynamic_cast<const IdentifierExpr*>(&expr)) {
		return lookupVariable(ident->name);
	}
	throw CodeGenError("Invalid assignment target");
}

// ==========================
// If statement
// ==========================

void CodeGen::generateIf(const IfStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();

	llvm::Value* cond = generateExpression(*stmt.condition);
	// Ensure it's i1.
	if (!cond->getType()->isIntegerTy(1)) {
		cond = builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "tobool");
	}

	auto* mergeBB = llvm::BasicBlock::Create(*context, "if.end", fn);
	auto* thenBB = llvm::BasicBlock::Create(*context, "if.then", fn, mergeBB);

	// Build the chain of else-if and else blocks.
	std::vector<llvm::BasicBlock*> elseIfCondBBs;
	std::vector<llvm::BasicBlock*> elseIfBodyBBs;
	llvm::BasicBlock* elseBB = nullptr;

	for (size_t i = 0; i < stmt.elseIfBlocks.size(); i++) {
		auto* condBB = llvm::BasicBlock::Create(*context, "elseif.cond", fn, mergeBB);
		auto* bodyBB = llvm::BasicBlock::Create(*context, "elseif.body", fn, mergeBB);
		elseIfCondBBs.push_back(condBB);
		elseIfBodyBBs.push_back(bodyBB);
	}

	if (stmt.elseBlock) {
		elseBB = llvm::BasicBlock::Create(*context, "if.else", fn, mergeBB);
	}

	// Determine the "false" target of the initial if condition.
	llvm::BasicBlock* falseDest = nullptr;
	if (!elseIfCondBBs.empty()) {
		falseDest = elseIfCondBBs[0];
	} else if (elseBB) {
		falseDest = elseBB;
	} else {
		falseDest = mergeBB;
	}

	builder->CreateCondBr(cond, thenBB, falseDest);

	// Then block.
	builder->SetInsertPoint(thenBB);
	for (const auto& s : stmt.thenBlock.statements) {
		generateStatement(*s);
		if (builder->GetInsertBlock()->getTerminator()) break;
	}
	if (!builder->GetInsertBlock()->getTerminator()) {
		builder->CreateBr(mergeBB);
	}

	// Else-if blocks.
	for (size_t i = 0; i < stmt.elseIfBlocks.size(); i++) {
		builder->SetInsertPoint(elseIfCondBBs[i]);
		llvm::Value* eicond = generateExpression(*stmt.elseIfBlocks[i].first);
		if (!eicond->getType()->isIntegerTy(1)) {
			eicond = builder->CreateICmpNE(eicond, llvm::Constant::getNullValue(eicond->getType()), "tobool");
		}

		llvm::BasicBlock* nextFalse = nullptr;
		if (i + 1 < elseIfCondBBs.size()) {
			nextFalse = elseIfCondBBs[i + 1];
		} else if (elseBB) {
			nextFalse = elseBB;
		} else {
			nextFalse = mergeBB;
		}
		builder->CreateCondBr(eicond, elseIfBodyBBs[i], nextFalse);

		builder->SetInsertPoint(elseIfBodyBBs[i]);
		for (const auto& s : stmt.elseIfBlocks[i].second.statements) {
			generateStatement(*s);
			if (builder->GetInsertBlock()->getTerminator()) break;
		}
		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateBr(mergeBB);
		}
	}

	// Else block.
	if (elseBB) {
		builder->SetInsertPoint(elseBB);
		for (const auto& s : stmt.elseBlock->statements) {
			generateStatement(*s);
			if (builder->GetInsertBlock()->getTerminator()) break;
		}
		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateBr(mergeBB);
		}
	}

	builder->SetInsertPoint(mergeBB);
}

// ==========================
// While loop
// ==========================

void CodeGen::generateWhile(const WhileStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* condBB = llvm::BasicBlock::Create(*context, "while.cond", fn);
	auto* bodyBB = llvm::BasicBlock::Create(*context, "while.body", fn);
	auto* endBB = llvm::BasicBlock::Create(*context, "while.end", fn);

	builder->CreateBr(condBB);

	// Condition.
	builder->SetInsertPoint(condBB);
	llvm::Value* cond = generateExpression(*stmt.condition);
	if (!cond->getType()->isIntegerTy(1)) {
		cond = builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "tobool");
	}
	builder->CreateCondBr(cond, bodyBB, endBB);

	// Body.
	breakTargets.push_back(endBB);
	continueTargets.push_back(condBB);

	builder->SetInsertPoint(bodyBB);
	for (const auto& s : stmt.body.statements) {
		generateStatement(*s);
		if (builder->GetInsertBlock()->getTerminator()) break;
	}
	if (!builder->GetInsertBlock()->getTerminator()) {
		builder->CreateBr(condBB);
	}

	breakTargets.pop_back();
	continueTargets.pop_back();

	builder->SetInsertPoint(endBB);
}

// ==========================
// For loop (C-style)
// ==========================

void CodeGen::generateFor(const ForStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();

	pushScope();

	// Init.
	if (stmt.init) {
		generateStatement(*stmt.init);
	}

	auto* condBB = llvm::BasicBlock::Create(*context, "for.cond", fn);
	auto* bodyBB = llvm::BasicBlock::Create(*context, "for.body", fn);
	auto* updateBB = llvm::BasicBlock::Create(*context, "for.update", fn);
	auto* endBB = llvm::BasicBlock::Create(*context, "for.end", fn);

	builder->CreateBr(condBB);

	// Condition.
	builder->SetInsertPoint(condBB);
	if (stmt.condition) {
		llvm::Value* cond = generateExpression(*stmt.condition);
		if (!cond->getType()->isIntegerTy(1)) {
			cond = builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "tobool");
		}
		builder->CreateCondBr(cond, bodyBB, endBB);
	} else {
		builder->CreateBr(bodyBB);  // infinite loop if no condition
	}

	// Body.
	breakTargets.push_back(endBB);
	continueTargets.push_back(updateBB);

	builder->SetInsertPoint(bodyBB);
	for (const auto& s : stmt.body.statements) {
		generateStatement(*s);
		if (builder->GetInsertBlock()->getTerminator()) break;
	}
	if (!builder->GetInsertBlock()->getTerminator()) {
		builder->CreateBr(updateBB);
	}

	breakTargets.pop_back();
	continueTargets.pop_back();

	// Update.
	builder->SetInsertPoint(updateBB);
	if (stmt.update) {
		generateExpression(*stmt.update);
	}
	builder->CreateBr(condBB);

	popScope();

	builder->SetInsertPoint(endBB);
}

// ==========================
// For-in loop (range-based)
// ==========================

void CodeGen::generateForIn(const ForInStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();

	pushScope();

	llvm::Type* varType = getLLVMType(*stmt.varType);
	auto alloca = createEntryBlockAlloca(fn, stmt.varName, varType);
	llvm::Value* startVal = generateExpression(*stmt.rangeStart);
	builder->CreateStore(startVal, alloca);
	declareVariable(stmt.varName, alloca, varType);

	llvm::Value* endVal = generateExpression(*stmt.rangeEnd);

	auto* condBB = llvm::BasicBlock::Create(*context, "forin.cond", fn);
	auto* bodyBB = llvm::BasicBlock::Create(*context, "forin.body", fn);
	auto* updateBB = llvm::BasicBlock::Create(*context, "forin.update", fn);
	auto* endBB = llvm::BasicBlock::Create(*context, "forin.end", fn);

	builder->CreateBr(condBB);

	// Condition: i < end.
	builder->SetInsertPoint(condBB);
	llvm::Value* curVal = builder->CreateLoad(varType, alloca, stmt.varName);
	llvm::Value* cond = builder->CreateICmpSLT(curVal, endVal, "forin.cmp");
	builder->CreateCondBr(cond, bodyBB, endBB);

	// Body.
	breakTargets.push_back(endBB);
	continueTargets.push_back(updateBB);

	builder->SetInsertPoint(bodyBB);
	for (const auto& s : stmt.body.statements) {
		generateStatement(*s);
		if (builder->GetInsertBlock()->getTerminator()) break;
	}
	if (!builder->GetInsertBlock()->getTerminator()) {
		builder->CreateBr(updateBB);
	}

	breakTargets.pop_back();
	continueTargets.pop_back();

	// Update: i++.
	builder->SetInsertPoint(updateBB);
	llvm::Value* cur = builder->CreateLoad(varType, alloca, stmt.varName);
	llvm::Value* next = builder->CreateAdd(cur, llvm::ConstantInt::get(varType, 1), "forin.inc");
	builder->CreateStore(next, alloca);
	builder->CreateBr(condBB);

	popScope();

	builder->SetInsertPoint(endBB);
}

// ==========================
// Match statement
// ==========================

void CodeGen::generateMatch(const MatchStmt& stmt) {
	auto* fn = builder->GetInsertBlock()->getParent();
	llvm::Value* subject = generateExpression(*stmt.subject);
	bool isString = isStringType(subject->getType());

	auto* mergeBB = llvm::BasicBlock::Create(*context, "match.end", fn);

	// For each case, create a condition check and body block.
	struct CaseInfo {
		llvm::BasicBlock* bodyBB;
		const MatchCase* mc;
	};
	std::vector<CaseInfo> cases;
	llvm::BasicBlock* defaultBB = nullptr;

	for (const auto& mc : stmt.cases) {
		auto* bodyBB = llvm::BasicBlock::Create(*context, "match.case", fn, mergeBB);
		cases.push_back({bodyBB, &mc});
		if (mc.isDefault) {
			defaultBB = bodyBB;
		}
	}

	llvm::BasicBlock* fallthrough = defaultBB ? defaultBB : mergeBB;

	// Generate conditions as a chain of if-else.
	for (size_t i = 0; i < cases.size(); i++) {
		const auto& mc = *cases[i].mc;
		if (mc.isDefault) continue;

		// Compute the OR of all pattern matches.
		llvm::Value* matchCond = nullptr;
		for (const auto& pattern : mc.patterns) {
			llvm::Value* patVal = generateExpression(*pattern);
			llvm::Value* cmp = nullptr;

			if (isString) {
				// strcmp(subject, pattern) == 0
				llvm::Value* cmpResult = builder->CreateCall(
					strcmpFunc, {subject, patVal}, "strcmp");
				cmp = builder->CreateICmpEQ(
					cmpResult, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0), "streq");
			} else if (subject->getType()->isDoubleTy()) {
				cmp = builder->CreateFCmpOEQ(subject, patVal, "fcmp");
			} else {
				cmp = builder->CreateICmpEQ(subject, patVal, "icmp");
			}

			if (!matchCond) {
				matchCond = cmp;
			} else {
				matchCond = builder->CreateOr(matchCond, cmp, "match.or");
			}
		}

		// Determine next condition block.
		llvm::BasicBlock* nextCondBB = nullptr;
		// Find the next non-default case.
		bool foundNext = false;
		for (size_t j = i + 1; j < cases.size(); j++) {
			if (!cases[j].mc->isDefault) {
				nextCondBB = llvm::BasicBlock::Create(*context, "match.check", fn, cases[j].bodyBB);
				foundNext = true;

				builder->CreateCondBr(matchCond, cases[i].bodyBB, nextCondBB);
				builder->SetInsertPoint(nextCondBB);
				break;
			}
		}
		if (!foundNext) {
			builder->CreateCondBr(matchCond, cases[i].bodyBB, fallthrough);
		}
	}

	// If we're still at a non-terminated block (should not happen normally),
	// branch to fallthrough.
	if (!builder->GetInsertBlock()->getTerminator()) {
		builder->CreateBr(fallthrough);
	}

	// Generate case bodies.
	for (auto& c : cases) {
		builder->SetInsertPoint(c.bodyBB);
		for (const auto& s : c.mc->body.statements) {
			generateStatement(*s);
			if (builder->GetInsertBlock()->getTerminator()) break;
		}
		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateBr(mergeBB);
		}
	}

	builder->SetInsertPoint(mergeBB);
}

// ==========================
// Return
// ==========================

void CodeGen::generateReturn(const ReturnStmt& stmt) {
	if (stmt.value) {
		llvm::Value* val = generateExpression(*stmt.value);
		builder->CreateRet(val);
	} else {
		builder->CreateRetVoid();
	}
}

// ==========================
// Expression statement
// ==========================

void CodeGen::generateExprStmt(const ExprStmt& stmt) {
	generateExpression(*stmt.expression);
}

// ==========================
// Expression dispatch
// ==========================

llvm::Value* CodeGen::generateExpression(const Expression& expr) {
	if (auto* e = dynamic_cast<const IntLiteralExpr*>(&expr)) {
		return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), e->value);
	}
	if (auto* e = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
		return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), e->value);
	}
	if (auto* e = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
		return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), e->value ? 1 : 0);
	}
	if (auto* e = dynamic_cast<const StringLiteralExpr*>(&expr)) {
		return builder->CreateGlobalStringPtr(e->value, "str");
	}
	if (auto* e = dynamic_cast<const CharLiteralExpr*>(&expr)) {
		if (e->value.empty()) return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), 0);
		char c = e->value[0];
		// Handle escape sequences stored as single chars.
		if (e->value.size() > 1 && e->value[0] == '\\') {
			switch (e->value[1]) {
				case 'n': c = '\n'; break;
				case 't': c = '\t'; break;
				case '\\': c = '\\'; break;
				case '\'': c = '\''; break;
				case '0': c = '\0'; break;
				default: c = e->value[1]; break;
			}
		}
		return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), c);
	}
	if (auto* e = dynamic_cast<const IdentifierExpr*>(&expr)) {
		return generateIdentifier(*e);
	}
	if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
		return generateBinary(*e);
	}
	if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
		return generateUnary(*e);
	}
	if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
		return generateCall(*e);
	}
	if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
		// Handle assignment expressions (e.g., inside for-update).
		llvm::Value* ptr = generateLValue(*e->target);
		llvm::Value* rhs = generateExpression(*e->value);
		builder->CreateStore(rhs, ptr);
		return rhs;
	}
	throw CodeGenError("Unsupported expression type");
}

// ==========================
// Identifier (load from variable)
// ==========================

llvm::Value* CodeGen::generateIdentifier(const IdentifierExpr& expr) {
	auto alloca = lookupVariable(expr.name);
	auto type = lookupVarType(expr.name);
	return builder->CreateLoad(type, alloca, expr.name);
}

// ==========================
// Binary expressions
// ==========================

llvm::Value* CodeGen::generateBinary(const BinaryExpr& expr) {
	llvm::Value* lhs = generateExpression(*expr.left);
	llvm::Value* rhs = generateExpression(*expr.right);

	bool isFloat = isFloatType(lhs->getType()) || isFloatType(rhs->getType());

	// Promote int to float if mixed.
	if (isFloat) {
		if (lhs->getType()->isIntegerTy()) {
			lhs = builder->CreateSIToFP(lhs, llvm::Type::getDoubleTy(*context), "cast");
		}
		if (rhs->getType()->isIntegerTy()) {
			rhs = builder->CreateSIToFP(rhs, llvm::Type::getDoubleTy(*context), "cast");
		}
	}

	bool isStr = isStringType(lhs->getType()) && isStringType(rhs->getType());

	// Arithmetic.
	if (expr.op == "+") {
		return isFloat ? builder->CreateFAdd(lhs, rhs, "fadd")
					   : builder->CreateAdd(lhs, rhs, "add");
	}
	if (expr.op == "-") {
		return isFloat ? builder->CreateFSub(lhs, rhs, "fsub")
					   : builder->CreateSub(lhs, rhs, "sub");
	}
	if (expr.op == "*") {
		return isFloat ? builder->CreateFMul(lhs, rhs, "fmul")
					   : builder->CreateMul(lhs, rhs, "mul");
	}
	if (expr.op == "/") {
		return isFloat ? builder->CreateFDiv(lhs, rhs, "fdiv")
					   : builder->CreateSDiv(lhs, rhs, "div");
	}
	if (expr.op == "%") {
		return isFloat ? builder->CreateFRem(lhs, rhs, "frem")
					   : builder->CreateSRem(lhs, rhs, "rem");
	}

	// Comparison operators.
	if (expr.op == "==") {
		if (isStr) {
			llvm::Value* cmp = builder->CreateCall(strcmpFunc, {lhs, rhs}, "strcmp");
			return builder->CreateICmpEQ(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0), "streq");
		}
		return isFloat ? builder->CreateFCmpOEQ(lhs, rhs, "feq")
					   : builder->CreateICmpEQ(lhs, rhs, "eq");
	}
	if (expr.op == "!=") {
		if (isStr) {
			llvm::Value* cmp = builder->CreateCall(strcmpFunc, {lhs, rhs}, "strcmp");
			return builder->CreateICmpNE(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0), "strne");
		}
		return isFloat ? builder->CreateFCmpONE(lhs, rhs, "fne")
					   : builder->CreateICmpNE(lhs, rhs, "ne");
	}
	if (expr.op == "<") {
		return isFloat ? builder->CreateFCmpOLT(lhs, rhs, "flt")
					   : builder->CreateICmpSLT(lhs, rhs, "lt");
	}
	if (expr.op == ">") {
		return isFloat ? builder->CreateFCmpOGT(lhs, rhs, "fgt")
					   : builder->CreateICmpSGT(lhs, rhs, "gt");
	}
	if (expr.op == "<=") {
		return isFloat ? builder->CreateFCmpOLE(lhs, rhs, "fle")
					   : builder->CreateICmpSLE(lhs, rhs, "le");
	}
	if (expr.op == ">=") {
		return isFloat ? builder->CreateFCmpOGE(lhs, rhs, "fge")
					   : builder->CreateICmpSGE(lhs, rhs, "ge");
	}

	// Logical operators.
	if (expr.op == "&&") {
		// Convert both to i1 if needed.
		if (!lhs->getType()->isIntegerTy(1)) {
			lhs = builder->CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()), "tobool");
		}
		if (!rhs->getType()->isIntegerTy(1)) {
			rhs = builder->CreateICmpNE(rhs, llvm::Constant::getNullValue(rhs->getType()), "tobool");
		}
		return builder->CreateAnd(lhs, rhs, "and");
	}
	if (expr.op == "||") {
		if (!lhs->getType()->isIntegerTy(1)) {
			lhs = builder->CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()), "tobool");
		}
		if (!rhs->getType()->isIntegerTy(1)) {
			rhs = builder->CreateICmpNE(rhs, llvm::Constant::getNullValue(rhs->getType()), "tobool");
		}
		return builder->CreateOr(lhs, rhs, "or");
	}
	if (expr.op == "^^") {
		if (!lhs->getType()->isIntegerTy(1)) {
			lhs = builder->CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()), "tobool");
		}
		if (!rhs->getType()->isIntegerTy(1)) {
			rhs = builder->CreateICmpNE(rhs, llvm::Constant::getNullValue(rhs->getType()), "tobool");
		}
		return builder->CreateXor(lhs, rhs, "xor");
	}

	// Bitwise operators.
	if (expr.op == "&") return builder->CreateAnd(lhs, rhs, "band");
	if (expr.op == "|") return builder->CreateOr(lhs, rhs, "bor");
	if (expr.op == "^") return builder->CreateXor(lhs, rhs, "bxor");
	if (expr.op == "<<") return builder->CreateShl(lhs, rhs, "shl");
	if (expr.op == ">>") return builder->CreateAShr(lhs, rhs, "shr");

	throw CodeGenError("Unsupported binary operator: " + expr.op);
}

// ==========================
// Unary expressions
// ==========================

llvm::Value* CodeGen::generateUnary(const UnaryExpr& expr) {
	if (expr.prefix) {
		llvm::Value* operand = generateExpression(*expr.operand);
		if (expr.op == "-") {
			if (isFloatType(operand->getType())) {
				return builder->CreateFNeg(operand, "fneg");
			}
			return builder->CreateNeg(operand, "neg");
		}
		if (expr.op == "!") {
			if (!operand->getType()->isIntegerTy(1)) {
				operand = builder->CreateICmpNE(operand, llvm::Constant::getNullValue(operand->getType()), "tobool");
			}
			return builder->CreateNot(operand, "not");
		}
		if (expr.op == "~") {
			return builder->CreateNot(operand, "bnot");
		}
		throw CodeGenError("Unsupported prefix operator: " + expr.op);
	} else {
		// Postfix: ++ or --
		auto* ident = dynamic_cast<const IdentifierExpr*>(expr.operand.get());
		if (!ident) throw CodeGenError("Postfix operator requires an identifier");

		llvm::AllocaInst* ptr = lookupVariable(ident->name);
		llvm::Type* type = lookupVarType(ident->name);
		llvm::Value* val = builder->CreateLoad(type, ptr, ident->name);
		llvm::Value* oldVal = val;

		if (expr.op == "++") {
			val = builder->CreateAdd(val, llvm::ConstantInt::get(type, 1), "inc");
		} else if (expr.op == "--") {
			val = builder->CreateSub(val, llvm::ConstantInt::get(type, 1), "dec");
		} else {
			throw CodeGenError("Unsupported postfix operator: " + expr.op);
		}
		builder->CreateStore(val, ptr);
		return oldVal;  // Return original value (postfix semantics).
	}
}

// ==========================
// Function calls
// ==========================

llvm::Value* CodeGen::generateCall(const CallExpr& expr) {
	// Check for built-in: print
	auto* callee = dynamic_cast<const IdentifierExpr*>(expr.callee.get());
	if (callee && callee->name == "print") {
		return generatePrintCall(expr.arguments);
	}

	// Look up the function in the module.
	std::string fnName;
	if (callee) {
		fnName = callee->name;
	} else {
		throw CodeGenError("Unsupported callee expression");
	}

	llvm::Function* fn = module->getFunction(fnName);
	if (!fn) throw CodeGenError("Undefined function: " + fnName);

	std::vector<llvm::Value*> args;
	for (const auto& arg : expr.arguments) {
		args.push_back(generateExpression(*arg));
	}

	if (fn->getReturnType()->isVoidTy()) {
		builder->CreateCall(fn, args);
		return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context));
	}
	return builder->CreateCall(fn, args, "call");
}

// ==========================
// Built-in: print()
// ==========================

llvm::Value* CodeGen::generatePrintCall(const std::vector<ExprPtr>& args) {
	if (args.empty()) {
		// print() with no args = print newline
		llvm::Value* fmt = builder->CreateGlobalStringPtr("\n", "fmt");
		return builder->CreateCall(printfFunc, {fmt}, "printf");
	}

	for (const auto& arg : args) {
		llvm::Value* val = generateExpression(*arg);
		llvm::Value* fmt = nullptr;

		if (isStringType(val->getType())) {
			fmt = builder->CreateGlobalStringPtr("%s\n", "fmt");
			builder->CreateCall(printfFunc, {fmt, val}, "printf");
		} else if (isFloatType(val->getType())) {
			fmt = builder->CreateGlobalStringPtr("%g\n", "fmt");
			builder->CreateCall(printfFunc, {fmt, val}, "printf");
		} else if (val->getType()->isIntegerTy(1)) {
			// Bool: print "true" or "false".
			fmt = builder->CreateGlobalStringPtr("%s\n", "fmt");
			llvm::Value* trueStr = builder->CreateGlobalStringPtr("true", "true.str");
			llvm::Value* falseStr = builder->CreateGlobalStringPtr("false", "false.str");
			llvm::Value* str = builder->CreateSelect(val, trueStr, falseStr, "boolstr");
			builder->CreateCall(printfFunc, {fmt, str}, "printf");
		} else if (val->getType()->isIntegerTy(8)) {
			// Char.
			fmt = builder->CreateGlobalStringPtr("%c\n", "fmt");
			builder->CreateCall(printfFunc, {fmt, val}, "printf");
		} else {
			// Default: integer (i64).
			fmt = builder->CreateGlobalStringPtr("%ld\n", "fmt");
			builder->CreateCall(printfFunc, {fmt, val}, "printf");
		}
	}

	return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
}

// Stub implementations for features not yet fully supported.
llvm::Value* CodeGen::generateIndex(const IndexExpr& expr) {
	throw CodeGenError("Index expressions not yet supported in codegen");
}

llvm::Value* CodeGen::generateMemberAccess(const MemberAccessExpr& expr) {
	throw CodeGenError("Member access not yet supported in codegen");
}
