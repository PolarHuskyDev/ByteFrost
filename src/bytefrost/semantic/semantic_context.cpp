#include "bytefrost/semantic/semantic_context.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bytefrost {

// -----------------------------------------------------------------------
// stdlib math names (mirrors the set in CodeGen)
// -----------------------------------------------------------------------

const std::set<std::string>& SemanticContext::stdlibMathNames() {
	static const std::set<std::string> names = {
		"sin", "cos", "tan", "sqrt", "pow", "floor", "ceil", "round",
		"abs", "log", "log2", "log10", "exp", "min", "max"};
	return names;
}

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

SemanticContext::SemanticContext()
	: checker_(resolver_) {}

// -----------------------------------------------------------------------
// Top-level entry point
// -----------------------------------------------------------------------

bool SemanticContext::analyze(const Program& program,
                              const std::string& sourceFile) {
	sourceFile_ = sourceFile;
	diag_.clear();
	importedNames_.clear();
	structFieldInfo_.clear();

	// Pass 1: register all top-level names so forward references inside
	// function bodies resolve correctly.
	collectTopLevelDecls(program);
	checkDuplicateDecls(program);
	checkImportConflicts(program);
	checkMathOverrides(program);
	checkStructCycles(program);
	checkUninitializedStructFields(program);

	// Stop early — structural errors make body analysis unreliable.
	if (diag_.hasErrors())
		return false;

	// Pass 2: analyse function bodies.
	analyzeFunctions(program);
	analyzeStructMethods(program);

	return !diag_.hasErrors();
}

// -----------------------------------------------------------------------
// Pass 1 helpers
// -----------------------------------------------------------------------

void SemanticContext::collectTopLevelDecls(const Program& program) {
	for (const auto& ed : program.enums) {
		resolver_.registerEnum(ed->name);
		// Register each variant name so unqualified uses (e.g. SPADES) are valid.
		for (const auto& v : ed->variants)
			resolver_.registerEnumVariant(v.name, ed->name);
	}
	for (const auto& sd : program.structs) {
		resolver_.registerStruct(sd->name);
	}
	for (const auto& sd : program.structs) {
		auto& fieldMap = structFieldInfo_[sd->name];
		for (const auto& m : sd->members) {
			if (m.kind != StructMember::FIELD || !m.fieldType)
				continue;
			FieldSemInfo fi;
			fi.isReadonly = m.isReadonly;
			fi.isConstant = m.isConstant;
			fi.isNullable = m.fieldType->isNullable;
			fi.type = resolver_.resolve(*m.fieldType);
			// C.4: const fields cannot be nullable (parser also rejects this, but
			// guard here too for robustness).
			if (fi.isConstant && fi.isNullable) {
				diag_.error(loc(sd->line, sd->column),
				            "const field '" + m.fieldName + "' in struct '" +
				                sd->name + "' cannot be nullable");
			}
			fieldMap[m.fieldName] = std::move(fi);
		}
	}
	// Collect local names brought in by import statements.  We can't know
	// whether they're enums, structs, or functions without loading the imported
	// module, so we just record the names to suppress false-positive errors.
	for (const auto& imp : program.imports) {
		for (const auto& item : imp->items) {
			const std::string& localName = item.alias.empty() ? item.name : item.alias;
			importedNames_.insert(localName);
		}
	}
	// Register top-level functions in the global scope.
	for (const auto& fn : program.functions) {
		if (scopes_.isDeclaredInCurrentScope(fn->name))
			continue;  // duplicates reported separately
		BFType retType = fn->returnType ? resolver_.resolve(*fn->returnType)
		                               : BFType::makeVoid();
		scopes_.declare(fn->name, {fn->name, retType, false, ConstInitState::Uninitialized,		                           loc(fn->line, fn->column)});
	}
}

void SemanticContext::checkDuplicateDecls(const Program& program) {
	// Enum names
	std::unordered_map<std::string, int> enumLines;
	for (const auto& ed : program.enums) {
		if (enumLines.count(ed->name)) {
			diag_.error(loc(ed->line, ed->column),
			            "duplicate enum declaration '" + ed->name + "'")
			    .addNote(loc(enumLines[ed->name], 0),
			             "first declared here");
		} else {
			enumLines[ed->name] = ed->line;
		}
	}

	// Struct names (also check collision with enum names)
	std::unordered_map<std::string, int> structLines;
	for (const auto& sd : program.structs) {
		if (structLines.count(sd->name)) {
			diag_.error(loc(sd->line, sd->column),
			            "duplicate struct declaration '" + sd->name + "'")
			    .addNote(loc(structLines[sd->name], 0), "first declared here");
		} else if (enumLines.count(sd->name)) {
			diag_.error(loc(sd->line, sd->column),
			            "'" + sd->name + "' is already declared as an enum");
		} else {
			structLines[sd->name] = sd->line;
		}
	}

	// Top-level function names
	std::unordered_map<std::string, int> fnLines;
	for (const auto& fn : program.functions) {
		if (fnLines.count(fn->name)) {
			diag_.error(loc(fn->line, fn->column),
			            "duplicate function declaration '" + fn->name + "'")
			    .addNote(loc(fnLines[fn->name], 0), "first declared here");
		} else {
			fnLines[fn->name] = fn->line;
		}
	}
}

void SemanticContext::checkImportConflicts(const Program& program) {
	// An unaliased import of a stdlib math name creates an ambiguous call site.
	for (const auto& imp : program.imports) {
		for (const auto& item : imp->items) {
			if (item.alias.empty() && stdlibMathNames().count(item.name)) {
				diag_.error(loc(imp->line, imp->column),
				            "importing '" + item.name +
				                "' conflicts with the stdlib math function of the same name; "
				                "use an alias: import " + item.name + " as <alias> from ...");
			}
		}
	}
}

void SemanticContext::checkMathOverrides(const Program& program) {
	for (const auto& fn : program.functions) {
		if (stdlibMathNames().count(fn->name) && !fn->isOverridden) {
			diag_.error(loc(fn->line, fn->column),
			            "function '" + fn->name +
			                "' conflicts with a stdlib math function; "
			                "add 'overridden' to shadow it intentionally");
		}
	}
}

void SemanticContext::checkStructCycles(const Program& program) {
	// Build adjacency: struct name -> field type names that are also structs.
	std::unordered_map<std::string, std::vector<std::string>> deps;
	std::unordered_map<std::string, int> declLine;
	for (const auto& sd : program.structs) {
		declLine[sd->name] = sd->line;
		auto& d = deps[sd->name];
		for (const auto& m : sd->members) {
			if (m.kind == StructMember::FIELD &&
			    resolver_.isKnownStruct(m.fieldType->name)) {
				d.push_back(m.fieldType->name);
			}
		}
	}

	std::set<std::string> visited, inStack;
	std::function<void(const std::string&, std::vector<std::string>&)> dfs =
		[&](const std::string& name, std::vector<std::string>& path) {
			if (inStack.count(name)) {
				std::string cycle;
				bool inCycle = false;
				for (const auto& p : path) {
					if (p == name) inCycle = true;
					if (inCycle) cycle += p + " -> ";
				}
				cycle += name;
				int line = declLine.count(name) ? declLine.at(name) : 0;
				diag_.error(loc(line, 0),
				            "Cyclic struct dependency: " + cycle +
				                " (structs are value types; use Box<T> for indirection)");
				return;
			}
			if (visited.count(name)) return;
			visited.insert(name);
			inStack.insert(name);
			path.push_back(name);
			if (deps.count(name)) {
				for (const auto& dep : deps.at(name)) {
					dfs(dep, path);
				}
			}
			path.pop_back();
			inStack.erase(name);
		};

	for (const auto& sd : program.structs) {
		std::vector<std::string> path;
		dfs(sd->name, path);
	}
}

// -----------------------------------------------------------------------
// Check: struct fields of struct type must be initialized in a constructor
// -----------------------------------------------------------------------
//
// Rule: if a struct S has a field `f: T` where T is a locally-declared
// struct (not imported), S MUST have a constructor that assigns `this.f`.
// If S has no constructor at all, every such field is flagged.
// Imported struct types are excluded because we can't inspect their
// constructors without loading the imported module.
//
// Helper: collect all field names directly assigned via `this.name = …`
// anywhere in a statement list (recursively, but not into nested closures).
static void collectThisAssignments(const std::vector<std::unique_ptr<Statement>>& stmts,
                                   std::set<std::string>& assigned) {
	for (const auto& stmt : stmts) {
		// AssignStmt: target may be `this.field` or `this.field.subfield` etc.
		if (auto* as = dynamic_cast<const AssignStmt*>(stmt.get())) {
			if (auto* ma = dynamic_cast<const MemberAccessExpr*>(as->target.get())) {
				if (dynamic_cast<const ThisExpr*>(ma->object.get())) {
					assigned.insert(ma->member);
				}
			}
		}
		// Walk into if/while/for bodies.
		if (auto* ifs = dynamic_cast<const IfStmt*>(stmt.get())) {
			collectThisAssignments(ifs->thenBlock.statements, assigned);
			if (ifs->elseBlock)
				collectThisAssignments(ifs->elseBlock->statements, assigned);
		}
		if (auto* ws = dynamic_cast<const WhileStmt*>(stmt.get())) {
			collectThisAssignments(ws->body.statements, assigned);
		}
		if (auto* fs = dynamic_cast<const ForStmt*>(stmt.get())) {
			collectThisAssignments(fs->body.statements, assigned);
		}
	}
}

void SemanticContext::checkUninitializedStructFields(const Program& program) {
	// Build the set of locally-declared struct names (not from imports).
	std::unordered_set<std::string> localStructs;
	for (const auto& sd : program.structs)
		localStructs.insert(sd->name);

	for (const auto& sd : program.structs) {
		// Collect fields whose type is a locally-known struct.
		std::vector<std::pair<std::string, int>> structFields;  // (name, line)
		for (const auto& m : sd->members) {
			if (m.kind != StructMember::FIELD)
				continue;
			if (!m.fieldType)
				continue;
			if (localStructs.count(m.fieldType->name))
				structFields.emplace_back(m.fieldName, 0);
		}

		if (structFields.empty())
			continue;

		// Find the constructor method, if any.  If no constructor exists,
		// the struct is expected to be initialized via struct-literal syntax
		// ({ field: val, … }) which is validated at the call site by CodeGen.
		// We only check when a constructor IS present.
		const FunctionDecl* ctor = nullptr;
		for (const auto& m : sd->members) {
			if (m.kind == StructMember::METHOD && m.method &&
			    m.method->name == "constructor") {
				ctor = m.method.get();
				break;
			}
		}

		if (!ctor)
			continue;

		// Has a constructor: check each struct-typed field is assigned.
		std::set<std::string> assigned;
		collectThisAssignments(ctor->body.statements, assigned);

		for (const auto& [fname, fline] : structFields) {
			if (!assigned.count(fname)) {
				diag_.error(loc(ctor->line, ctor->column),
				            "constructor of '" + sd->name + "' does not initialize "
				                "field '" + fname + "' of struct type; "
				                "assign 'this." + fname + "' before use");
			}
		}
	}
}

// -----------------------------------------------------------------------
// Pass 2: function body analysis
// -----------------------------------------------------------------------

void SemanticContext::analyzeFunctions(const Program& program) {
	for (const auto& fn : program.functions) {
		analyzeFunction(*fn, "");
	}
}

void SemanticContext::analyzeStructMethods(const Program& program) {
	for (const auto& sd : program.structs) {
		for (const auto& m : sd->members) {
			if (m.kind == StructMember::METHOD) {
				analyzeFunction(*m.method, sd->name);
			}
		}
	}
}

void SemanticContext::analyzeFunction(const FunctionDecl& fn,
                                      const std::string& thisTypeName) {
	const std::string prevFunctionName = currentFunctionName_;
	const std::string prevThisTypeName = currentThisTypeName_;
	currentFunctionName_ = fn.name;
	currentThisTypeName_ = thisTypeName;

	scopes_.pushScope();

	// Inject 'this' for methods.
	if (!thisTypeName.empty()) {
		scopes_.declare("this", {"this", BFType::makeStruct(thisTypeName), false, ConstInitState::Uninitialized, {}});
	}

	// Declare parameters.
	for (const auto& param : fn.params) {
		BFType pType = param.type ? resolver_.resolve(*param.type)
		                          : BFType::makeUnknown();
		if (pType.isUnknown() && param.type &&
		    !importedNames_.count(param.type->name)) {
			diag_.error(loc(fn.line, fn.column),
			            "unknown type '" + param.type->name +
			                "' for parameter '" + param.name + "'");
		}
		scopes_.declare(param.name, {param.name, pType, false, ConstInitState::Uninitialized,		                             loc(fn.line, fn.column)});
	}

	analyzeBlock(fn.body);
	scopes_.popScope();

	currentFunctionName_ = prevFunctionName;
	currentThisTypeName_ = prevThisTypeName;
}

void SemanticContext::analyzeBlock(const Block& block) {
	scopes_.pushScope();
	for (const auto& stmt : block.statements) {
		analyzeStatement(*stmt);
	}
	scopes_.popScope();
}

// -----------------------------------------------------------------------
// Statement analysis
// -----------------------------------------------------------------------

void SemanticContext::analyzeStatement(const Statement& stmt) {
	if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
		BFType initType = BFType::makeUnknown();
		if (s->initializer) {
			initType = analyzeExpression(*s->initializer);
		}

		BFType declType = BFType::makeUnknown();
		if (s->type) {
			declType = resolver_.resolve(*s->type);
			if (declType.isUnknown() &&
			    !importedNames_.count(s->type->name)) {
				diag_.error(loc(s->line, s->column),
				            "unknown type '" + s->type->name + "'");
			}
			// Check initializer is compatible with declared type.
			if (s->initializer && !declType.isUnknown()) {
				if (dynamic_cast<const NullLiteralExpr*>(s->initializer.get()) && !declType.isNullable()) {
					diag_.error(loc(s->line, s->column),
					            "cannot assign null to non-nullable type '" + declType.toString() + "'");
				} else if (!initType.isUnknown() && !checker_.isAssignable(initType, declType)) {
					diag_.error(loc(s->line, s->column),
					            "cannot assign value of type '" + initType.toString() +
					                "' to variable of type '" + declType.toString() + "'");
				}
				// C.1: Validate struct literal completeness.
				if (declType.isStruct()) {
					if (auto* si = dynamic_cast<const StructInitExpr*>(s->initializer.get()))
						checkStructInit(declType.name, *si, s->line, s->column);
				}
			}
		} else {
			// Walrus / inferred: use initializer type.
			declType = initType;
		}

		if (s->isConstant) {
			if (declType.isNullable()) {
				diag_.error(loc(s->line, s->column),
				            "const declarations cannot have nullable type");
			}
			if (s->initializer && !isCompileTimeConstantExpr(*s->initializer)) {
				diag_.error(loc(s->line, s->column),
				            "const initializer is not a compile-time constant");
			}
		}

		// Check for shadowing in the same scope.
		if (scopes_.isDeclaredInCurrentScope(s->name)) {
			diag_.error(loc(s->line, s->column),
			            "variable '" + s->name + "' already declared in this scope");
		} else {
			ConstInitState initState = (s->isConstant && s->initializer)
			                        ? ConstInitState::InitializedOnce
			                        : ConstInitState::Uninitialized;
			scopes_.declare(s->name, {s->name, declType, s->isConstant, initState,
			                          loc(s->line, s->column)});
		}
		return;
	}

	if (auto* s = dynamic_cast<const AssignStmt*>(&stmt)) {
		if (auto* id = dynamic_cast<const IdentifierExpr*>(s->target.get())) {
			if (auto* sym = scopes_.lookupMutable(id->name); sym && sym->isConstant) {
				if (s->op != "=") {
					diag_.error(loc(s->line, s->column),
					            "cannot apply assignment operator '" + s->op +
					                "' to constant '" + id->name + "'");
				} else if (sym->constInitState == ConstInitState::InitializedOnce) {
					diag_.error(loc(s->line, s->column),
					            "cannot reassign constant '" + id->name + "'");
				} else {
					if (!isCompileTimeConstantExpr(*s->value)) {
						diag_.error(loc(s->line, s->column),
						            "const initializer is not a compile-time constant");
					}
					sym->constInitState = ConstInitState::InitializedOnce;
				}
			}
		}

		if (auto* ma = dynamic_cast<const MemberAccessExpr*>(s->target.get())) {
			BFType objType = analyzeExpression(*ma->object);
			if (objType.isStruct()) {
				auto sit = structFieldInfo_.find(objType.name);
				if (sit != structFieldInfo_.end()) {
					auto fit = sit->second.find(ma->member);
					if (fit != sit->second.end()) {
						const bool isThisFieldAssign = dynamic_cast<const ThisExpr*>(ma->object.get()) != nullptr;
						const bool isOwningCtor = currentFunctionName_ == "constructor" &&
						                         !currentThisTypeName_.empty() &&
						                         currentThisTypeName_ == objType.name;
						const bool allowReadonlyInit = s->op == "=" && isThisFieldAssign && isOwningCtor;
						if (fit->second.isConstant) {
							diag_.error(loc(s->line, s->column),
							            "cannot assign to const field '" + ma->member + "'");
						}
						if (fit->second.isReadonly && !allowReadonlyInit) {
							diag_.error(loc(s->line, s->column),
							            "cannot assign to readonly field '" + ma->member + "'");
						}
					}
				}
			}
		}

		BFType rhs = analyzeExpression(*s->value);
		BFType lhs = analyzeExpression(*s->target);
		if (!lhs.isUnknown()) {
			if (dynamic_cast<const NullLiteralExpr*>(s->value.get()) && !lhs.isNullable()) {
				diag_.error(loc(s->line, s->column),
				            "cannot assign null to non-nullable type '" + lhs.toString() + "'");
			} else if (!rhs.isUnknown() && !checker_.isAssignable(rhs, lhs)) {
				diag_.error(loc(s->line, s->column),
				            "cannot assign '" + rhs.toString() + "' to '" +
				                lhs.toString() + "'");
			}
			// C.1: Validate struct literal completeness on assignment.
			if (lhs.isStruct()) {
				if (auto* si = dynamic_cast<const StructInitExpr*>(s->value.get()))
					checkStructInit(lhs.name, *si, s->line, s->column);
			}
		}
		return;
	}

	if (auto* s = dynamic_cast<const IfStmt*>(&stmt)) {
		BFType cond = analyzeExpression(*s->condition);
		// Nullable types are allowed as truthy/falsy conditions (non-null ⇒ true).
		if (!cond.isUnknown() && !cond.isBool() && !cond.isNullable()) {
			diag_.error(loc(s->line, s->column),
			            "if condition must be bool, got '" + cond.toString() + "'");
		}

		// D.4: Null-narrowing — detect patterns like `x != null`, `x == null`,
		// or just `x` (truthy check on a nullable).  Within the appropriate
		// block, shadow the variable with its non-nullable type so member
		// access etc. resolves correctly.
		std::string narrowVar;
		BFType      narrowedType;
		bool        narrowInThen = false;
		bool        hasNarrowing = false;

		auto tryNarrow = [&](const std::string& name, bool inThen) {
			const auto* sym = scopes_.lookup(name);
			if (sym && sym->type.isNullable() && sym->type.elemType) {
				narrowVar    = name;
				narrowedType = *sym->type.elemType;
				narrowInThen = inThen;
				hasNarrowing = true;
			}
		};

		if (auto* bin = dynamic_cast<const BinaryExpr*>(s->condition.get())) {
			if (bin->op == "!=" || bin->op == "==") {
				if (auto* id = dynamic_cast<const IdentifierExpr*>(bin->left.get()))
					if (dynamic_cast<const NullLiteralExpr*>(bin->right.get()))
						tryNarrow(id->name, bin->op == "!=");
				if (!hasNarrowing)
					if (auto* id = dynamic_cast<const IdentifierExpr*>(bin->right.get()))
						if (dynamic_cast<const NullLiteralExpr*>(bin->left.get()))
							tryNarrow(id->name, bin->op == "!=");
			}
		} else if (auto* id = dynamic_cast<const IdentifierExpr*>(s->condition.get())) {
			tryNarrow(id->name, /*inThen=*/true);
		}

		// Analyse then-block (with narrowed type if applicable).
		if (hasNarrowing && narrowInThen)
			analyzeBlockWithNarrowing(s->thenBlock, narrowVar, narrowedType);
		else
			analyzeBlock(s->thenBlock);

		for (const auto& [eicond, eibody] : s->elseIfBlocks) {
			BFType eiType = analyzeExpression(*eicond);
			if (!eiType.isUnknown() && !eiType.isBool()) {
				diag_.error(loc(s->line, s->column),
				            "elseif condition must be bool, got '" + eiType.toString() + "'");
			}
			analyzeBlock(eibody);
		}

		// Analyse else-block (with narrowed type for `== null` case).
		if (s->elseBlock) {
			if (hasNarrowing && !narrowInThen)
				analyzeBlockWithNarrowing(*s->elseBlock, narrowVar, narrowedType);
			else
				analyzeBlock(*s->elseBlock);
		}
		return;
	}

	if (auto* s = dynamic_cast<const WhileStmt*>(&stmt)) {
		BFType cond = analyzeExpression(*s->condition);
		if (!cond.isUnknown() && !cond.isBool()) {
			diag_.error(loc(s->line, s->column),
			            "while condition must be bool, got '" + cond.toString() + "'");
		}
		analyzeBlock(s->body);
		return;
	}

	if (auto* s = dynamic_cast<const ForStmt*>(&stmt)) {
		scopes_.pushScope();
		if (s->init)      analyzeStatement(*s->init);
		if (s->condition) analyzeExpression(*s->condition);
		if (s->update)    analyzeExpression(*s->update);
		analyzeBlock(s->body);
		scopes_.popScope();
		return;
	}

	if (auto* s = dynamic_cast<const ForInStmt*>(&stmt)) {
		scopes_.pushScope();
		BFType rangeType = BFType::makeUnknown();
		if (s->range)
			rangeType = analyzeExpression(*s->range);

		if (!rangeType.isUnknown() && !rangeType.isArray() && !rangeType.isMap()) {
			diag_.error(loc(s->line, s->column),
			            "for-in range must be iterable (array/map), got '" +
			                rangeType.toString() + "'");
		}

		BFType inferredType = BFType::makeUnknown();
		if (rangeType.isArray() && rangeType.elemType)
			inferredType = *rangeType.elemType;
		else if (rangeType.isMap() && rangeType.valueType)
			inferredType = *rangeType.valueType;

		BFType varType = s->varType ? resolver_.resolve(*s->varType) : inferredType;
		if (varType.isUnknown())
			varType = BFType::makeInt();

		if (s->varType && !inferredType.isUnknown() && !checker_.isAssignable(inferredType, varType)) {
			diag_.error(loc(s->line, s->column),
			            "for-in variable type '" + varType.toString() +
			                "' is incompatible with iterable element type '" + inferredType.toString() + "'");
		}

		scopes_.declare(s->varName, {s->varName, varType, false, ConstInitState::Uninitialized,		                             loc(s->line, s->column)});
		analyzeBlock(s->body);
		scopes_.popScope();
		return;
	}

	if (auto* s = dynamic_cast<const MatchStmt*>(&stmt)) {
		analyzeExpression(*s->subject);
		for (const auto& c : s->cases) {
			for (const auto& p : c.patterns) analyzeExpression(*p);
			analyzeBlock(c.body);
		}
		return;
	}

	if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) {
		if (s->value) analyzeExpression(*s->value);
		return;
	}

	if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
		analyzeExpression(*s->expression);
		return;
	}

	// BreakStmt / ContinueStmt: no type work needed.
}

// -----------------------------------------------------------------------
// Expression analysis
// -----------------------------------------------------------------------

BFType SemanticContext::analyzeExpression(const Expression& expr) {
	if (dynamic_cast<const IntLiteralExpr*>(&expr))
		return BFType::makeInt();
	if (dynamic_cast<const FloatLiteralExpr*>(&expr))
		return BFType::makeFloat();
	if (dynamic_cast<const StringLiteralExpr*>(&expr))
		return BFType::makeString();
	if (dynamic_cast<const BoolLiteralExpr*>(&expr))
		return BFType::makeBool();
	if (dynamic_cast<const CharLiteralExpr*>(&expr))
		return BFType::makeChar();
	if (dynamic_cast<const NullLiteralExpr*>(&expr))
		return BFType::makeUnknown();  // null is compatible with any nullable
	if (dynamic_cast<const InterpolatedStringExpr*>(&expr)) {
		const auto& interp = *dynamic_cast<const InterpolatedStringExpr*>(&expr);
		for (const auto& e : interp.expressions) analyzeExpression(*e);
		return BFType::makeString();
	}
	if (dynamic_cast<const ThisExpr*>(&expr)) {
		const auto* sym = scopes_.lookup("this");
		return sym ? sym->type : BFType::makeUnknown();
	}

	if (auto* e = dynamic_cast<const IdentifierExpr*>(&expr)) {
		// Enum type name used as a namespace (e.g. CardRanks.ACE handled below)
		if (resolver_.isKnownEnum(e->name))
			return BFType::makeEnum(e->name);
		if (resolver_.isKnownStruct(e->name))
			return BFType::makeStruct(e->name);
		// Unqualified enum variant (e.g. SPADES in struct initializers).
		const std::string* variantEnum = resolver_.enumForVariant(e->name);
		if (variantEnum)
			return BFType::makeEnum(*variantEnum);
		const auto* sym = scopes_.lookup(e->name);
		if (!sym) {
			// Suppress error for names brought in via import — the cross-module
			// linker resolves them; we just return Unknown to avoid cascading.
			if (importedNames_.count(e->name))
				return BFType::makeUnknown();
			diag_.error(loc(e->line, e->column),
			            "undefined variable '" + e->name + "'");
			return BFType::makeUnknown();
		}
		return sym->type;
	}

	if (auto* e = dynamic_cast<const BinaryExpr*>(&expr))
		return analyzeBinary(*e);
	if (auto* e = dynamic_cast<const UnaryExpr*>(&expr))
		return analyzeUnary(*e);
	if (auto* e = dynamic_cast<const CallExpr*>(&expr))
		return analyzeCall(*e);
	if (auto* e = dynamic_cast<const MemberAccessExpr*>(&expr))
		return analyzeMemberAccess(*e);
	if (auto* e = dynamic_cast<const NullSafeAccessExpr*>(&expr))
		return analyzeNullSafeAccess(*e);
	if (auto* e = dynamic_cast<const IndexExpr*>(&expr))
		return analyzeIndex(*e);

	if (auto* e = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
		BFType elemType = BFType::makeUnknown();
		for (const auto& elem : e->elements)
			elemType = analyzeExpression(*elem);
		return BFType::makeArray(elemType);
	}

	if (auto* e = dynamic_cast<const StructInitExpr*>(&expr)) {
		for (const auto& [_, fieldExpr] : e->fields)
			analyzeExpression(*fieldExpr);
		return BFType::makeUnknown();  // type determined by assignment context
	}

	if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
		analyzeExpression(*e->target);
		analyzeExpression(*e->value);
		return BFType::makeUnknown();
	}

	return BFType::makeUnknown();
}

// -----------------------------------------------------------------------
// Expression sub-analysers
// -----------------------------------------------------------------------

BFType SemanticContext::analyzeBinary(const BinaryExpr& expr) {
	BFType lhs = analyzeExpression(*expr.left);
	BFType rhs = analyzeExpression(*expr.right);
	if (!checker_.isBinaryOpValid(expr.op, lhs, rhs)) {
		diag_.error(loc(expr.line, expr.column),
		            "operator '" + expr.op + "' cannot be applied to '" +
		                lhs.toString() + "' and '" + rhs.toString() + "'");
		return BFType::makeUnknown();
	}
	return checker_.inferBinaryResult(expr.op, lhs, rhs);
}

BFType SemanticContext::analyzeUnary(const UnaryExpr& expr) {
	BFType operand = analyzeExpression(*expr.operand);
	if (!checker_.isUnaryOpValid(expr.op, operand)) {
		diag_.error(loc(expr.line, expr.column),
		            "operator '" + expr.op + "' cannot be applied to '" +
		                operand.toString() + "'");
		return BFType::makeUnknown();
	}
	return checker_.inferUnaryResult(expr.op, operand);
}

BFType SemanticContext::analyzeCall(const CallExpr& expr) {
	// Analyse all arguments regardless of whether the callee resolves.
	for (const auto& arg : expr.arguments)
		analyzeExpression(*arg);

	// Simple identifier callee: look up return type if known.
	if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
		// Builtin functions: print, input, srand, rand.
		if (id->name == "print" || id->name == "srand")
			return BFType::makeVoid();
		if (id->name == "input")
			return BFType::makeString();
		if (id->name == "rand")
			return BFType::makeInt();
		// Stdlib math functions return float.
		if (stdlibMathNames().count(id->name))
			return BFType::makeFloat();
		// Struct constructor call: Deck()
		if (resolver_.isKnownStruct(id->name) &&
		    !importedNames_.count(id->name))
			return BFType::makeStruct(id->name);
		// Enum type used as a function — not valid, but don't crash here.
		if (resolver_.isKnownEnum(id->name))
			return BFType::makeUnknown();
		// Look up in scope (user-defined functions are declared in global scope).
		const auto* sym = scopes_.lookup(id->name);
		return sym ? sym->type : BFType::makeUnknown();
	}

	// Member call (method invocation or namespace call) — return unknown;
	// method return types are tracked per-struct (not yet in Phase 1 scope).
	if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.callee.get())) {
		analyzeExpression(*ma->object);
	}

	return BFType::makeUnknown();
}

BFType SemanticContext::analyzeMemberAccess(const MemberAccessExpr& expr) {
	BFType obj = analyzeExpression(*expr.object);

	// Enum variant access: CardRanks.ACE
	if (obj.isEnum())
		return BFType::makeEnum(obj.name);

	// Struct field access.
	if (obj.isStruct()) {
		auto sit = structFieldInfo_.find(obj.name);
		if (sit != structFieldInfo_.end()) {
			auto fit = sit->second.find(expr.member);
			if (fit != sit->second.end())
				return fit->second.type;
		}
		return BFType::makeUnknown();
	}

	// Array built-in members: arr.length() → int (caller wraps in CallExpr)
	if (obj.isArray())
		return BFType::makeUnknown();

	return BFType::makeUnknown();
}

BFType SemanticContext::analyzeNullSafeAccess(const NullSafeAccessExpr& expr) {
	BFType objType = analyzeExpression(*expr.object);
	if (!objType.isUnknown() && !objType.isNullable()) {
		diag_.error(loc(expr.line, expr.column),
		            "null-safe access requires nullable object, got '" + objType.toString() + "'");
	}

	BFType fallbackType = analyzeExpression(*expr.fallback);

	if (objType.isNullable() && objType.elemType && objType.elemType->isStruct()) {
		const std::string& structName = objType.elemType->name;
		auto sit = structFieldInfo_.find(structName);
		if (sit != structFieldInfo_.end()) {
			auto fit = sit->second.find(expr.field);
			if (fit == sit->second.end()) {
				diag_.error(loc(expr.line, expr.column),
				            "unknown field '" + expr.field + "' on struct '" + structName + "'");
				return BFType::makeUnknown();
			}

			const BFType& fieldType = fit->second.type;
			if (!fallbackType.isUnknown() && !fieldType.isUnknown()
			    && !checker_.isAssignable(fallbackType, fieldType)) {
				diag_.error(loc(expr.line, expr.column),
				            "null-safe fallback type '" + fallbackType.toString() +
				                "' is incompatible with field type '" + fieldType.toString() + "'");
			}
			return fieldType;
		}
	}

	return fallbackType.isUnknown() ? BFType::makeUnknown() : fallbackType;
}

bool SemanticContext::isCompileTimeConstantExpr(const Expression& expr) const {
	if (dynamic_cast<const IntLiteralExpr*>(&expr)
	    || dynamic_cast<const FloatLiteralExpr*>(&expr)
	    || dynamic_cast<const StringLiteralExpr*>(&expr)
	    || dynamic_cast<const BoolLiteralExpr*>(&expr)
	    || dynamic_cast<const CharLiteralExpr*>(&expr)
	    || dynamic_cast<const NullLiteralExpr*>(&expr)) {
		return true;
	}

	if (auto* u = dynamic_cast<const UnaryExpr*>(&expr))
		return isCompileTimeConstantExpr(*u->operand);

	if (auto* b = dynamic_cast<const BinaryExpr*>(&expr))
		return isCompileTimeConstantExpr(*b->left) && isCompileTimeConstantExpr(*b->right);

	if (auto* a = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
		for (const auto& e : a->elements)
			if (!isCompileTimeConstantExpr(*e))
				return false;
		return true;
	}

	if (auto* s = dynamic_cast<const StructInitExpr*>(&expr)) {
		for (const auto& [_, e] : s->fields)
			if (!isCompileTimeConstantExpr(*e))
				return false;
		return true;
	}

	if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
		const auto* sym = scopes_.lookup(id->name);
		return sym && sym->isConstant && sym->constInitState == ConstInitState::InitializedOnce;
	}

	return false;
}

BFType SemanticContext::analyzeIndex(const IndexExpr& expr) {
	BFType obj = analyzeExpression(*expr.object);
	analyzeExpression(*expr.index);

	if (obj.isArray() && obj.elemType)
		return *obj.elemType;

	if (obj.isMap() && obj.valueType)
		return *obj.valueType;

	return BFType::makeUnknown();
}

// -----------------------------------------------------------------------
// C.1: checkStructInit
// -----------------------------------------------------------------------

void SemanticContext::checkStructInit(const std::string& structName,
                                      const StructInitExpr& initExpr,
                                      int line, int col) {
	auto sit = structFieldInfo_.find(structName);
	if (sit == structFieldInfo_.end())
		return;  // unknown struct — error reported elsewhere

	// Collect provided field names.
	std::unordered_set<std::string> provided;
	for (const auto& [fname, _] : initExpr.fields)
		provided.insert(fname);

	// Report unknown fields first.
	for (const auto& [fname, _] : initExpr.fields) {
		if (!sit->second.count(fname)) {
			diag_.error(loc(line, col),
			            "unknown field '" + fname + "' in struct '" + structName + "'");
		}
	}

	// Every non-nullable field must be present.
	for (const auto& [fname, finfo] : sit->second) {
		if (!finfo.isNullable && !provided.count(fname)) {
			diag_.error(loc(line, col),
			            "non-nullable field '" + fname + "' of struct '" + structName +
			                "' must be initialized in struct literal");
		}
	}
}

// -----------------------------------------------------------------------
// D.4: analyzeBlockWithNarrowing
// -----------------------------------------------------------------------

void SemanticContext::analyzeBlockWithNarrowing(const Block& block,
                                                 const std::string& varName,
                                                 const BFType& narrowedType) {
	scopes_.pushScope();
	// Shadow the nullable variable with its narrowed (non-nullable) type so
	// that code inside this block can access it without null-safety noise.
	scopes_.declare(varName,
	                {varName, narrowedType, false, ConstInitState::Uninitialized, {}});
	for (const auto& stmt : block.statements)
		analyzeStatement(*stmt);
	scopes_.popScope();
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

SourceLocation SemanticContext::loc(int line, int col) const {
	return {sourceFile_, line, col};
}

}  // namespace bytefrost
