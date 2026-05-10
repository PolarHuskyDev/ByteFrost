#include "bytefrost/semantic/symbol_table.h"

namespace bytefrost {

ScopeManager::ScopeManager() {
	// Start with a global scope.
	scopes_.emplace_back();
}

void ScopeManager::pushScope() {
	scopes_.emplace_back();
}

void ScopeManager::popScope() {
	if (scopes_.size() > 1) {
		scopes_.pop_back();
	}
}

void ScopeManager::declare(const std::string& name, SymbolInfo info) {
	scopes_.back()[name] = std::move(info);
}

const SymbolInfo* ScopeManager::lookup(const std::string& name) const {
	for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
		auto found = it->find(name);
		if (found != it->end()) {
			return &found->second;
		}
	}
	return nullptr;
}

SymbolInfo* ScopeManager::lookupMutable(const std::string& name) {
	for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
		auto found = it->find(name);
		if (found != it->end()) {
			return &found->second;
		}
	}
	return nullptr;
}

bool ScopeManager::isDeclaredInCurrentScope(const std::string& name) const {
	if (scopes_.empty())
		return false;
	return scopes_.back().count(name) > 0;
}

}  // namespace bytefrost
