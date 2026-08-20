//
// Created by Mathias Vatter on 20.08.26.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugars structs of the form:
 * found non parameterized object type: List<int>
 * struct List<T>
 *	value: T
 *	next: List<T>
 * end struct
 * ----> to:
 * struct List::int
 *  value: int
 *  next: List::int
 * end struct
 *
 */
class DesugarGenericStruct final : public ASTDesugaring {
	ObjectType* object_type{};
	TypeSubstitutions substitutions{};
public:
	explicit DesugarGenericStruct(NodeProgram *program) : ASTDesugaring(program) {}

	NodeAST* run(NodeStruct& node, ObjectType* type) {
		object_type = type;
		substitutions.clear();

		const auto& arguments = type->get_type_arguments();
		if (node.type_parameters.size() != arguments.size()) {
			auto error = Diagnostic(
				ErrorType::TypeError,
				"Generic struct <" + node.name + "> expects "
					+ std::to_string(node.type_parameters.size()) + " type argument(s), but <"
					+ type->to_string() + "> provides " + std::to_string(arguments.size()) + ".",
				std::to_string(node.type_parameters.size()) + " type argument(s)",
				node.tok
			);
			error.exit();
		}

		for (const auto& [_, parameter] : node.type_parameter_table) {
			const auto index = static_cast<size_t>(parameter->index());
			if (index >= arguments.size()) {
				auto error = Diagnostic(
					ErrorType::InternalError,
					"Type parameter <" + parameter->to_string() + "> of struct <" + node.name
						+ "> has the invalid index " + std::to_string(index) + ".",
					"type parameter index below " + std::to_string(arguments.size()),
					node.tok
				);
				error.exit();
			}
			substitutions.emplace(
				parameter.get(),
				arguments[index]
			);
		}
		return node.accept(*this);
	}

private:

	void substitute(NodeAST& node) const {
		if (node.ty) {
			node.ty = node.ty->substitute_type_parameters(substitutions);
		}
		for (auto& reference : node.type_references) {
			if (reference.type) {
				reference.type = reference.type->substitute_type_parameters(substitutions);
			}
		}
	}

	NodeAST* visit(NodeStruct& node) override {
		ASTVisitor::visit(node);
		node.name = object_type->ksp_encoded_string();
		node.ty = object_type;
		node.type_parameters.clear();
		node.type_parameter_table.clear();
		return &node;
	}

	NodeAST* visit(NodeVariable& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArray& node)	override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArray& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointer& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeFunctionHeader& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeList& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariableRef& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST * visit(NodePointerRef& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST * visit(NodeArrayRef& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST * visit(NodeNDArrayRef& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST * visit(NodeListRef& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST * visit(NodeFunctionCall& node) override {
		substitute(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		substitute(node);
		if (node.parameterized_type) {
			node.parameterized_type = static_cast<ObjectType*>(
				node.parameterized_type->substitute_type_parameters(substitutions)
			);
		}
		return ASTVisitor::visit(node);
	}

};
