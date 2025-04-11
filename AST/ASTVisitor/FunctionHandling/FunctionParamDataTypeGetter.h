//
// Created by Mathias Vatter on 01.04.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Tries to infer the data type of the function parameters of a given function call by
 * comparing the actual parameters and their types with the formal parameters and their types.
 *
 */
class FunctionParamDataTypeGetter final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit FunctionParamDataTypeGetter(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	/// tracks function parameter and their data types for later get_ui_id substitution in function body
	std::unordered_map<NodeFunctionParam*, std::unordered_set<DataType>> m_data_type_per_param;
	NodeFunctionParam* m_current_formal_param = nullptr;
	void insert_ui_control_type_of_arg(const NodeReference& actual_param, NodeFunctionParam* formal_param) {
		if (!actual_param.is_func_arg()) return;
		if (formal_param) {
			m_data_type_per_param[formal_param].insert(actual_param.data_type);
		}
	}

	void write_data_type_per_param() {
		for (const auto& [param, types] : m_data_type_per_param) {
			if (types.size() != 1) continue;
			auto data_type = *types.begin();
			if (data_type == DataType::UIControl)
				param->variable->data_type = data_type;
		}
	}


	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		write_data_type_per_param();
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		const auto definition = node.get_definition();
		if (definition) {
			for (int i = 0; i< node.function->get_num_args(); i++) {
				const auto& actual_param = node.function->get_arg(i);
				m_current_formal_param = definition->header->params[i].get();
				actual_param->accept(*this);
				m_current_formal_param = nullptr;
			}

			if (!definition->visited) definition->accept(*this);
			definition->visited = true;
		}
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}
	NodeAST * visit(NodeVariableRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}
	NodeAST * visit(NodeFunctionHeaderRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}
	NodeAST * visit(NodeNDArrayRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}
	NodeAST * visit(NodePointerRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}
	NodeAST * visit(NodeListRef& node) override {
		insert_ui_control_type_of_arg(node, m_current_formal_param);
		return &node;
	}


};
