#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "bytefrost/diagnostics/diagnostic.h"
#include "bytefrost/semantic/bf_type.h"

namespace bytefrost {

/// Metadata stored per declared symbol.
struct SymbolInfo {
	std::string    name;
	BFType         type;
	bool           isMutable  = true;   // future: immutable-by-default (Phase 3)
	bool           isConst    = false;  // future: const keyword (Phase 3)
	SourceLocation declaredAt;
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

	/// Returns true if name is already declared in the current (innermost) scope.
	bool isDeclaredInCurrentScope(const std::string& name) const;

private:
	using ScopeMap = std::unordered_map<std::string, SymbolInfo>;
	std::vector<ScopeMap> scopes_;
};

}  // namespace bytefrost
