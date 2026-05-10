#include "bytefrost/semantic/bf_type.h"

namespace bytefrost {

bool BFType::operator==(const BFType& other) const {
	if (kind != other.kind)
		return false;
	if (kind == BFTypeKind::Enum || kind == BFTypeKind::Struct)
		return name == other.name;
	if (kind == BFTypeKind::Array) {
		if (!elemType || !other.elemType)
			return elemType == other.elemType;
		return *elemType == *other.elemType;
	}
	if (kind == BFTypeKind::Map) {
		if (!keyType || !other.keyType || !valueType || !other.valueType)
			return false;
		return *keyType == *other.keyType && *valueType == *other.valueType;
	}
	if (kind == BFTypeKind::Nullable) {
		if (!elemType || !other.elemType)
			return elemType == other.elemType;
		return *elemType == *other.elemType;
	}
	return true;  // primitives match on kind alone
}

std::string BFType::toString() const {
	switch (kind) {
		case BFTypeKind::Void:    return "void";
		case BFTypeKind::Int:     return "int";
		case BFTypeKind::Float:   return "float";
		case BFTypeKind::Bool:    return "bool";
		case BFTypeKind::Char:    return "char";
		case BFTypeKind::String:  return "string";
		case BFTypeKind::Unknown: return "<unknown>";
		case BFTypeKind::Enum:
		case BFTypeKind::Struct:
			return name;
		case BFTypeKind::Array:
			return "array<" + (elemType ? elemType->toString() : "?") + ">";
		case BFTypeKind::Map:
			return "map<" + (keyType ? keyType->toString() : "?")
			       + ", " + (valueType ? valueType->toString() : "?") + ">";
		case BFTypeKind::Nullable:
			return (elemType ? elemType->toString() : "?") + "?";
	}
	return "<unknown>";
}

}  // namespace bytefrost
