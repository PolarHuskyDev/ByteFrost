#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "bytefrost/diagnostics/diagnostic.h"
#include "bytefrost/semantic/bf_type.h"

namespace bytefrost {

/// Track the initialization state of const declarations for deferred initialization support.
/// Allows declaring a const and assigning it in one place, but never reassigning.
enum class ConstInitState {
	Uninitialized,     // const X; declared but not yet assigned
	InitializedOnce,   // const X = val; or previously declared const assigned once
	Reassigned,        // ERROR state: const assigned more than once
};

/// Metadata stored per declared symbol.
/// Phase 3: Enhanced with const tracking and nullability support
struct SymbolInfo {
	std::string    name;
	BFType         type;
	
	/// True if this is a const declaration (immutable by definition).
	/// Variables are mutable by default, so no mutable flag needed.
	bool           isConstant = false;
	
	/// Tracks const initialization state for deferred const initialization.
	/// Allows: const X; ... X = 5;  (one assignment)
	/// Prevents: const X; X = 5; X = 10;  (multiple assignments)
	ConstInitState constInitState = ConstInitState::Uninitialized;
	
	SourceLocation declaredAt;

	// -----------------------------------------------------------------------
	// Helper predicates (Phase 3)
	// -----------------------------------------------------------------------

	bool isConst() const { return isConstant; }
	bool isMutable() const { return !isConstant; }  // variables mutable by default
};

/// ScopeManager maintains a stack of lexical scopes for symbol resolution.
/// Mirrors the runtime scope stack in CodeGen but at the semantic (type) level.
class ScopeManager {
public:
	ScopeManager();

	/// Push a new lexical scope (function entry, block, etc.).
	void pushScope();

	/// Pop the innermost scope. Must be balanced with pushScope().
	void popScope();

	/// Current nesting depth (0 = global).
	size_t depth() const { return scopes_.size(); }

	/// Declare a symbol in the current (innermost) scope.
	/// Emits nothing — callers must check isDeclaredInCurrentScope first to
	/// report duplicate errors before calling this.
	void declare(const std::string& name, SymbolInfo info);

	/// Look up a symbol by name, searching from innermost scope outward.
	/// Returns nullptr if not found.
	const SymbolInfo* lookup(const std::string& name) const;

	/// Mutable lookup variant used when semantic analysis needs to update
	/// symbol metadata (e.g. const initialization state).
	SymbolInfo* lookupMutable(const std::string& name);

	/// Returns true if name is already declared in the current (innermost) scope.
	bool isDeclaredInCurrentScope(const std::string& name) const;

private:
	using ScopeMap = std::unordered_map<std::string, SymbolInfo>;
	std::vector<ScopeMap> scopes_;
};

}  // namespace bytefrost
