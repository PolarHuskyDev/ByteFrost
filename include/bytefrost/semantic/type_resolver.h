#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "bytefrost/diagnostics/diagnostic.h"
#include "bytefrost/semantic/bf_type.h"
#include "parser/ast.h"

namespace bytefrost {

/// TypeResolver converts AST TypeNodes into semantic BFTypes.
///
/// It must be populated with known enum and struct names (via registerEnum /
/// registerStruct) before any TypeNode is resolved.  These names come from the
/// SemanticContext after the first pass over all top-level declarations.
class TypeResolver {
public:
	TypeResolver() = default;

	/// Register a user-defined enum type so it can be resolved.
	void registerEnum(const std::string& name);

	/// Register a user-defined struct type so it can be resolved.
	void registerStruct(const std::string& name);

	/// Register an unqualified enum variant name so identifiers like SPADES
	/// are recognised without a qualifier.  Returns the owning enum name.
	void registerEnumVariant(const std::string& variantName,
	                         const std::string& enumName);

	/// If variantName is a known unqualified variant, returns a pointer to the
	/// owning enum name; otherwise returns nullptr.
	const std::string* enumForVariant(const std::string& variantName) const;

	/// Resolve a TypeNode to a BFType.
	/// Returns BFType::makeUnknown() for types that cannot be resolved,
	/// optionally writing a diagnostic to the provided location.
	BFType resolve(const TypeNode& node) const;

	bool isKnownEnum(const std::string& name)   const { return enumNames_.count(name) > 0; }
	bool isKnownStruct(const std::string& name) const { return structNames_.count(name) > 0; }
	bool isKnownType(const std::string& name)   const {
		return isKnownEnum(name) || isKnownStruct(name) || isPrimitive(name);
	}

	const std::unordered_set<std::string>& enumNames()   const { return enumNames_; }
	const std::unordered_set<std::string>& structNames() const { return structNames_; }

private:
	std::unordered_set<std::string> enumNames_;
	std::unordered_set<std::string> structNames_;
	/// Maps unqualified variant name → owning enum name.
	std::unordered_map<std::string, std::string> variantToEnum_;

	static bool isPrimitive(const std::string& name);
	static BFType primitiveFor(const std::string& name);
};

}  // namespace bytefrost
