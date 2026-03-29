#pragma once

#include <iostream>
#include <string>

#include "ast.h"

class ASTPrinter {
   public:
	static void print(const Program& program) {
		std::cout << "Program" << std::endl;
		for (const auto& func : program.functions) {
			printFunctionDecl(func, 1);
		}
	}

   private:
	static std::string indent(int level) {
		return std::string(level * 2, ' ');
	}

	static void printFunctionDecl(const FunctionDecl& func, int level) {
		std::cout << indent(level) << "FunctionDecl: " << func.name << "(";
		for (size_t i = 0; i < func.parameters.size(); i++) {
			if (i > 0) std::cout << ", ";
			std::cout << func.parameters[i].name << ": " << func.parameters[i].typeName;
		}
		std::cout << "): " << func.returnType << std::endl;
		printBlock(*func.body, level + 1);
	}

	static void printBlock(const Block& block, int level) {
		std::cout << indent(level) << "Block" << std::endl;
		for (const auto& stmt : block.statements) {
			printStmt(*stmt, level + 1);
		}
	}

	static void printStmt(const Stmt& stmt, int level) {
		switch (stmt.kind) {
			case StmtKind::Block:
				printBlock(static_cast<const Block&>(stmt), level);
				break;
			case StmtKind::VarDecl: {
				const auto& vd = static_cast<const VarDecl&>(stmt);
				std::cout << indent(level) << "VarDecl: " << vd.name
						  << ": " << vd.typeName << std::endl;
				if (vd.initializer) printExpr(*vd.initializer, level + 1);
				break;
			}
			case StmtKind::IfStmt: {
				const auto& is = static_cast<const IfStmt&>(stmt);
				std::cout << indent(level) << "IfStmt" << std::endl;
				std::cout << indent(level + 1) << "Condition:" << std::endl;
				printExpr(*is.condition, level + 2);
				std::cout << indent(level + 1) << "Then:" << std::endl;
				printBlock(*is.thenBlock, level + 2);
				if (is.elseBlock) {
					std::cout << indent(level + 1) << "Else:" << std::endl;
					printBlock(*is.elseBlock, level + 2);
				}
				break;
			}
			case StmtKind::ReturnStmt: {
				const auto& rs = static_cast<const ReturnStmt&>(stmt);
				std::cout << indent(level) << "ReturnStmt" << std::endl;
				if (rs.value) printExpr(*rs.value, level + 1);
				break;
			}
			case StmtKind::ExprStmt: {
				const auto& es = static_cast<const ExprStmt&>(stmt);
				std::cout << indent(level) << "ExprStmt" << std::endl;
				printExpr(*es.expression, level + 1);
				break;
			}
		}
	}

	static void printExpr(const Expr& expr, int level) {
		switch (expr.kind) {
			case ExprKind::NumberLiteral: {
				const auto& nl = static_cast<const NumberLiteral&>(expr);
				std::cout << indent(level) << "NumberLiteral: " << nl.value << std::endl;
				break;
			}
			case ExprKind::StringLiteral: {
				const auto& sl = static_cast<const StringLiteral&>(expr);
				std::cout << indent(level) << "StringLiteral: \"" << sl.value << "\"" << std::endl;
				break;
			}
			case ExprKind::Identifier: {
				const auto& id = static_cast<const Identifier&>(expr);
				std::cout << indent(level) << "Identifier: " << id.name << std::endl;
				break;
			}
			case ExprKind::UnaryExpr: {
				const auto& ue = static_cast<const UnaryExpr&>(expr);
				std::cout << indent(level) << "UnaryExpr: " << ue.op << std::endl;
				printExpr(*ue.operand, level + 1);
				break;
			}
			case ExprKind::BinaryExpr: {
				const auto& be = static_cast<const BinaryExpr&>(expr);
				std::cout << indent(level) << "BinaryExpr: " << be.op << std::endl;
				printExpr(*be.left, level + 1);
				printExpr(*be.right, level + 1);
				break;
			}
			case ExprKind::CallExpr: {
				const auto& ce = static_cast<const CallExpr&>(expr);
				std::cout << indent(level) << "CallExpr: " << ce.callee << std::endl;
				for (const auto& arg : ce.arguments) {
					printExpr(*arg, level + 1);
				}
				break;
			}
		}
	}
};
