#pragma once

#include <memory>
#include <string>

namespace bytefrost {

/// BFTypeKind enumerates every type kind in the ByteFrost type system.
/// This is intentionally independent from LLVM types so the semantic layer
/// has no LLVM dependency.
enum class BFTypeKind {
	// Primitives
	Void,
	Int,     // i64 in LLVM
	Float,   // double in LLVM
	Bool,    // i1 in LLVM
	Char,    // i8 in LLVM
	String,  // i8* in LLVM

	// Named types
	Enum,   // value = i32 in LLVM
	Struct, // opaque ptr in LLVM (heap-allocated, ref-counted)

	// Parameterised types
	Array,  // array<T>
	Map,    // map<K,V>

	// Nullability (Phase 3 — scaffolded here for forward compatibility)
	Nullable,  // T?

	// Unresolved — used as a sentinel before resolution completes
	Unknown,
};

/// BFType is the ByteFrost semantic type representation.
/// All instances should be treated as immutable once constructed.
/// Use the static factory methods to build types.
struct BFType {
	BFTypeKind kind = BFTypeKind::Unknown;

	/// Non-empty for Enum and Struct kinds (the declared name).
	std::string name;

	/// Element type: Array<elemType>, Nullable<innerType>.
	std::shared_ptr<BFType> elemType;

	/// Key type for Map<keyType, valueType>.
	std::shared_ptr<BFType> keyType;

	/// Value type for Map<keyType, valueType>.
	std::shared_ptr<BFType> valueType;

	// -----------------------------------------------------------------------
	// Factory methods
	// -----------------------------------------------------------------------

	static BFType makeVoid()    { return {BFTypeKind::Void};    }
	static BFType makeInt()     { return {BFTypeKind::Int};     }
	static BFType makeFloat()   { return {BFTypeKind::Float};   }
	static BFType makeBool()    { return {BFTypeKind::Bool};    }
	static BFType makeChar()    { return {BFTypeKind::Char};    }
	static BFType makeString()  { return {BFTypeKind::String};  }
	static BFType makeUnknown() { return {BFTypeKind::Unknown}; }

	static BFType makeEnum(const std::string& typeName) {
		BFType t;
		t.kind = BFTypeKind::Enum;
		t.name = typeName;
		return t;
	}

	static BFType makeStruct(const std::string& typeName) {
		BFType t;
		t.kind = BFTypeKind::Struct;
		t.name = typeName;
		return t;
	}

	static BFType makeArray(BFType elem) {
		BFType t;
		t.kind = BFTypeKind::Array;
		t.name = "array";
		t.elemType = std::make_shared<BFType>(std::move(elem));
		return t;
	}

	static BFType makeMap(BFType key, BFType val) {
		BFType t;
		t.kind = BFTypeKind::Map;
		t.name = "map";
		t.keyType   = std::make_shared<BFType>(std::move(key));
		t.valueType = std::make_shared<BFType>(std::move(val));
		return t;
	}

	static BFType makeNullable(BFType inner) {
		BFType t;
		t.kind = BFTypeKind::Nullable;
		t.elemType = std::make_shared<BFType>(std::move(inner));
		return t;
	}

	// -----------------------------------------------------------------------
	// Predicates
	// -----------------------------------------------------------------------

	bool isVoid()    const { return kind == BFTypeKind::Void; }
	bool isInt()     const { return kind == BFTypeKind::Int; }
	bool isFloat()   const { return kind == BFTypeKind::Float; }
	bool isBool()    const { return kind == BFTypeKind::Bool; }
	bool isChar()    const { return kind == BFTypeKind::Char; }
	bool isString()  const { return kind == BFTypeKind::String; }
	bool isEnum()    const { return kind == BFTypeKind::Enum; }
	bool isStruct()  const { return kind == BFTypeKind::Struct; }
	bool isArray()   const { return kind == BFTypeKind::Array; }
	bool isMap()     const { return kind == BFTypeKind::Map; }
	bool isNullable() const { return kind == BFTypeKind::Nullable; }
	bool isUnknown() const { return kind == BFTypeKind::Unknown; }

	/// True for int or float.
	bool isNumeric() const { return kind == BFTypeKind::Int || kind == BFTypeKind::Float; }

	/// True for any type that supports == and != comparison.
	bool isComparable() const {
		return isNumeric() || isBool() || isChar() || isString() || isEnum();
	}

	/// True for any type that supports <, >, <=, >=.
	bool isOrdered() const {
		return isNumeric() || isChar();
	}

	// -----------------------------------------------------------------------
	// Equality
	// -----------------------------------------------------------------------

	bool operator==(const BFType& other) const;
	bool operator!=(const BFType& other) const { return !(*this == other); }

	// -----------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------

	/// Return a human-readable name suitable for diagnostics.
	std::string toString() const;
};

}  // namespace bytefrost
