//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTHandler.h"
#include "../Interpreter/SimpleExprInterpreter.h"

/// called bei NodeUIControl and NodeSingleDeclareStatement
class UIControlArrayHandler : public ASTHandler {
private:
	std::unique_ptr<NodeUIControl> m_ui_control_array;
public:
	std::unique_ptr<NodeAST> perform_lowering(NodeUIControl& node) override {
		auto node_native_declaration = cast_node<NodeUIControl>(node.declaration);
		if(!node_native_declaration) return nullptr;

		// is UI Array -> lowering
		auto node_array = cast_node<NodeArray>(node.control_var.get());
		if(node_array and node_native_declaration->control_var->get_node_type() != NodeType::Array) {
			if (node_array->sizes->params.empty()) {
				CompileError(ErrorType::SyntaxError, "Unable to infer array size.", node.tok.line, "initializer list", node.ui_control_type, node.tok.file).exit();
			}
			node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
			node.sizes->parent = &node;
			m_ui_control_array = clone_as<NodeUIControl>(&node);
			return std::move(node.control_var);
		}
		return nullptr;
	}
	// pre lowering just forwards to handler of control_var in case of nd array
	std::unique_ptr<NodeBody> add_members_pre_lowering(DataStructure& node) override {
		if(auto node_ui_control = cast_node<NodeUIControl>(&node)) {
			if(auto handler = node_ui_control->control_var->get_handler()) {
				return handler->add_members_pre_lowering(*node_ui_control);
			}
		}
		return nullptr;
	}
/**
 * Example:
 * declare ui_label lbl_lbl[4,2](1,1)
 * gets lowered to:
 * declare %_lbl_lbl[4*2]
 * declare const $lbl_lbl__SIZE_D1 := 4
 * declare const $lbl_lbl__SIZE_D2 := 2
 * declare ui_label $_lbl_lbl0(1,1)
 * declare ui_label $_lbl_lbl1(1,1)
 * declare ui_label $_lbl_lbl2(1,1)
 * declare ui_label $_lbl_lbl3(1,1)
 * declare ui_label $_lbl_lbl4(1,1)
 * declare ui_label $_lbl_lbl5(1,1)
 * declare ui_label $_lbl_lbl6(1,1)
 * declare ui_label $_lbl_lbl7(1,1)
 * $preproc_i := 0
 * while ($preproc_i<=7)
 * %_lbl_lbl[$preproc_i] := get_ui_id($_lbl_lbl0)+$preproc_i
 * inc($preproc_i)
 * end while
 */
	std::unique_ptr<NodeBody> add_members_post_lowering(DataStructure& node) override {
		auto node_body = std::make_unique<NodeBody>(node.tok);
		// calculate array size
		SimpleInterpreter eval;
		auto array_size_res = eval.evaluate_int_expression(m_ui_control_array->sizes->params[0]);
		if (array_size_res.is_error()) {
			array_size_res.get_error().exit();
		}
		int array_size = array_size_res.unwrap();
		std::string new_control_name = m_ui_control_array->control_var->name;
		auto new_ui_control_template = clone_as<NodeUIControl>(m_ui_control_array->declaration->);
		for (int i = 0; i < array_size; i++) {
			new_control_name += std::to_string(i);
			auto new_ui_control = clone_as<NodeUIControl>(m_ui_control_array->declaration);
			new_ui_control->control_var->name = new_control_name;
			new_ui_control->params = clone_as<NodeParamList>(m_ui_control_array->params.get());
			auto new_node_declaration =
				std::make_unique<NodeSingleDeclareStatement>(std::move(new_ui_control), nullptr, node.tok);
			node_body->statements.push_back(statement_wrapper(std::move(new_node_declaration), node_body.get()));
		}
		// reinstantiate control name for while loop after
		new_control_name = m_ui_control_array->control_var->name + std::to_string(0);

		auto node_iterator_var =
			std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, node.tok);
		auto node_while_body = std::make_unique<NodeBody>(node.tok);

		auto node_ui_control_var = std::move(new_ui_control_template.control_var);
		if (auto node_ui_control_var_array = cast_node<NodeArray>(node_ui_control_var.get())) {
			node_ui_control_var_array->show_brackets = false;
		}
		node_ui_control_var->name = new_control_name;

		auto node_get_ui_id = make_function_call("get_ui_id", {node_ui_control_var}, nullptr, node.tok);

		auto node_while_body_expression = make_binary_expr(ASTType::Integer,"+",std::move(node_get_ui_id),node_iterator_var->clone(),nullptr,node.tok);

		auto node_raw_array_copy = clone_as<NodeArray>(m_ui_control_array->control_var.get());
		node_raw_array_copy->indexes->params.clear();
		node_raw_array_copy->indexes->params.push_back(node_iterator_var->clone());
		auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_raw_array_copy),
																		   std::move(node_while_body_expression),
																		   node.tok);
		node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
		auto node_while_loop =
			make_while_loop(node_iterator_var.get(), 0, array_size, std::move(node_while_body), node_body.get());
		node_body->statements.push_back(statement_wrapper(std::move(node_while_loop), node_body.get()));
		node_body->update_parents(node.parent);
		return node_body;
	}
};

