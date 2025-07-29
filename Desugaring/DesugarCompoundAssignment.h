
//
// Created by Mathias Vatter on 09.07.25.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Transform var += 2 into SingleAssignments and BinaryExpressions
 * Transform var -=1 or var +=1 into Increment/Decrement nodes
 */
class DesugarCompoundAssignment final : public ASTDesugaring {
	std::unordered_map<token, std::function<NodeAST*(NodeCompoundAssignment&)>> m_inc_dec_transform = {
		{token::ADD, [](NodeCompoundAssignment& node) -> NodeAST* {
			auto int_lit = node.r_value->cast<NodeInt>();
			if (int_lit && int_lit->value == 1) {
				return node.replace_with(DefinitionProvider::inc(std::move(node.l_value)));
			} else if (int_lit && int_lit->value == -1) {
				return node.replace_with(DefinitionProvider::dec(std::move(node.l_value)));
			}
			return nullptr;
		}},
		{token::SUB, [](NodeCompoundAssignment& node) -> NodeAST* {
			auto int_lit = node.r_value->cast<NodeInt>();
			if (int_lit && int_lit->value == 1) {
				return node.replace_with(DefinitionProvider::dec(std::move(node.l_value)));
			} else if (int_lit && int_lit->value == -1) {
				return node.replace_with(DefinitionProvider::inc(std::move(node.l_value)));
			}
			return nullptr;
		}}
	};

public:
	explicit DesugarCompoundAssignment(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST* visit(NodeCompoundAssignment& node) override {
		// transform to dec or inc if operator is +/- and r_value is 1 or -1
		auto it = m_inc_dec_transform.find(node.op);
		if (it != m_inc_dec_transform.end()) {
			if (auto transformed = it->second(node)) {
				return transformed;
			}
		}


		// clone l_value beforehand to avoid dangling pointers because windows thx
		auto l_value = clone_as<NodeReference>(node.l_value.get());
		auto assignment = std::make_unique<NodeSingleAssignment>(
			std::move(l_value),
			std::make_unique<NodeBinaryExpr>(
				node.op,
				std::move(node.l_value),
				std::move(node.r_value),
				node.tok
			),
			node.tok
		);

		return node.replace_with(std::move(assignment));
	}

};

