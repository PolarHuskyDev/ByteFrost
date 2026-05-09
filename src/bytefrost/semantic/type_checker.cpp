#include "bytefrost/semantic/type_checker.h"

namespace bytefrost {

TypeChecker::TypeChecker(const TypeResolver& resolver)
	: resolver_(resolver) {}

// -----------------------------------------------------------------------
// Operator classification helpers
// -----------------------------------------------------------------------

bool TypeChecker::isArithmeticOp(const std::string& op) {
	return op == "+" || op == "-" || op == "*" || op == "/" || op == "%";
}

bool TypeChecker::isComparisonOp(const std::string& op) {
	return op == "<" || op == ">" || op == "<=" || op == ">=";
}

bool TypeChecker::isEqualityOp(const std::string& op) {
	return op == "==" || op == "!=";
}

bool TypeChecker::isLogicalOp(const std::string& op) {
	return op == "&&" || op == "||" || op == "\^\^";
}

// -----------------------------------------------------------------------
// Assignment compatibility
// -----------------------------------------------------------------------

bool TypeChecker::isAssignable(const BFType& from, const BFType& to) const {
	if (from.isUnknown() || to.isUnknown())
		return true;  // suppress cascading errors on unresolved types

	// Identical types are always assignable.
	if (from == to)
		return true;

	// int is assignable to float (widening).
	if (from.isInt() && to.isFloat())
		return true;

	// Null literal is assignable to any nullable type.
	// (NullLiteralExpr resolves to Unknown; handled by the caller.)

	return false;
}

// -----------------------------------------------------------------------
// Binary operation validity
// -----------------------------------------------------------------------

bool TypeChecker::isBinaryOpValid(const std::string& op, const BFType& lhs,
                                  const BFType& rhs) const {
	// Suppress errors when operand types are unresolved.
	if (lhs.isUnknown() || rhs.isUnknown())
		return true;

	if (isArithmeticOp(op)) {
		// String concatenation with +.
		if (op == "+") {
			if (lhs.isString() && rhs.isString())
				return true;
		}
		// Numeric arithmetic.
		return lhs.isNumeric() && rhs.isNumeric();
	}

	if (isComparisonOp(op))
		return isOrderable(lhs, rhs);

	if (isEqualityOp(op))
		return isEquatable(lhs, rhs);

	if (isLogicalOp(op))
		return lhs.isBool() && rhs.isBool();

	// Bitwise operators (future): &, |, ^, <<, >>
	if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>")
		return lhs.isInt() && rhs.isInt();

	return false;
}

BFType TypeChecker::inferBinaryResult(const std::string& op, const BFType& lhs,
                                      const BFType& rhs) const {
	if (lhs.isUnknown() || rhs.isUnknown())
		return BFType::makeUnknown();

	if (isArithmeticOp(op)) {
		if (op == "+" && lhs.isString() && rhs.isString())
			return BFType::makeString();
		// If either operand is float the result is float.
		if (lhs.isFloat() || rhs.isFloat())
			return BFType::makeFloat();
		if (lhs.isInt() && rhs.isInt())
			return BFType::makeInt();
		return BFType::makeUnknown();
	}

	if (isComparisonOp(op) || isEqualityOp(op) || isLogicalOp(op))
		return BFType::makeBool();

	if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>")
		return BFType::makeInt();

	return BFType::makeUnknown();
}

// -----------------------------------------------------------------------
// Equatability / orderability
// -----------------------------------------------------------------------

bool TypeChecker::isEquatable(const BFType& lhs, const BFType& rhs) const {
	if (lhs.isUnknown() || rhs.isUnknown())
		return true;
	// Same-type comparisons are always valid for comparable kinds.
	if (lhs == rhs)
		return lhs.isComparable();
	// int/float cross-comparison.
	if (lhs.isNumeric() && rhs.isNumeric())
		return true;
	// Enum: both sides must be the same enum type.
	if (lhs.isEnum() || rhs.isEnum())
		return lhs.isEnum() && rhs.isEnum() && lhs.name == rhs.name;
	return false;
}

bool TypeChecker::isOrderable(const BFType& lhs, const BFType& rhs) const {
	if (lhs.isUnknown() || rhs.isUnknown())
		return true;
	return lhs.isOrdered() && rhs.isOrdered();
}

// -----------------------------------------------------------------------
// Unary operations
// -----------------------------------------------------------------------

bool TypeChecker::isUnaryOpValid(const std::string& op,
                                 const BFType& operand) const {
	if (operand.isUnknown())
		return true;
	if (op == "-")
		return operand.isNumeric();
	if (op == "!")
		return operand.isBool();
	if (op == "++" || op == "--")
		return operand.isInt();
	if (op == "~")
		return operand.isInt();
	return false;
}

BFType TypeChecker::inferUnaryResult(const std::string& op,
                                     const BFType& operand) const {
	if (operand.isUnknown())
		return BFType::makeUnknown();
	if (op == "!")
		return BFType::makeBool();
	if (op == "-" || op == "++" || op == "--" || op == "~")
		return operand;
	return BFType::makeUnknown();
}

}  // namespace bytefrost
