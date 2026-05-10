#include "bytefrost/semantic/type_resolver.h"

namespace bytefrost {

// -----------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------

void TypeResolver::registerEnum(const std::string& name) {
	enumNames_.insert(name);
}

void TypeResolver::registerStruct(const std::string& name) {
	structNames_.insert(name);
}

void TypeResolver::registerEnumVariant(const std::string& variantName,
                                       const std::string& enumName) {
	variantToEnum_.emplace(variantName, enumName);
}

const std::string* TypeResolver::enumForVariant(const std::string& variantName) const {
	auto it = variantToEnum_.find(variantName);
	return it != variantToEnum_.end() ? &it->second : nullptr;
}

// -----------------------------------------------------------------------
// Primitive lookup
// -----------------------------------------------------------------------

bool TypeResolver::isPrimitive(const std::string& name) {
	return name == "int"    || name == "float"  || name == "bool"
	    || name == "char"   || name == "string" || name == "void";
}

BFType TypeResolver::primitiveFor(const std::string& name) {
	if (name == "int")    return BFType::makeInt();
	if (name == "float")  return BFType::makeFloat();
	if (name == "bool")   return BFType::makeBool();
	if (name == "char")   return BFType::makeChar();
	if (name == "string") return BFType::makeString();
	if (name == "void")   return BFType::makeVoid();
	return BFType::makeUnknown();
}

// -----------------------------------------------------------------------
// Resolution
// -----------------------------------------------------------------------

BFType TypeResolver::resolve(const TypeNode& node) const {
	const std::string& name = node.name;
	BFType resolved = BFType::makeUnknown();

	// Primitives
	if (isPrimitive(name))
		resolved = primitiveFor(name);

	// Nullable: T?  (represented via TypeNode name ending in '?' — future sugar)
	// Kept for forward compatibility; parser does not emit this yet.
	else if (!name.empty() && name.back() == '?') {
		std::string inner = name.substr(0, name.size() - 1);
		TypeNode innerNode(inner);
		resolved = BFType::makeNullable(resolve(innerNode));
	}

	// array<T>
	else if (name == "array" && node.typeParams.size() == 1) {
		BFType elem = resolve(*node.typeParams[0]);
		resolved = BFType::makeArray(std::move(elem));
	}

	// map<K, V>
	else if (name == "map" && node.typeParams.size() == 2) {
		BFType key = resolve(*node.typeParams[0]);
		BFType val = resolve(*node.typeParams[1]);
		resolved = BFType::makeMap(std::move(key), std::move(val));
	}

	// Named user types
	else if (enumNames_.count(name))
		resolved = BFType::makeEnum(name);
	else if (structNames_.count(name))
		resolved = BFType::makeStruct(name);

	if (node.isNullable && !resolved.isUnknown()) {
		resolved = BFType::makeNullable(std::move(resolved));
	}

	return resolved;
}

}  // namespace bytefrost
