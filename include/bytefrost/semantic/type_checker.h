#pragma once

#include <string>

#include "bytefrost/semantic/bf_type.h"
#include "bytefrost/semantic/type_resolver.h"

namespace bytefrost {

/// TypeChecker validates semantic type constraints.
///
/// All methods are pure — they inspect types and return results without
/// touching diagnostics directly.  The caller (SemanticContext) is responsible
/// for reporting errors so source locations can be attached.
class TypeChecker {
public:
	explicit TypeChecker(const TypeResolver& resolver);

	/// Returns true if a value of type `from` can be assigned to a variable of
	/// type `to`.  Handles numeric widening (int -> float) and same-type checks.
	bool isAssignable(const BFType& from, const BFType& to) const;

	/// Returns true if `op` is valid for the given operand types.
	/// Supported op values: "+", "-", "*", "/", "%", "&&", "||",
	///                      "==", "!=", "<", ">", "<=", ">=".
	bool isBinaryOpValid(const std::string& op, const BFType& lhs,
	                     const BFType& rhs) const;

	/// Infer the result type of a binary operation.
	/// Returns BFType::makeUnknown() when the operation is invalid.
	BFType inferBinaryResult(const std::string& op, const BFType& lhs,
	                         const BFType& rhs) const;

	/// Returns true if the two types can be meaningfully compared with == / !=.
	/// Enum cross-type comparison (different enum names) is rejected.
	bool isEquatable(const BFType& lhs, const BFType& rhs) const;

	/// Returns true if the two types support ordered comparison (<, >, <=, >=).
	bool isOrderable(const BFType& lhs, const BFType& rhs) const;

	/// Returns true if a unary operator is valid for the given operand type.
	/// Supported ops: "-" (negate numeric), "!" (logical not bool),
	///               "++" / "--" (increment/decrement int).
	bool isUnaryOpValid(const std::string& op, const BFType& operand) const;

	/// Return the result type of a unary op.  Unknown if invalid.
	BFType inferUnaryResult(const std::string& op, const BFType& operand) const;

private:
	const TypeResolver& resolver_;

	static bool isArithmeticOp(const std::string& op);
	static bool isComparisonOp(const std::string& op);
	static bool isLogicalOp(const std::string& op);
	static bool isEqualityOp(const std::string& op);
};

}  // namespace bytefrost
