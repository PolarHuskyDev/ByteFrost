#include "codegen/codegen.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <set>
#include <sstream>

// ==========================
// Static stdlib math name set
// ==========================

const std::set<std::string>& CodeGen::stdlibMathNames() {
	static const std::set<std::string> names = {
		"sin", "cos", "tan", "sqrt", "pow", "floor", "ceil", "round",
		"abs", "log", "log2", "log10", "exp", "min", "max"
	};
	return names;
}

// ==========================
// Constructor
// ==========================

CodeGen::CodeGen() {
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>("bytefrost", *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

// ==========================
// Target initialization
// ==========================

void CodeGen::initializeTarget() {
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	auto triple = llvm::sys::getDefaultTargetTriple();
	module->setTargetTriple(triple);

	std::string error;
	const auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
	if (!target) {
		throw CodeGenError("Failed to lookup target: " + error);
	}

	targetMachine.reset(target->createTargetMachine(
		triple,
		"generic",
		"",
		llvm::TargetOptions{},
		llvm::Reloc::PIC_));

	if (!targetMachine) {
		throw CodeGenError("Failed to create target machine for: " + triple);
	}

	module->setDataLayout(targetMachine->createDataLayout());
}

// ==========================
// Public interface
// ==========================

void CodeGen::buildIR(const Program& program) {
	declareBuiltins();

	// Process import declarations:
	//  - Collect alias mappings (import sin as mySin from ...).
	//  - Register namespace imports (import math.utils → "utils").
	//  - Fail fast if a stdlib math name is imported without an alias — the
	//    unaliased name would silently resolve to the intrinsic instead of
	//    the imported function, which is always a bug.
	importAliases_.clear();
	namespaceNames_.clear();
	for (const auto& imp : program.imports) {
		if (imp->isNamespaceImport && !imp->modulePath.empty()) {
			namespaceNames_.insert(imp->modulePath.back());
		}
		for (const auto& item : imp->items) {
			if (!item.alias.empty()) {
				importAliases_[item.alias] = item.name;
			} else if (stdlibMathNames().count(item.name)) {
				throw CodeGenError(
					"Importing '" + item.name + "' conflicts with the stdlib math function "
					"of the same name. Use an alias: import " + item.name + " as <alias> from ...;");
			}
		}
	}

	// Detect stdlib math function conflicts.
	// If a user defines a function whose name matches a stdlib math function
	// without marking it 'overridden', emit a clear compile-time error.
	overriddenMathFuncs_.clear();
	for (const auto& fn : program.functions) {
		if (stdlibMathNames().count(fn->name)) {
			if (!fn->isOverridden) {
				throw CodeGenError(
					"Function '" + fn->name + "' conflicts with a stdlib math function. "
					"Use 'overridden' to shadow it.");
			}
			overriddenMathFuncs_.insert(fn->name);
		}
	}

	// Register all struct types first (so they can be referenced).
	registerStructTypes(program);

	// Generate struct methods.
	generateStructMethods(program);

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
}

std::string CodeGen::generate(const Program& program) {
	buildIR(program);

	// Print the IR to a string.
	std::string irStr;
	llvm::raw_string_ostream irStream(irStr);
	module->print(irStream, nullptr);
	return irStr;
}

void CodeGen::emitObjectFile(const Program& program, const std::string& outputPath) {
	initializeTarget();
	buildIR(program);

	std::error_code ec;
	llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
	if (ec) {
		throw CodeGenError("Could not open output file: " + ec.message());
	}

	llvm::legacy::PassManager pass;
	if (targetMachine->addPassesToEmitFile(pass, dest, nullptr,
										   llvm::CodeGenFileType::ObjectFile)) {
		throw CodeGenError("Target machine cannot emit object files");
	}

	pass.run(*module);
	dest.flush();
}

// ==========================
// Extern function declaration
// (used by orca for cross-module calls)
// ==========================

void CodeGen::declareExternFunction(const FunctionDecl& fn) {
	// If the function is already known (declared or defined), skip.
	if (module->getFunction(fn.name)) return;

	std::vector<llvm::Type*> paramTypes;
	for (const auto& param : fn.params) {
		paramTypes.push_back(getLLVMType(*param.type));
	}
	llvm::Type* retType = getLLVMType(*fn.returnType);
	auto funcType = llvm::FunctionType::get(retType, paramTypes, false);
	llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, fn.name, module.get());
}

// ==========================
// Built-in declarations
// ==========================

void CodeGen::declareBuiltins() {
	auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
	auto* i32 = llvm::Type::getInt32Ty(*context);
	auto* i64 = llvm::Type::getInt64Ty(*context);

	// printf(const char* fmt, ...) -> i32
	auto printfType = llvm::FunctionType::get(i32, {i8Ptr}, true);
	printfFunc = llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", module.get());

	// strcmp(const char*, const char*) -> i32
	auto strcmpType = llvm::FunctionType::get(i32, {i8Ptr, i8Ptr}, false);
	strcmpFunc = llvm::Function::Create(strcmpType, llvm::Function::ExternalLinkage, "strcmp", module.get());

	// malloc(size_t) -> void*
	auto mallocType = llvm::FunctionType::get(i8Ptr, {i64}, false);
	mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", module.get());

	// realloc(void*, size_t) -> void*
	auto reallocType = llvm::FunctionType::get(i8Ptr, {i8Ptr, i64}, false);
	reallocFunc = llvm::Function::Create(reallocType, llvm::Function::ExternalLinkage, "realloc", module.get());

	// free(void*) -> void
	auto freeType = llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {i8Ptr}, false);
	freeFunc = llvm::Function::Create(freeType, llvm::Function::ExternalLinkage, "free", module.get());

	// snprintf(char* buf, size_t size, const char* fmt, ...) -> i32
	auto snprintfType = llvm::FunctionType::get(i32, {i8Ptr, i64, i8Ptr}, true);
	snprintfFunc = llvm::Function::Create(snprintfType, llvm::Function::ExternalLinkage, "snprintf", module.get());

	// Math stdlib: tan(double) -> double  (no LLVM intrinsic available).
	// Declared lazily in generateMathCall to avoid collision with user-defined 'overridden tan'.
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

void CodeGen::emitRefIncrement(llvm::Value* structAlloca, bool isMap) {
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	size_t rcIdx = isMap ? 4 : 3;
	llvm::StructType* structType;
	if (isMap) {
		structType = llvm::StructType::get(*context, {ptrType, ptrType, i64, i64, ptrType});
	} else {
		structType = llvm::StructType::get(*context, {ptrType, i64, i64, ptrType});
	}

	auto* rcPtrPtr = builder->CreateStructGEP(structType, structAlloca, rcIdx, "rc.ptr.ptr");
	auto* rcPtr = builder->CreateLoad(ptrType, rcPtrPtr, "rc.ptr");
	auto* rcVal = builder->CreateLoad(i64, rcPtr, "rc.val");
	auto* newRc = builder->CreateAdd(rcVal, llvm::ConstantInt::get(i64, 1), "rc.inc");
	builder->CreateStore(newRc, rcPtr);
}

void CodeGen::emitRefDecrement(llvm::Value* structAlloca, bool isMap) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	size_t rcIdx = isMap ? 4 : 3;
	llvm::StructType* structType;
	if (isMap) {
		structType = llvm::StructType::get(*context, {ptrType, ptrType, i64, i64, ptrType});
	} else {
		structType = llvm::StructType::get(*context, {ptrType, i64, i64, ptrType});
	}

	auto* rcPtrPtr = builder->CreateStructGEP(structType, structAlloca, rcIdx, "rc.ptr.ptr");
	auto* rcPtr = builder->CreateLoad(ptrType, rcPtrPtr, "rc.ptr");
	auto* rcVal = builder->CreateLoad(i64, rcPtr, "rc.val");
	auto* newRc = builder->CreateSub(rcVal, llvm::ConstantInt::get(i64, 1), "rc.dec");
	builder->CreateStore(newRc, rcPtr);

	// If rc reaches 0, free the heap buffers and the rc itself.
	auto* freeBB = llvm::BasicBlock::Create(*context, "rc.free", fn);
	auto* contBB = llvm::BasicBlock::Create(*context, "rc.cont", fn);
	auto* isZero = builder->CreateICmpEQ(newRc, llvm::ConstantInt::get(i64, 0), "rc.iszero");
	builder->CreateCondBr(isZero, freeBB, contBB);

	builder->SetInsertPoint(freeBB);
	if (isMap) {
		auto* keysPtr = builder->CreateLoad(ptrType,
			builder->CreateStructGEP(structType, structAlloca, 0), "keys");
		builder->CreateCall(freeFunc, {keysPtr});
		auto* valsPtr = builder->CreateLoad(ptrType,
			builder->CreateStructGEP(structType, structAlloca, 1), "vals");
		builder->CreateCall(freeFunc, {valsPtr});
	} else {
		auto* dataPtr = builder->CreateLoad(ptrType,
			builder->CreateStructGEP(structType, structAlloca, 0), "data");
		builder->CreateCall(freeFunc, {dataPtr});
	}
	builder->CreateCall(freeFunc, {rcPtr});
	builder->CreateBr(contBB);

	builder->SetInsertPoint(contBB);
}

void CodeGen::emitScopeCleanup() {
	auto& scope = scopes.back();
	for (const auto& varName : scope.heapOwned) {
		auto* alloca = scope.variables.at(varName);
		std::string bfType = scope.varBFTypeNames.at(varName);
		bool isMap = bfType.substr(0, 4) == "map<";
		emitRefDecrement(alloca, isMap);
	}
}

void CodeGen::declareVariable(const std::string& name, llvm::AllocaInst* alloca,
							  llvm::Type* type, const std::string& bfTypeName) {
	scopes.back().variables[name] = alloca;
	scopes.back().varTypes[name] = type;
	if (!bfTypeName.empty()) {
		scopes.back().varBFTypeNames[name] = bfTypeName;
	}
	// Track heap-owning variables for refcount cleanup.
	if (bfTypeName.substr(0, 6) == "array<" || bfTypeName.substr(0, 4) == "map<") {
		scopes.back().heapOwned.push_back(name);
	}
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

std::string CodeGen::lookupVarBFTypeName(const std::string& name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		auto found = it->varBFTypeNames.find(name);
		if (found != it->varBFTypeNames.end()) return found->second;
	}
	return "";  // no BF type name tracked
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

	// User-defined struct type
	auto structIt = structRegistry.find(type.name);
	if (structIt != structRegistry.end()) {
		return structIt->second.llvmType;
	}

	// Parameterized types
	if (type.name == "array" && type.typeParams.size() == 1) {
		llvm::Type* elemType = getLLVMType(*type.typeParams[0]);
		return getOrCreateArrayType(elemType);
	}
	if (type.name == "map" && type.typeParams.size() == 2) {
		llvm::Type* keyType = getLLVMType(*type.typeParams[0]);
		llvm::Type* valType = getLLVMType(*type.typeParams[1]);
		return getOrCreateMapType(keyType, valType);
	}

	throw CodeGenError("Unsupported type: " + type.name);
}

bool CodeGen::isStringType(llvm::Type* type) const {
	return type->isPointerTy();
}

bool CodeGen::isFloatType(llvm::Type* type) const {
	return type->isDoubleTy() || type->isFloatTy();
}

std::string CodeGen::getFormatSpecifier(llvm::Type* type) const {
	if (type->isPointerTy()) return "%s";
	if (type->isDoubleTy() || type->isFloatTy()) return "%g";
	if (type->isIntegerTy(1)) return "%s";  // bool — handled specially
	if (type->isIntegerTy(8)) return "%c";
	return "%ld";  // i64 or other int
}

// ==========================
// Struct support
// ==========================

void CodeGen::registerStructTypes(const Program& program) {
	// Build a map of struct name -> field type names for cycle detection.
	std::map<std::string, std::vector<std::pair<std::string, std::string>>> structFields;
	for (const auto& sd : program.structs) {
		auto& fields = structFields[sd->name];
		for (const auto& member : sd->members) {
			if (member.kind == StructMember::FIELD) {
				fields.emplace_back(member.fieldName, member.fieldType->name);
			}
		}
	}

	// DFS cycle detection: detect direct (A→A) and indirect (A→B→A) cycles.
	std::set<std::string> visited, inStack;
	std::function<void(const std::string&, std::vector<std::string>&)> detectCycle =
		[&](const std::string& name, std::vector<std::string>& path) {
			if (inStack.count(name)) {
				// Build cycle description: find where the cycle starts in path.
				std::string cycle;
				bool inCycle = false;
				for (const auto& p : path) {
					if (p == name) inCycle = true;
					if (inCycle) cycle += p + " -> ";
				}
				cycle += name;
				throw CodeGenError(
					"Cyclic struct dependency detected: " + cycle +
					". Structs are value types and cannot form cycles "
					"(infinite size). Future: use Box<T> for heap-allocated indirection.");
			}
			if (visited.count(name)) return;
			visited.insert(name);
			inStack.insert(name);
			path.push_back(name);
			if (structFields.count(name)) {
				for (const auto& [fieldName, fieldType] : structFields.at(name)) {
					if (structFields.count(fieldType)) {
						detectCycle(fieldType, path);
					}
				}
			}
			path.pop_back();
			inStack.erase(name);
		};

	for (const auto& sd : program.structs) {
		std::vector<std::string> path;
		detectCycle(sd->name, path);
	}

	// All structs are acyclic — safe to register types.
	for (const auto& sd : program.structs) {
		StructInfo info;

		// Collect field types.
		std::vector<llvm::Type*> fieldTypes;
		for (const auto& member : sd->members) {
			if (member.kind == StructMember::FIELD) {
				llvm::Type* ft = getLLVMType(*member.fieldType);
				info.fieldNames.push_back(member.fieldName);
				info.fieldLLVMTypes.push_back(ft);
				info.fieldIndices[member.fieldName] = fieldTypes.size();
				fieldTypes.push_back(ft);
			}
		}

		info.llvmType = llvm::StructType::create(*context, fieldTypes, sd->name);

		// Register method names.
		for (const auto& member : sd->members) {
			if (member.kind == StructMember::METHOD) {
				std::string mangledName = sd->name + "." + member.method->name;
				info.methods[member.method->name] = mangledName;
				if (member.method->name == "constructor") {
					info.hasConstructor = true;
				}
			}
		}

		structRegistry[sd->name] = std::move(info);
	}
}

void CodeGen::generateStructMethods(const Program& program) {
	for (const auto& sd : program.structs) {
		auto& info = structRegistry.at(sd->name);

		for (const auto& member : sd->members) {
			if (member.kind != StructMember::METHOD) continue;
			const auto& fn = *member.method;
			std::string mangledName = sd->name + "." + fn.name;

			// Build parameter types: first is pointer to struct (this), then regular params.
			std::vector<llvm::Type*> paramTypes;
			auto* ptrType = llvm::PointerType::getUnqual(*context);
			paramTypes.push_back(ptrType);  // this pointer

			for (const auto& param : fn.params) {
				paramTypes.push_back(getLLVMType(*param.type));
			}

			llvm::Type* retType = getLLVMType(*fn.returnType);
			auto funcType = llvm::FunctionType::get(retType, paramTypes, false);

			llvm::Function* function = llvm::Function::Create(
				funcType, llvm::Function::ExternalLinkage, mangledName, module.get());

			// Set parameter names.
			auto argIt = function->arg_begin();
			argIt->setName("this");
			++argIt;
			for (size_t i = 0; i < fn.params.size(); i++, ++argIt) {
				argIt->setName(fn.params[i].name);
			}

			auto* entry = llvm::BasicBlock::Create(*context, "entry", function);
			builder->SetInsertPoint(entry);

			pushScope();

			// Store 'this' pointer in an alloca.
			auto thisAlloca = createEntryBlockAlloca(function, "this", ptrType);
			builder->CreateStore(&*function->arg_begin(), thisAlloca);
			declareVariable("this", thisAlloca, ptrType, sd->name);

			// Store regular parameters.
			argIt = function->arg_begin();
			++argIt;  // skip 'this'
			for (size_t i = 0; i < fn.params.size(); i++, ++argIt) {
				llvm::Type* paramType = getLLVMType(*fn.params[i].type);
				auto alloca = createEntryBlockAlloca(function, fn.params[i].name, paramType);
				builder->CreateStore(&*argIt, alloca);
				declareVariable(fn.params[i].name, alloca, paramType);
			}

			// Generate body.
			for (const auto& stmt : fn.body.statements) {
				generateStatement(*stmt);
				if (builder->GetInsertBlock()->getTerminator()) break;
			}

			if (!builder->GetInsertBlock()->getTerminator()) {
				if (retType->isVoidTy()) {
					builder->CreateRetVoid();
				} else {
					builder->CreateRet(llvm::Constant::getNullValue(retType));
				}
			}

			popScope();
		}
	}
}

void CodeGen::generateStructInit(const StructInitExpr& expr, llvm::Value* basePtr,
								 const std::string& structName) {
	auto& info = structRegistry.at(structName);

	for (const auto& [fieldName, fieldExpr] : expr.fields) {
		auto idxIt = info.fieldIndices.find(fieldName);
		if (idxIt == info.fieldIndices.end()) {
			throw CodeGenError("Unknown field '" + fieldName + "' in struct " + structName);
		}
		size_t idx = idxIt->second;
		auto* fieldPtr = builder->CreateStructGEP(info.llvmType, basePtr, idx, fieldName + ".ptr");

		// Handle nested struct init: center: { x: 10, y: 20 }
		if (auto* nestedInit = dynamic_cast<const StructInitExpr*>(fieldExpr.get())) {
			llvm::Type* fieldType = info.fieldLLVMTypes[idx];
			if (auto* st = llvm::dyn_cast<llvm::StructType>(fieldType)) {
				if (st->hasName()) {
					generateStructInit(*nestedInit, fieldPtr, st->getName().str());
					continue;
				}
			}
		}

		llvm::Value* val = generateExpression(*fieldExpr);
		builder->CreateStore(val, fieldPtr);
	}
}

std::pair<llvm::Value*, std::string> CodeGen::resolveStructBase(const Expression& expr) {
	if (auto* ident = dynamic_cast<const IdentifierExpr*>(&expr)) {
		std::string structName = lookupVarBFTypeName(ident->name);
		llvm::Type* varType = lookupVarType(ident->name);
		llvm::Value* basePtr;
		if (varType->isStructTy()) {
			basePtr = lookupVariable(ident->name);
		} else {
			basePtr = builder->CreateLoad(varType, lookupVariable(ident->name), ident->name + ".ptr");
		}
		return {basePtr, structName};
	}
	if (dynamic_cast<const ThisExpr*>(&expr)) {
		std::string structName = lookupVarBFTypeName("this");
		llvm::Type* thisType = lookupVarType("this");
		llvm::Value* basePtr = builder->CreateLoad(thisType, lookupVariable("this"), "this.ptr");
		return {basePtr, structName};
	}
	if (auto* member = dynamic_cast<const MemberAccessExpr*>(&expr)) {
		// Chained access: resolve parent, GEP to intermediate field
		auto [parentPtr, parentStructName] = resolveStructBase(*member->object);
		auto& parentInfo = structRegistry.at(parentStructName);
		auto fieldIt = parentInfo.fieldIndices.find(member->member);
		if (fieldIt == parentInfo.fieldIndices.end()) {
			throw CodeGenError("Unknown field: " + member->member + " on struct " + parentStructName);
		}
		size_t idx = fieldIt->second;
		auto* fieldPtr = builder->CreateStructGEP(parentInfo.llvmType, parentPtr, idx, member->member + ".ptr");

		// Determine BF type name of this field
		llvm::Type* fieldType = parentInfo.fieldLLVMTypes[idx];
		std::string fieldStructName;
		if (auto* st = llvm::dyn_cast<llvm::StructType>(fieldType)) {
			if (st->hasName()) {
				fieldStructName = st->getName().str();
			}
		}
		return {fieldPtr, fieldStructName};
	}
	throw CodeGenError("Cannot resolve struct base for expression");
}

// ==========================
// Array support
// ==========================

llvm::StructType* CodeGen::getOrCreateArrayType(llvm::Type* elemType) {
	// Array struct: { elem_ptr*, i64 length, i64 capacity, i64* refcount }
	auto* ptrType = llvm::PointerType::getUnqual(*context);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	return llvm::StructType::get(*context, {ptrType, i64, i64, ptrType});
}

llvm::Value* CodeGen::generateArrayLiteral(const ArrayLiteralExpr& expr, llvm::Type* elemType) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* arrType = getOrCreateArrayType(elemType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	size_t count = expr.elements.size();

	// Allocate the array struct on the stack.
	auto alloca = createEntryBlockAlloca(fn, "arr", arrType);

	// Compute element size + malloc data buffer.
	uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);
	llvm::Value* bufSize = llvm::ConstantInt::get(i64, count * elemSize);
	llvm::Value* buf = builder->CreateCall(mallocFunc, {bufSize}, "arr.data");

	// Store elements.
	for (size_t i = 0; i < count; i++) {
		llvm::Value* val = generateExpression(*expr.elements[i]);
		llvm::Value* elemPtr = builder->CreateGEP(elemType, buf, llvm::ConstantInt::get(i64, i), "elem.ptr");
		builder->CreateStore(val, elemPtr);
	}

	// Store into the struct: { data, length, capacity, refcount }
	auto* dataPtr = builder->CreateStructGEP(arrType, alloca, 0, "arr.data.ptr");
	builder->CreateStore(buf, dataPtr);
	auto* lenPtr = builder->CreateStructGEP(arrType, alloca, 1, "arr.len.ptr");
	builder->CreateStore(llvm::ConstantInt::get(i64, count), lenPtr);
	auto* capPtr = builder->CreateStructGEP(arrType, alloca, 2, "arr.cap.ptr");
	builder->CreateStore(llvm::ConstantInt::get(i64, count), capPtr);

	// Allocate and init refcount to 1.
	llvm::Value* rcBuf = builder->CreateCall(mallocFunc, {llvm::ConstantInt::get(i64, 8)}, "arr.rc");
	builder->CreateStore(llvm::ConstantInt::get(i64, 1), rcBuf);
	auto* rcPtr = builder->CreateStructGEP(arrType, alloca, 3, "arr.rc.ptr");
	builder->CreateStore(rcBuf, rcPtr);

	return alloca;
}

llvm::Value* CodeGen::generateEmptyArray(llvm::Type* elemType) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* arrType = getOrCreateArrayType(elemType);
	auto* i64 = llvm::Type::getInt64Ty(*context);

	auto alloca = createEntryBlockAlloca(fn, "arr", arrType);

	// Initial capacity of 8.
	uint64_t initCap = 8;
	uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);
	llvm::Value* bufSize = llvm::ConstantInt::get(i64, initCap * elemSize);
	llvm::Value* buf = builder->CreateCall(mallocFunc, {bufSize}, "arr.data");

	auto* dataPtr = builder->CreateStructGEP(arrType, alloca, 0, "arr.data.ptr");
	builder->CreateStore(buf, dataPtr);
	auto* lenPtr = builder->CreateStructGEP(arrType, alloca, 1, "arr.len.ptr");
	builder->CreateStore(llvm::ConstantInt::get(i64, 0), lenPtr);
	auto* capPtr = builder->CreateStructGEP(arrType, alloca, 2, "arr.cap.ptr");
	builder->CreateStore(llvm::ConstantInt::get(i64, initCap), capPtr);

	// Allocate and init refcount to 1.
	llvm::Value* rcBuf = builder->CreateCall(mallocFunc, {llvm::ConstantInt::get(i64, 8)}, "arr.rc");
	builder->CreateStore(llvm::ConstantInt::get(i64, 1), rcBuf);
	auto* rcPtr = builder->CreateStructGEP(arrType, alloca, 3, "arr.rc.ptr");
	builder->CreateStore(rcBuf, rcPtr);

	return alloca;
}

void CodeGen::generateArrayPush(llvm::AllocaInst* arrAlloca, llvm::Type* elemType, llvm::Value* value) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* arrType = getOrCreateArrayType(elemType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);
	uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);

	// Load current length and capacity.
	auto* lenPtr = builder->CreateStructGEP(arrType, arrAlloca, 1, "len.ptr");
	auto* capPtr = builder->CreateStructGEP(arrType, arrAlloca, 2, "cap.ptr");
	auto* dataPtr = builder->CreateStructGEP(arrType, arrAlloca, 0, "data.ptr");
	llvm::Value* len = builder->CreateLoad(i64, lenPtr, "len");
	llvm::Value* cap = builder->CreateLoad(i64, capPtr, "cap");
	llvm::Value* data = builder->CreateLoad(ptrType, dataPtr, "data");

	// Check if len >= cap: need to grow.
	auto* needGrowBB = llvm::BasicBlock::Create(*context, "arr.grow", fn);
	auto* pushBB = llvm::BasicBlock::Create(*context, "arr.push", fn);
	llvm::Value* full = builder->CreateICmpSGE(len, cap, "full");
	builder->CreateCondBr(full, needGrowBB, pushBB);

	// Grow: double the capacity, realloc.
	builder->SetInsertPoint(needGrowBB);
	llvm::Value* newCap = builder->CreateMul(cap, llvm::ConstantInt::get(i64, 2), "newcap");
	llvm::Value* newSize = builder->CreateMul(newCap, llvm::ConstantInt::get(i64, elemSize), "newsize");
	llvm::Value* newData = builder->CreateCall(reallocFunc, {data, newSize}, "newdata");
	builder->CreateStore(newData, dataPtr);
	builder->CreateStore(newCap, capPtr);
	builder->CreateBr(pushBB);

	// Push: store element at data[len], increment length.
	// Re-load data pointer since it may have been updated by realloc.
	builder->SetInsertPoint(pushBB);
	llvm::Value* curData = builder->CreateLoad(ptrType, dataPtr, "curdata");
	llvm::Value* elemPtr = builder->CreateGEP(elemType, curData, len, "push.ptr");
	builder->CreateStore(value, elemPtr);
	llvm::Value* newLen = builder->CreateAdd(len, llvm::ConstantInt::get(i64, 1), "newlen");
	builder->CreateStore(newLen, lenPtr);
}

// ==========================
// Map support
// ==========================

llvm::StructType* CodeGen::getOrCreateMapType(llvm::Type* keyType, llvm::Type* valType) {
	// Map struct: { keys_ptr*, values_ptr*, i64 length, i64 capacity, i64* refcount }
	auto* ptrType = llvm::PointerType::getUnqual(*context);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	return llvm::StructType::get(*context, {ptrType, ptrType, i64, i64, ptrType});
}

llvm::Value* CodeGen::generateEmptyMap(llvm::Type* keyType, llvm::Type* valType) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* mapType = getOrCreateMapType(keyType, valType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	auto alloca = createEntryBlockAlloca(fn, "map", mapType);

	uint64_t initCap = 16;
	uint64_t keySize = module->getDataLayout().getTypeAllocSize(keyType);
	uint64_t valSize = module->getDataLayout().getTypeAllocSize(valType);

	llvm::Value* keyBuf = builder->CreateCall(mallocFunc,
		{llvm::ConstantInt::get(i64, initCap * keySize)}, "map.keys");
	llvm::Value* valBuf = builder->CreateCall(mallocFunc,
		{llvm::ConstantInt::get(i64, initCap * valSize)}, "map.vals");

	builder->CreateStore(keyBuf, builder->CreateStructGEP(mapType, alloca, 0, "keys.ptr"));
	builder->CreateStore(valBuf, builder->CreateStructGEP(mapType, alloca, 1, "vals.ptr"));
	builder->CreateStore(llvm::ConstantInt::get(i64, 0),
						 builder->CreateStructGEP(mapType, alloca, 2, "len.ptr"));
	builder->CreateStore(llvm::ConstantInt::get(i64, initCap),
						 builder->CreateStructGEP(mapType, alloca, 3, "cap.ptr"));

	// Allocate and init refcount to 1.
	llvm::Value* rcBuf = builder->CreateCall(mallocFunc, {llvm::ConstantInt::get(i64, 8)}, "map.rc");
	builder->CreateStore(llvm::ConstantInt::get(i64, 1), rcBuf);
	builder->CreateStore(rcBuf, builder->CreateStructGEP(mapType, alloca, 4, "map.rc.ptr"));

	return alloca;
}

void CodeGen::generateMapSet(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType,
							 llvm::Value* key, llvm::Value* val) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* mapType = getOrCreateMapType(keyType, valType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	uint64_t keySize = module->getDataLayout().getTypeAllocSize(keyType);
	uint64_t valSize = module->getDataLayout().getTypeAllocSize(valType);

	llvm::Value* keys = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 0), "keys");
	llvm::Value* vals = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 1), "vals");
	llvm::Value* len = builder->CreateLoad(i64,
		builder->CreateStructGEP(mapType, mapAlloca, 2), "len");
	llvm::Value* cap = builder->CreateLoad(i64,
		builder->CreateStructGEP(mapType, mapAlloca, 3), "cap");

	// Linear search loop: for (i = 0; i < len; i++) if (keys[i] == key) { vals[i] = val; return; }
	auto* loopBB = llvm::BasicBlock::Create(*context, "map.search", fn);
	auto* foundBB = llvm::BasicBlock::Create(*context, "map.found", fn);
	auto* notFoundBB = llvm::BasicBlock::Create(*context, "map.notfound", fn);
	auto* appendBB = llvm::BasicBlock::Create(*context, "map.append", fn);
	auto* growBB = llvm::BasicBlock::Create(*context, "map.grow", fn);
	auto* doneBB = llvm::BasicBlock::Create(*context, "map.done", fn);

	builder->CreateBr(loopBB);

	// Loop header
	builder->SetInsertPoint(loopBB);
	auto* iPhi = builder->CreatePHI(i64, 2, "i");
	iPhi->addIncoming(llvm::ConstantInt::get(i64, 0), loopBB->getSinglePredecessor());

	llvm::Value* inBounds = builder->CreateICmpSLT(iPhi, len, "inbounds");
	auto* bodyBB = llvm::BasicBlock::Create(*context, "map.body", fn, foundBB);
	builder->CreateCondBr(inBounds, bodyBB, notFoundBB);

	// Loop body: compare keys[i] with key
	builder->SetInsertPoint(bodyBB);
	llvm::Value* keyPtr = builder->CreateGEP(keyType, keys, iPhi, "key.ptr");
	llvm::Value* curKey = builder->CreateLoad(keyType, keyPtr, "curkey");
	llvm::Value* match;
	if (keyType->isIntegerTy()) {
		match = builder->CreateICmpEQ(curKey, key, "keymatch");
	} else if (isStringType(keyType)) {
		llvm::Value* cmp = builder->CreateCall(strcmpFunc, {curKey, key}, "strcmp");
		match = builder->CreateICmpEQ(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0), "keymatch");
	} else if (isFloatType(keyType)) {
		match = builder->CreateFCmpOEQ(curKey, key, "keymatch");
	} else {
		match = builder->CreateICmpEQ(curKey, key, "keymatch");
	}

	// Compute next index BEFORE the terminator.
	llvm::Value* iNext = builder->CreateAdd(iPhi, llvm::ConstantInt::get(i64, 1), "i.next");

	// Create an increment block that branches to the loop header.
	auto* incBB = llvm::BasicBlock::Create(*context, "map.inc", fn, foundBB);
	builder->CreateCondBr(match, foundBB, incBB);

	builder->SetInsertPoint(incBB);
	builder->CreateBr(loopBB);
	iPhi->addIncoming(iNext, incBB);

	// Found: update value
	builder->SetInsertPoint(foundBB);
	auto* foundI = builder->CreatePHI(i64, 1, "found.i");
	foundI->addIncoming(iPhi, bodyBB);
	llvm::Value* valPtr = builder->CreateGEP(valType, vals, foundI, "val.ptr");
	builder->CreateStore(val, valPtr);
	builder->CreateBr(doneBB);

	// Not found: check capacity, grow if needed, then append
	builder->SetInsertPoint(notFoundBB);
	llvm::Value* needGrow = builder->CreateICmpSGE(len, cap, "needgrow");
	builder->CreateCondBr(needGrow, growBB, appendBB);

	// Grow
	builder->SetInsertPoint(growBB);
	llvm::Value* newCap = builder->CreateMul(cap, llvm::ConstantInt::get(i64, 2), "newcap");
	llvm::Value* newKeyBuf = builder->CreateCall(reallocFunc,
		{keys, builder->CreateMul(newCap, llvm::ConstantInt::get(i64, keySize))}, "newkeys");
	llvm::Value* newValBuf = builder->CreateCall(reallocFunc,
		{vals, builder->CreateMul(newCap, llvm::ConstantInt::get(i64, valSize))}, "newvals");
	builder->CreateStore(newKeyBuf, builder->CreateStructGEP(mapType, mapAlloca, 0));
	builder->CreateStore(newValBuf, builder->CreateStructGEP(mapType, mapAlloca, 1));
	builder->CreateStore(newCap, builder->CreateStructGEP(mapType, mapAlloca, 3));
	builder->CreateBr(appendBB);

	// Append
	builder->SetInsertPoint(appendBB);
	auto* keysP = builder->CreatePHI(ptrType, 2, "keys.phi");
	keysP->addIncoming(keys, notFoundBB);
	keysP->addIncoming(newKeyBuf, growBB);
	auto* valsP = builder->CreatePHI(ptrType, 2, "vals.phi");
	valsP->addIncoming(vals, notFoundBB);
	valsP->addIncoming(newValBuf, growBB);

	llvm::Value* newKeyPtr = builder->CreateGEP(keyType, keysP, len, "newkey.ptr");
	builder->CreateStore(key, newKeyPtr);
	llvm::Value* newValPtr = builder->CreateGEP(valType, valsP, len, "newval.ptr");
	builder->CreateStore(val, newValPtr);
	llvm::Value* newLen = builder->CreateAdd(len, llvm::ConstantInt::get(i64, 1), "newlen");
	builder->CreateStore(newLen, builder->CreateStructGEP(mapType, mapAlloca, 2));
	builder->CreateBr(doneBB);

	builder->SetInsertPoint(doneBB);
}

llvm::Value* CodeGen::generateMapGet(llvm::AllocaInst* mapAlloca, llvm::Type* keyType,
									 llvm::Type* valType, llvm::Value* key) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* mapType = getOrCreateMapType(keyType, valType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	llvm::Value* keys = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 0), "keys");
	llvm::Value* vals = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 1), "vals");
	llvm::Value* len = builder->CreateLoad(i64,
		builder->CreateStructGEP(mapType, mapAlloca, 2), "len");

	// Linear search
	auto* loopBB = llvm::BasicBlock::Create(*context, "mapget.loop", fn);
	auto* foundBB = llvm::BasicBlock::Create(*context, "mapget.found", fn);
	auto* defaultBB = llvm::BasicBlock::Create(*context, "mapget.default", fn);
	auto* doneBB = llvm::BasicBlock::Create(*context, "mapget.done", fn);

	builder->CreateBr(loopBB);

	builder->SetInsertPoint(loopBB);
	auto* iPhi = builder->CreatePHI(i64, 2, "i");
	iPhi->addIncoming(llvm::ConstantInt::get(i64, 0), loopBB->getSinglePredecessor());

	llvm::Value* inBounds = builder->CreateICmpSLT(iPhi, len, "inbounds");
	auto* bodyBB = llvm::BasicBlock::Create(*context, "mapget.body", fn, foundBB);
	builder->CreateCondBr(inBounds, bodyBB, defaultBB);

	builder->SetInsertPoint(bodyBB);
	llvm::Value* keyPtr = builder->CreateGEP(keyType, keys, iPhi, "key.ptr");
	llvm::Value* curKey = builder->CreateLoad(keyType, keyPtr, "curkey");
	llvm::Value* match;
	if (keyType->isIntegerTy()) {
		match = builder->CreateICmpEQ(curKey, key, "keymatch");
	} else if (isStringType(keyType)) {
		llvm::Value* cmp = builder->CreateCall(strcmpFunc, {curKey, key}, "strcmp");
		match = builder->CreateICmpEQ(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0), "keymatch");
	} else {
		match = builder->CreateICmpEQ(curKey, key, "keymatch");
	}

	// Compute next index BEFORE the terminator.
	llvm::Value* iNext = builder->CreateAdd(iPhi, llvm::ConstantInt::get(i64, 1), "i.next");

	auto* incBB = llvm::BasicBlock::Create(*context, "mapget.inc", fn, foundBB);
	builder->CreateCondBr(match, foundBB, incBB);

	builder->SetInsertPoint(incBB);
	builder->CreateBr(loopBB);
	iPhi->addIncoming(iNext, incBB);

	// Found
	builder->SetInsertPoint(foundBB);
	auto* foundI = builder->CreatePHI(i64, 1, "found.i");
	foundI->addIncoming(iPhi, bodyBB);
	llvm::Value* valPtr = builder->CreateGEP(valType, vals, foundI, "val.ptr");
	llvm::Value* foundVal = builder->CreateLoad(valType, valPtr, "foundval");
	builder->CreateBr(doneBB);

	// Default (key not found): return zero value
	builder->SetInsertPoint(defaultBB);
	llvm::Value* defaultVal = llvm::Constant::getNullValue(valType);
	builder->CreateBr(doneBB);

	builder->SetInsertPoint(doneBB);
	auto* result = builder->CreatePHI(valType, 2, "mapget.result");
	result->addIncoming(foundVal, foundBB);
	result->addIncoming(defaultVal, defaultBB);
	return result;
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
		// Emit refcount cleanup before the implicit return.
		emitScopeCleanup();
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
	std::string bfTypeName;

	if (stmt.type) {
		varType = getLLVMType(*stmt.type);
		bfTypeName = stmt.type->name;

		// Handle array<T> type params
		if (bfTypeName == "array" && stmt.type->typeParams.size() == 1) {
			bfTypeName = "array<" + stmt.type->typeParams[0]->name + ">";
		}
		// Handle map<K,V> type params
		if (bfTypeName == "map" && stmt.type->typeParams.size() == 2) {
			bfTypeName = "map<" + stmt.type->typeParams[0]->name + "," + stmt.type->typeParams[1]->name + ">";
		}
	} else if (stmt.isWalrus && stmt.initializer) {
		// Infer type from initializer.
		llvm::Value* initVal = generateExpression(*stmt.initializer);
		varType = initVal->getType();
		auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);
		builder->CreateStore(initVal, alloca);
		// If the inferred type is a registered struct, track its BF type name.
		if (varType->isStructTy()) {
			auto* st = llvm::cast<llvm::StructType>(varType);
			if (st->hasName()) {
				std::string name = st->getName().str();
				if (structRegistry.count(name)) {
					declareVariable(stmt.name, alloca, varType, name);
					return;
				}
			}
		}
		declareVariable(stmt.name, alloca, varType);
		return;
	} else {
		throw CodeGenError("Cannot infer type for variable: " + stmt.name);
	}

	// Check for struct type: special initialization.
	if (varType->isStructTy()) {
		auto structIt = structRegistry.find(bfTypeName);
		if (structIt != structRegistry.end()) {
			// It's a struct variable.
			auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);
			if (stmt.initializer) {
				if (auto* structInit = dynamic_cast<const StructInitExpr*>(stmt.initializer.get())) {
					generateStructInit(*structInit, alloca, bfTypeName);
				} else {
					llvm::Value* initVal = generateExpression(*stmt.initializer);
					builder->CreateStore(initVal, alloca);
				}
			} else {
				// Zero-init struct.
				builder->CreateStore(llvm::Constant::getNullValue(varType), alloca);
			}
			declareVariable(stmt.name, alloca, varType, bfTypeName);
			return;
		}

		// It's an array or map type (those are also LLVM struct types).
		if (bfTypeName.substr(0, 6) == "array<") {
			auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);
			if (stmt.initializer) {
				if (auto* arrLit = dynamic_cast<const ArrayLiteralExpr*>(stmt.initializer.get())) {
					// Extract element type from the BF type.
					llvm::Type* elemType = getLLVMType(*stmt.type->typeParams[0]);
					// Generate the literal into a temporary alloca, then copy.
					auto* tmpAlloca = static_cast<llvm::AllocaInst*>(
						generateArrayLiteral(*arrLit, elemType));
					// Copy the struct fields.
					llvm::Value* tmpVal = builder->CreateLoad(varType, tmpAlloca, "arr.tmp");
					builder->CreateStore(tmpVal, alloca);
				} else {
					llvm::Value* initVal = generateExpression(*stmt.initializer);
					builder->CreateStore(initVal, alloca);
				}
			} else {
				// Empty array with initial capacity.
				llvm::Type* elemType = getLLVMType(*stmt.type->typeParams[0]);
				auto* tmpAlloca = static_cast<llvm::AllocaInst*>(generateEmptyArray(elemType));
				llvm::Value* tmpVal = builder->CreateLoad(varType, tmpAlloca, "arr.tmp");
				builder->CreateStore(tmpVal, alloca);
			}
			declareVariable(stmt.name, alloca, varType, bfTypeName);
			return;
		}

		if (bfTypeName.substr(0, 4) == "map<") {
			auto alloca = createEntryBlockAlloca(fn, stmt.name, varType);
			// Maps are always initialized empty.
			llvm::Type* keyType = getLLVMType(*stmt.type->typeParams[0]);
			llvm::Type* valType2 = getLLVMType(*stmt.type->typeParams[1]);
			auto* tmpAlloca = static_cast<llvm::AllocaInst*>(generateEmptyMap(keyType, valType2));
			llvm::Value* tmpVal = builder->CreateLoad(varType, tmpAlloca, "map.tmp");
			builder->CreateStore(tmpVal, alloca);
			declareVariable(stmt.name, alloca, varType, bfTypeName);
			return;
		}
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

	declareVariable(stmt.name, alloca, varType, bfTypeName);
}

// ==========================
// Assignment
// ==========================

void CodeGen::generateAssign(const AssignStmt& stmt) {
	// Check for map index assignment: map[key] = val
	if (auto* indexExpr = dynamic_cast<const IndexExpr*>(stmt.target.get())) {
		if (auto* ident = dynamic_cast<const IdentifierExpr*>(indexExpr->object.get())) {
			std::string bfType = lookupVarBFTypeName(ident->name);
			if (bfType.substr(0, 4) == "map<") {
				// Extract K,V type names from bfType string.
				auto commaPos = bfType.find(',', 4);
				std::string keyTypeName = bfType.substr(4, commaPos - 4);
				std::string valTypeName = bfType.substr(commaPos + 1, bfType.size() - commaPos - 2);

				TypeNode keyTN(keyTypeName);
				TypeNode valTN(valTypeName);
				llvm::Type* keyType = getLLVMType(keyTN);
				llvm::Type* valType = getLLVMType(valTN);

				auto* mapAlloca = lookupVariable(ident->name);
				llvm::Value* key = generateExpression(*indexExpr->index);
				llvm::Value* val = generateExpression(*stmt.value);

				if (stmt.op == "=") {
					generateMapSet(mapAlloca, keyType, valType, key, val);
				} else {
					throw CodeGenError("Compound assignment on map elements not supported");
				}
				return;
			}
		}
	}

	llvm::Value* ptr = generateLValue(*stmt.target);
	llvm::Value* rhs = generateExpression(*stmt.value);

	llvm::Type* ptrElemType = nullptr;
	// Get the type from the lvalue.
	if (auto* ident = dynamic_cast<const IdentifierExpr*>(stmt.target.get())) {
		ptrElemType = lookupVarType(ident->name);
	} else if (auto* indexExpr = dynamic_cast<const IndexExpr*>(stmt.target.get())) {
		// For array index assignment, the element type comes from the array's BF type.
		if (auto* ident = dynamic_cast<const IdentifierExpr*>(indexExpr->object.get())) {
			std::string bfType = lookupVarBFTypeName(ident->name);
			if (bfType.substr(0, 6) == "array<") {
				std::string elemTypeName = bfType.substr(6, bfType.size() - 7);
				TypeNode tn(elemTypeName);
				ptrElemType = getLLVMType(tn);
			}
		}
		if (!ptrElemType) ptrElemType = rhs->getType();
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
	// Index lvalue: arr[i]
	if (auto* indexExpr = dynamic_cast<const IndexExpr*>(&expr)) {
		if (auto* ident = dynamic_cast<const IdentifierExpr*>(indexExpr->object.get())) {
			std::string bfType = lookupVarBFTypeName(ident->name);
			if (bfType.substr(0, 6) == "array<") {
				std::string elemTypeName = bfType.substr(6, bfType.size() - 7);
				TypeNode tn(elemTypeName);
				llvm::Type* elemType = getLLVMType(tn);
				auto* arrAlloca = lookupVariable(ident->name);
				auto* arrType = getOrCreateArrayType(elemType);
				auto* ptrType = llvm::PointerType::getUnqual(*context);

				llvm::Value* data = builder->CreateLoad(ptrType,
					builder->CreateStructGEP(arrType, arrAlloca, 0), "data");
				llvm::Value* idx = generateExpression(*indexExpr->index);
				return builder->CreateGEP(elemType, data, idx, "elem.ptr");
			}
		}
	}
	// Member access lvalue: obj.field (supports chained access)
	if (auto* memberExpr = dynamic_cast<const MemberAccessExpr*>(&expr)) {
		auto [basePtr, structName] = resolveStructBase(*memberExpr->object);

		auto structIt = structRegistry.find(structName);
		if (structIt == structRegistry.end()) {
			throw CodeGenError("Unknown struct type: " + structName);
		}
		auto& info = structIt->second;
		auto fieldIt = info.fieldIndices.find(memberExpr->member);
		if (fieldIt == info.fieldIndices.end()) {
			throw CodeGenError("Unknown field: " + memberExpr->member);
		}
		return builder->CreateStructGEP(info.llvmType, basePtr, fieldIt->second,
										memberExpr->member + ".ptr");
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
		bool foundNext = false;
		for (size_t j = i + 1; j < cases.size(); j++) {
			if (!cases[j].mc->isDefault) {
				auto* nextCondBB = llvm::BasicBlock::Create(*context, "match.check", fn, cases[j].bodyBB);

				builder->CreateCondBr(matchCond, cases[i].bodyBB, nextCondBB);
				builder->SetInsertPoint(nextCondBB);
				foundNext = true;
				break;
			}
		}
		if (!foundNext) {
			builder->CreateCondBr(matchCond, cases[i].bodyBB, fallthrough);
		}
	}

	// If we're still at a non-terminated block, branch to fallthrough.
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
		// Emit refcount cleanup for all active scopes before returning.
		for (auto& scope : scopes) {
			for (const auto& varName : scope.heapOwned) {
				auto* alloca = scope.variables.at(varName);
				std::string bfType = scope.varBFTypeNames.at(varName);
				bool isMap = bfType.substr(0, 4) == "map<";
				emitRefDecrement(alloca, isMap);
			}
		}
		builder->CreateRet(val);
	} else {
		// Emit refcount cleanup for all active scopes before returning.
		for (auto& scope : scopes) {
			for (const auto& varName : scope.heapOwned) {
				auto* alloca = scope.variables.at(varName);
				std::string bfType = scope.varBFTypeNames.at(varName);
				bool isMap = bfType.substr(0, 4) == "map<";
				emitRefDecrement(alloca, isMap);
			}
		}
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
	if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
		return generateIndex(*e);
	}
	if (auto* e = dynamic_cast<const MemberAccessExpr*>(&expr)) {
		return generateMemberAccess(*e);
	}
	if (dynamic_cast<const ThisExpr*>(&expr)) {
		// Load the this pointer.
		llvm::Type* thisType = lookupVarType("this");
		return builder->CreateLoad(thisType, lookupVariable("this"), "this.ptr");
	}
	if (auto* e = dynamic_cast<const InterpolatedStringExpr*>(&expr)) {
		return generateInterpolatedString(*e);
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
// Index expression (array[i])
// ==========================

llvm::Value* CodeGen::generateIndex(const IndexExpr& expr) {
	if (auto* ident = dynamic_cast<const IdentifierExpr*>(expr.object.get())) {
		std::string bfType = lookupVarBFTypeName(ident->name);

		// Array index
		if (bfType.substr(0, 6) == "array<") {
			std::string elemTypeName = bfType.substr(6, bfType.size() - 7);
			TypeNode tn(elemTypeName);
			llvm::Type* elemType = getLLVMType(tn);
			auto* arrAlloca = lookupVariable(ident->name);
			auto* arrType = getOrCreateArrayType(elemType);
			auto* ptrType = llvm::PointerType::getUnqual(*context);

			llvm::Value* data = builder->CreateLoad(ptrType,
				builder->CreateStructGEP(arrType, arrAlloca, 0), "data");
			llvm::Value* idx = generateExpression(*expr.index);
			llvm::Value* elemPtr = builder->CreateGEP(elemType, data, idx, "elem.ptr");
			return builder->CreateLoad(elemType, elemPtr, "elem");
		}

		// Map index
		if (bfType.substr(0, 4) == "map<") {
			auto commaPos = bfType.find(',', 4);
			std::string keyTypeName = bfType.substr(4, commaPos - 4);
			std::string valTypeName = bfType.substr(commaPos + 1, bfType.size() - commaPos - 2);

			TypeNode keyTN(keyTypeName);
			TypeNode valTN(valTypeName);
			llvm::Type* keyType = getLLVMType(keyTN);
			llvm::Type* valType = getLLVMType(valTN);

			auto* mapAlloca = lookupVariable(ident->name);
			llvm::Value* key = generateExpression(*expr.index);
			return generateMapGet(mapAlloca, keyType, valType, key);
		}
	}
	throw CodeGenError("Unsupported index expression");
}

// ==========================
// Member access (obj.field)
// ==========================

llvm::Value* CodeGen::generateMemberAccess(const MemberAccessExpr& expr) {
	auto [basePtr, structName] = resolveStructBase(*expr.object);

	auto structIt = structRegistry.find(structName);
	if (structIt != structRegistry.end()) {
		auto& info = structIt->second;
		auto fieldIt = info.fieldIndices.find(expr.member);
		if (fieldIt != info.fieldIndices.end()) {
			size_t idx = fieldIt->second;
			auto* fieldPtr = builder->CreateStructGEP(info.llvmType, basePtr, idx, expr.member + ".ptr");
			return builder->CreateLoad(info.fieldLLVMTypes[idx], fieldPtr, expr.member);
		}
		throw CodeGenError("Unknown field: " + expr.member + " on struct " + structName);
	}
	throw CodeGenError("Member access on non-struct type: " + structName);
}

// ==========================
// Interpolated string generation
// ==========================

llvm::Value* CodeGen::generateInterpolatedString(const InterpolatedStringExpr& expr) {
	// Build printf-style format string and collect argument values.
	std::string formatStr;
	std::vector<llvm::Value*> args;
	std::vector<bool> isBoolArg;

	for (size_t i = 0; i < expr.expressions.size(); i++) {
		// Escape % in fragment for printf.
		for (char c : expr.fragments[i]) {
			if (c == '%') formatStr += "%%";
			else formatStr += c;
		}

		llvm::Value* val = generateExpression(*expr.expressions[i]);

		if (val->getType()->isIntegerTy(1)) {
			// Bool: convert to string.
			llvm::Value* trueStr = builder->CreateGlobalStringPtr("true", "true.str");
			llvm::Value* falseStr = builder->CreateGlobalStringPtr("false", "false.str");
			val = builder->CreateSelect(val, trueStr, falseStr, "boolstr");
			formatStr += "%s";
		} else if (isStringType(val->getType())) {
			formatStr += "%s";
		} else if (isFloatType(val->getType())) {
			formatStr += "%g";
		} else if (val->getType()->isIntegerTy(8)) {
			formatStr += "%c";
		} else {
			formatStr += "%ld";
		}
		args.push_back(val);
	}

	// Add the final fragment.
	for (char c : expr.fragments.back()) {
		if (c == '%') formatStr += "%%";
		else formatStr += c;
	}

	// Use snprintf to build the string: first get length, then allocate and fill.
	llvm::Value* fmtStr = builder->CreateGlobalStringPtr(formatStr, "interp.fmt");
	auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
	auto* i64 = llvm::Type::getInt64Ty(*context);

	// snprintf(NULL, 0, fmt, ...) to get required length.
	std::vector<llvm::Value*> sizeArgs = {
		llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr)),
		llvm::ConstantInt::get(i64, 0),
		fmtStr
	};
	sizeArgs.insert(sizeArgs.end(), args.begin(), args.end());
	llvm::Value* len = builder->CreateCall(snprintfFunc, sizeArgs, "interp.len");

	// Allocate buffer: malloc(len + 1)
	llvm::Value* len64 = builder->CreateSExt(len, i64, "len64");
	llvm::Value* bufSize = builder->CreateAdd(len64, llvm::ConstantInt::get(i64, 1), "bufsize");
	llvm::Value* buf = builder->CreateCall(mallocFunc, {bufSize}, "interp.buf");

	// Fill buffer: snprintf(buf, bufSize, fmt, ...)
	std::vector<llvm::Value*> fillArgs = {buf, bufSize, fmtStr};
	fillArgs.insert(fillArgs.end(), args.begin(), args.end());
	builder->CreateCall(snprintfFunc, fillArgs);

	return buf;
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

	// Check for math stdlib functions.
	if (callee && stdlibMathNames().count(callee->name) &&
	    !overriddenMathFuncs_.count(callee->name)) {
		return generateMathCall(callee->name, expr.arguments);
	}

	// Check for constructor call: StructName(args...)
	if (callee) {
		auto structIt = structRegistry.find(callee->name);
		if (structIt != structRegistry.end() && structIt->second.hasConstructor) {
			auto& info = structIt->second;
			auto* fn = builder->GetInsertBlock()->getParent();
			auto alloca = createEntryBlockAlloca(fn, "ctor.tmp", info.llvmType);
			// Zero-init before calling constructor.
			builder->CreateStore(llvm::Constant::getNullValue(info.llvmType), alloca);
			// Call StructName.constructor(alloca, args...)
			llvm::Function* ctor = module->getFunction(callee->name + ".constructor");
			if (!ctor) throw CodeGenError("Constructor not found for " + callee->name);
			std::vector<llvm::Value*> args = {alloca};
			for (const auto& arg : expr.arguments) {
				args.push_back(generateExpression(*arg));
			}
			builder->CreateCall(ctor, args);
			// Return the struct by value (load from alloca).
			return builder->CreateLoad(info.llvmType, alloca, "ctor.result");
		}
	}

	// Check for method call: obj.method() or obj.push()/obj.length()
	if (auto* memberAccess = dynamic_cast<const MemberAccessExpr*>(expr.callee.get())) {
		std::string bfType;
		llvm::Value* objPtr = nullptr;

		if (auto* ident = dynamic_cast<const IdentifierExpr*>(memberAccess->object.get())) {
			// Namespace import qualified call: utils.abs(-7) where 'utils' was introduced
			// by 'import math.utils;'.  Resolved directly to the LLVM function; no stdlib
			// intercept, since the qualification makes intent unambiguous.
			if (namespaceNames_.count(ident->name)) {
				llvm::Function* fn = module->getFunction(memberAccess->member);
				if (!fn) throw CodeGenError(
					"Undefined function '" + memberAccess->member +
					"' in module namespace '" + ident->name + "'");
				std::vector<llvm::Value*> callArgs;
				for (const auto& arg : expr.arguments)
					callArgs.push_back(generateExpression(*arg));
				if (fn->getReturnType()->isVoidTy()) {
					builder->CreateCall(fn, callArgs);
					return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context));
				}
				return builder->CreateCall(fn, callArgs, "nscall");
			}

			bfType = lookupVarBFTypeName(ident->name);

			// Array built-ins
			if (bfType.substr(0, 6) == "array<") {
				std::string elemTypeName = bfType.substr(6, bfType.size() - 7);
				TypeNode tn(elemTypeName);
				llvm::Type* elemType = getLLVMType(tn);
				auto* arrAlloca = lookupVariable(ident->name);

				if (memberAccess->member == "length") {
					auto* arrType = getOrCreateArrayType(elemType);
					auto* lenPtr = builder->CreateStructGEP(arrType, arrAlloca, 1, "len.ptr");
					return builder->CreateLoad(llvm::Type::getInt64Ty(*context), lenPtr, "len");
				}
				if (memberAccess->member == "push") {
					if (expr.arguments.size() != 1) {
						throw CodeGenError("push() expects exactly 1 argument");
					}
					llvm::Value* val = generateExpression(*expr.arguments[0]);
					generateArrayPush(arrAlloca, elemType, val);
					return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0);
				}
				throw CodeGenError("Unknown array method: " + memberAccess->member);
			}

			// Struct method calls
			llvm::Type* varType = lookupVarType(ident->name);
			if (varType->isStructTy()) {
				objPtr = lookupVariable(ident->name);  // alloca IS the struct pointer
			} else {
				objPtr = builder->CreateLoad(varType, lookupVariable(ident->name), ident->name + ".ptr");
			}
		} else if (dynamic_cast<const ThisExpr*>(memberAccess->object.get())) {
			bfType = lookupVarBFTypeName("this");
			llvm::Type* thisType = lookupVarType("this");
			objPtr = builder->CreateLoad(thisType, lookupVariable("this"), "this.ptr");
		}

		// Look up the method in the struct registry.
		auto structIt = structRegistry.find(bfType);
		if (structIt != structRegistry.end()) {
			auto& info = structIt->second;
			auto methodIt = info.methods.find(memberAccess->member);
			if (methodIt != info.methods.end()) {
				llvm::Function* method = module->getFunction(methodIt->second);
				if (!method) throw CodeGenError("Undefined method: " + methodIt->second);

				std::vector<llvm::Value*> args = {objPtr};
				for (const auto& arg : expr.arguments) {
					args.push_back(generateExpression(*arg));
				}

				if (method->getReturnType()->isVoidTy()) {
					builder->CreateCall(method, args);
					return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0);
				}
				return builder->CreateCall(method, args, "methodcall");
			}
			throw CodeGenError("Unknown method: " + memberAccess->member + " on " + bfType);
		}
		throw CodeGenError("Method call on non-struct type: " + bfType);
	}

	// Look up the function in the module.
	std::string fnName;
	if (callee) {
		fnName = callee->name;
	} else {
		throw CodeGenError("Unsupported callee expression");
	}

	// Prefer a real function defined in the current module. Only resolve an
	// import alias when no function with the original source-level name exists,
	// so a local definition of e.g. 'myAbs' is never silently bypassed.
	llvm::Function* fn = module->getFunction(fnName);
	if (!fn) {
		auto aliasIt = importAliases_.find(fnName);
		if (aliasIt != importAliases_.end()) {
			fn = module->getFunction(aliasIt->second);
		}
	}
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
		// Check for print(array_var) — detect by BF type name
		if (auto* ident = dynamic_cast<const IdentifierExpr*>(arg.get())) {
			std::string bfType = lookupVarBFTypeName(ident->name);
			if (bfType.substr(0, 6) == "array<") {
				std::string elemTypeName = bfType.substr(6, bfType.size() - 7);
				TypeNode tn(elemTypeName);
				llvm::Type* elemType = getLLVMType(tn);
				auto* arrAlloca = lookupVariable(ident->name);
				generatePrintArray(arrAlloca, elemType);
				continue;
			}
			if (bfType.substr(0, 4) == "map<") {
				auto commaPos = bfType.find(',', 4);
				std::string keyTypeName = bfType.substr(4, commaPos - 4);
				std::string valTypeName = bfType.substr(commaPos + 1, bfType.size() - commaPos - 2);
				TypeNode keyTN(keyTypeName);
				TypeNode valTN(valTypeName);
				llvm::Type* keyType = getLLVMType(keyTN);
				llvm::Type* valType = getLLVMType(valTN);
				auto* mapAlloca = lookupVariable(ident->name);
				generatePrintMap(mapAlloca, keyType, valType);
				continue;
			}
		}

		// Special case: InterpolatedStringExpr inside print → use printf directly
		if (auto* interpStr = dynamic_cast<const InterpolatedStringExpr*>(arg.get())) {
			// Build format string with \n at the end.
			std::string formatStr;
			std::vector<llvm::Value*> printArgs;

			for (size_t i = 0; i < interpStr->expressions.size(); i++) {
				for (char c : interpStr->fragments[i]) {
					if (c == '%') formatStr += "%%";
					else formatStr += c;
				}

				llvm::Value* val = generateExpression(*interpStr->expressions[i]);

				if (val->getType()->isIntegerTy(1)) {
					llvm::Value* trueStr = builder->CreateGlobalStringPtr("true", "true.str");
					llvm::Value* falseStr = builder->CreateGlobalStringPtr("false", "false.str");
					val = builder->CreateSelect(val, trueStr, falseStr, "boolstr");
					formatStr += "%s";
				} else if (isStringType(val->getType())) {
					formatStr += "%s";
				} else if (isFloatType(val->getType())) {
					formatStr += "%g";
				} else if (val->getType()->isIntegerTy(8)) {
					formatStr += "%c";
				} else {
					formatStr += "%ld";
				}
				printArgs.push_back(val);
			}
			for (char c : interpStr->fragments.back()) {
				if (c == '%') formatStr += "%%";
				else formatStr += c;
			}
			formatStr += "\n";

			llvm::Value* fmtVal = builder->CreateGlobalStringPtr(formatStr, "interp.fmt");
			std::vector<llvm::Value*> allArgs = {fmtVal};
			allArgs.insert(allArgs.end(), printArgs.begin(), printArgs.end());
			builder->CreateCall(printfFunc, allArgs, "printf");
			continue;
		}

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

// ==========================
// Print array: [elem, elem, ...]
// ==========================

void CodeGen::generatePrintArray(llvm::AllocaInst* arrAlloca, llvm::Type* elemType) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* arrType = getOrCreateArrayType(elemType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	llvm::Value* data = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(arrType, arrAlloca, 0), "data");
	llvm::Value* len = builder->CreateLoad(i64,
		builder->CreateStructGEP(arrType, arrAlloca, 1), "len");

	// Print "["
	llvm::Value* openFmt = builder->CreateGlobalStringPtr("[", "arr.open");
	builder->CreateCall(printfFunc, {openFmt});

	// Loop: print each element
	auto* condBB = llvm::BasicBlock::Create(*context, "print.cond", fn);
	auto* bodyBB = llvm::BasicBlock::Create(*context, "print.body", fn);
	auto* doneBB = llvm::BasicBlock::Create(*context, "print.done", fn);

	builder->CreateBr(condBB);

	builder->SetInsertPoint(condBB);
	auto* iPhi = builder->CreatePHI(i64, 2, "i");
	iPhi->addIncoming(llvm::ConstantInt::get(i64, 0), condBB->getSinglePredecessor());
	llvm::Value* inBounds = builder->CreateICmpSLT(iPhi, len, "inbounds");
	builder->CreateCondBr(inBounds, bodyBB, doneBB);

	builder->SetInsertPoint(bodyBB);

	// Print separator if not first element
	auto* sepBB = llvm::BasicBlock::Create(*context, "print.sep", fn);
	auto* elemBB = llvm::BasicBlock::Create(*context, "print.elem", fn);
	llvm::Value* isFirst = builder->CreateICmpEQ(iPhi, llvm::ConstantInt::get(i64, 0), "isfirst");
	builder->CreateCondBr(isFirst, elemBB, sepBB);

	builder->SetInsertPoint(sepBB);
	llvm::Value* sepFmt = builder->CreateGlobalStringPtr(", ", "sep");
	builder->CreateCall(printfFunc, {sepFmt});
	builder->CreateBr(elemBB);

	builder->SetInsertPoint(elemBB);
	llvm::Value* elemPtr = builder->CreateGEP(elemType, data, iPhi, "elem.ptr");
	llvm::Value* elem = builder->CreateLoad(elemType, elemPtr, "elem");

	// Print the element based on type.
	if (isStringType(elemType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%s", "fmt");
		builder->CreateCall(printfFunc, {fmt, elem});
	} else if (isFloatType(elemType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%g", "fmt");
		builder->CreateCall(printfFunc, {fmt, elem});
	} else {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%ld", "fmt");
		builder->CreateCall(printfFunc, {fmt, elem});
	}

	llvm::Value* iNext = builder->CreateAdd(iPhi, llvm::ConstantInt::get(i64, 1), "i.next");
	iPhi->addIncoming(iNext, builder->GetInsertBlock());
	builder->CreateBr(condBB);

	builder->SetInsertPoint(doneBB);
	llvm::Value* closeFmt = builder->CreateGlobalStringPtr("]\n", "arr.close");
	builder->CreateCall(printfFunc, {closeFmt});
}

// ==========================
// Print map: {key: val, key: val, ...}
// ==========================

void CodeGen::generatePrintMap(llvm::AllocaInst* mapAlloca, llvm::Type* keyType, llvm::Type* valType) {
	auto* fn = builder->GetInsertBlock()->getParent();
	auto* mapType = getOrCreateMapType(keyType, valType);
	auto* i64 = llvm::Type::getInt64Ty(*context);
	auto* ptrType = llvm::PointerType::getUnqual(*context);

	llvm::Value* keys = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 0), "keys");
	llvm::Value* vals = builder->CreateLoad(ptrType,
		builder->CreateStructGEP(mapType, mapAlloca, 1), "vals");
	llvm::Value* len = builder->CreateLoad(i64,
		builder->CreateStructGEP(mapType, mapAlloca, 2), "len");

	// Print "{"
	llvm::Value* openFmt = builder->CreateGlobalStringPtr("{", "map.open");
	builder->CreateCall(printfFunc, {openFmt});

	auto* condBB = llvm::BasicBlock::Create(*context, "mprint.cond", fn);
	auto* bodyBB = llvm::BasicBlock::Create(*context, "mprint.body", fn);
	auto* doneBB = llvm::BasicBlock::Create(*context, "mprint.done", fn);

	builder->CreateBr(condBB);

	builder->SetInsertPoint(condBB);
	auto* iPhi = builder->CreatePHI(i64, 2, "i");
	iPhi->addIncoming(llvm::ConstantInt::get(i64, 0), condBB->getSinglePredecessor());
	llvm::Value* inBounds = builder->CreateICmpSLT(iPhi, len, "inbounds");
	builder->CreateCondBr(inBounds, bodyBB, doneBB);

	builder->SetInsertPoint(bodyBB);

	auto* sepBB = llvm::BasicBlock::Create(*context, "mprint.sep", fn);
	auto* kvBB = llvm::BasicBlock::Create(*context, "mprint.kv", fn);
	llvm::Value* isFirst = builder->CreateICmpEQ(iPhi, llvm::ConstantInt::get(i64, 0), "isfirst");
	builder->CreateCondBr(isFirst, kvBB, sepBB);

	builder->SetInsertPoint(sepBB);
	llvm::Value* sepFmt = builder->CreateGlobalStringPtr(", ", "sep");
	builder->CreateCall(printfFunc, {sepFmt});
	builder->CreateBr(kvBB);

	builder->SetInsertPoint(kvBB);

	// Print key
	llvm::Value* keyPtr = builder->CreateGEP(keyType, keys, iPhi, "key.ptr");
	llvm::Value* keyVal = builder->CreateLoad(keyType, keyPtr, "key");
	if (isStringType(keyType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%s: ", "kfmt");
		builder->CreateCall(printfFunc, {fmt, keyVal});
	} else if (isFloatType(keyType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%g: ", "kfmt");
		builder->CreateCall(printfFunc, {fmt, keyVal});
	} else {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%ld: ", "kfmt");
		builder->CreateCall(printfFunc, {fmt, keyVal});
	}

	// Print value
	llvm::Value* valPtr = builder->CreateGEP(valType, vals, iPhi, "val.ptr");
	llvm::Value* valVal = builder->CreateLoad(valType, valPtr, "val");
	if (isStringType(valType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%s", "vfmt");
		builder->CreateCall(printfFunc, {fmt, valVal});
	} else if (isFloatType(valType)) {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%g", "vfmt");
		builder->CreateCall(printfFunc, {fmt, valVal});
	} else {
		llvm::Value* fmt = builder->CreateGlobalStringPtr("%ld", "vfmt");
		builder->CreateCall(printfFunc, {fmt, valVal});
	}

	llvm::Value* iNext = builder->CreateAdd(iPhi, llvm::ConstantInt::get(i64, 1), "i.next");
	iPhi->addIncoming(iNext, builder->GetInsertBlock());
	builder->CreateBr(condBB);

	builder->SetInsertPoint(doneBB);
	llvm::Value* closeFmt = builder->CreateGlobalStringPtr("}\n", "map.close");
	builder->CreateCall(printfFunc, {closeFmt});
}

// ==========================
// Math stdlib
// ==========================

llvm::Value* CodeGen::generateMathCall(const std::string& name,
                                        const std::vector<ExprPtr>& args) {
	auto* f64 = llvm::Type::getDoubleTy(*context);
	auto* i64 = llvm::Type::getInt64Ty(*context);

	// Helper: get or create an LLVM intrinsic declaration for f64.
	auto getIntrinsic = [&](llvm::Intrinsic::ID id,
	                         std::vector<llvm::Type*> types = {}) -> llvm::Function* {
		if (types.empty()) types = {f64};
		return llvm::Intrinsic::getDeclaration(module.get(), id, types);
	};

	// --- abs(x) ---
	if (name == "abs") {
		if (args.size() != 1) throw CodeGenError("abs() requires exactly 1 argument");
		llvm::Value* val = generateExpression(*args[0]);
		if (val->getType()->isDoubleTy() || val->getType()->isFloatTy()) {
			if (!val->getType()->isDoubleTy()) val = builder->CreateFPExt(val, f64, "abs.promote");
			return builder->CreateCall(getIntrinsic(llvm::Intrinsic::fabs), {val}, "abs");
		}
		// Integer: abs via compare and negate.
		if (!val->getType()->isIntegerTy(64)) val = builder->CreateSExt(val, i64, "abs.ext");
		llvm::Value* neg = builder->CreateNeg(val, "abs.neg");
		llvm::Value* isNeg = builder->CreateICmpSLT(val, llvm::ConstantInt::get(i64, 0), "abs.isneg");
		return builder->CreateSelect(isNeg, neg, val, "abs");
	}

	// --- min(a, b) ---
	if (name == "min") {
		if (args.size() != 2) throw CodeGenError("min() requires exactly 2 arguments");
		llvm::Value* a = generateExpression(*args[0]);
		llvm::Value* b = generateExpression(*args[1]);
		bool isFloat = a->getType()->isDoubleTy() || a->getType()->isFloatTy() ||
		               b->getType()->isDoubleTy() || b->getType()->isFloatTy();
		if (isFloat) {
			if (a->getType()->isIntegerTy())      a = builder->CreateSIToFP(a, f64, "min.a");
			else if (!a->getType()->isDoubleTy()) a = builder->CreateFPExt(a, f64, "min.a");
			if (b->getType()->isIntegerTy())      b = builder->CreateSIToFP(b, f64, "min.b");
			else if (!b->getType()->isDoubleTy()) b = builder->CreateFPExt(b, f64, "min.b");
			return builder->CreateCall(getIntrinsic(llvm::Intrinsic::minnum), {a, b}, "min");
		}
		if (!a->getType()->isIntegerTy(64)) a = builder->CreateSExt(a, i64, "min.a");
		if (!b->getType()->isIntegerTy(64)) b = builder->CreateSExt(b, i64, "min.b");
		llvm::Value* cmp = builder->CreateICmpSLT(a, b, "min.cmp");
		return builder->CreateSelect(cmp, a, b, "min");
	}

	// --- max(a, b) ---
	if (name == "max") {
		if (args.size() != 2) throw CodeGenError("max() requires exactly 2 arguments");
		llvm::Value* a = generateExpression(*args[0]);
		llvm::Value* b = generateExpression(*args[1]);
		bool isFloat = a->getType()->isDoubleTy() || a->getType()->isFloatTy() ||
		               b->getType()->isDoubleTy() || b->getType()->isFloatTy();
		if (isFloat) {
			if (a->getType()->isIntegerTy())      a = builder->CreateSIToFP(a, f64, "max.a");
			else if (!a->getType()->isDoubleTy()) a = builder->CreateFPExt(a, f64, "max.a");
			if (b->getType()->isIntegerTy())      b = builder->CreateSIToFP(b, f64, "max.b");
			else if (!b->getType()->isDoubleTy()) b = builder->CreateFPExt(b, f64, "max.b");
			return builder->CreateCall(getIntrinsic(llvm::Intrinsic::maxnum), {a, b}, "max");
		}
		if (!a->getType()->isIntegerTy(64)) a = builder->CreateSExt(a, i64, "max.a");
		if (!b->getType()->isIntegerTy(64)) b = builder->CreateSExt(b, i64, "max.b");
		llvm::Value* cmp = builder->CreateICmpSGT(a, b, "max.cmp");
		return builder->CreateSelect(cmp, a, b, "max");
	}

	// --- pow(base, exp) ---
	if (name == "pow") {
		if (args.size() != 2) throw CodeGenError("pow() requires exactly 2 arguments");
		llvm::Value* base = generateExpression(*args[0]);
		llvm::Value* exp  = generateExpression(*args[1]);
		if (!base->getType()->isDoubleTy()) base = builder->CreateSIToFP(base, f64, "pow.base");
		if (!exp->getType()->isDoubleTy())  exp  = builder->CreateSIToFP(exp,  f64, "pow.exp");
		return builder->CreateCall(getIntrinsic(llvm::Intrinsic::pow, {f64}), {base, exp}, "pow");
	}

	// --- single-argument float intrinsics ---
	if (args.size() != 1) throw CodeGenError(name + "() requires exactly 1 argument");
	llvm::Value* val = generateExpression(*args[0]);
	if (!val->getType()->isDoubleTy()) {
		if (val->getType()->isIntegerTy())
			val = builder->CreateSIToFP(val, f64, name + ".conv");
		else
			val = builder->CreateFPExt(val, f64, name + ".conv");
	}

	if (name == "sin")   return builder->CreateCall(getIntrinsic(llvm::Intrinsic::sin),   {val}, "sin");
	if (name == "cos")   return builder->CreateCall(getIntrinsic(llvm::Intrinsic::cos),   {val}, "cos");
	if (name == "tan") {
		// tan has no LLVM intrinsic; use the C math library function.
		// Declare lazily to avoid symbol collision if user also defines 'overridden tan'.
		llvm::Function* tanFn = module->getFunction("tan");
		if (!tanFn) {
			auto tanType = llvm::FunctionType::get(f64, {f64}, false);
			tanFn = llvm::Function::Create(tanType, llvm::Function::ExternalLinkage, "tan", module.get());
		}
		return builder->CreateCall(tanFn, {val}, "tan");
	}
	if (name == "sqrt")  return builder->CreateCall(getIntrinsic(llvm::Intrinsic::sqrt),  {val}, "sqrt");
	if (name == "floor") return builder->CreateCall(getIntrinsic(llvm::Intrinsic::floor), {val}, "floor");
	if (name == "ceil")  return builder->CreateCall(getIntrinsic(llvm::Intrinsic::ceil),  {val}, "ceil");
	if (name == "round") return builder->CreateCall(getIntrinsic(llvm::Intrinsic::round), {val}, "round");
	if (name == "log")   return builder->CreateCall(getIntrinsic(llvm::Intrinsic::log),   {val}, "log");
	if (name == "log2")  return builder->CreateCall(getIntrinsic(llvm::Intrinsic::log2),  {val}, "log2");
	if (name == "log10") return builder->CreateCall(getIntrinsic(llvm::Intrinsic::log10), {val}, "log10");
	if (name == "exp")   return builder->CreateCall(getIntrinsic(llvm::Intrinsic::exp),   {val}, "exp");

	throw CodeGenError("Unknown math function: " + name);
}
