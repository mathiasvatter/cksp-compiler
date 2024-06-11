//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "../../../misc/Gensym.h"


class ASTGlobalScope : public ASTVisitor {
protected:
	DefinitionProvider* m_def_provider;
	static inline std::unique_ptr<NodeAST> to_assign_statement(NodeSingleDeclaration& node) {
		auto node_assignment = node.to_assign_stmt();
		if (node_assignment->l_value->get_node_type() == NodeType::ArrayRef) {
			auto node_array_ref = static_cast<NodeArrayRef *>(node_assignment->l_value.get());
			if (!node_array_ref->index and node_assignment->r_value->get_node_type() == NodeType::ParamList) {
				return std::make_unique<NodeDeadCode>(node.tok);
			}
		}
		return std::move(node_assignment);
	};

public:
	explicit ASTGlobalScope(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	void visit(NodeProgram& node) override;


};
