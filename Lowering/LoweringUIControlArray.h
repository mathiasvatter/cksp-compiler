//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"
#include "../Interpreter/SimpleExprInterpreter.h"

/// called bei NodeUIControl and NodeSingleDeclareStatement
class LoweringUIControlArray : public ASTLowering {
private:
    // size of e.g. ui_table array
    std::unique_ptr<NodeParamList> m_ui_control_var_size = nullptr;
    // size of the ui array -> number of ui_controls that need to be declared
	std::unique_ptr<NodeAST> m_ui_array_size = nullptr;
	std::unique_ptr<NodeUIControl> m_ui_control_array = nullptr;
	std::optional<Token> m_persistence;
public:

	void visit(NodeSingleDeclareStatement &node) override {
		auto node_ui_control = cast_node<NodeUIControl>(node.to_be_declared.get());
		if(node_ui_control) {
			if(!is_ui_control_array(node_ui_control)) return;
			// move persistence information to not get a persistent array
			m_persistence = node_ui_control->control_var->persistence;
			node_ui_control->control_var->persistence = std::nullopt;
			m_ui_control_array = clone_as<NodeUIControl>(node_ui_control);
			if(m_ui_control_array->control_var->get_node_type() == NodeType::NDArray) {
				// get correct size for ui control array by lowering ndarray
				m_ui_control_array->control_var->accept(*this);
			}
			m_ui_control_array->control_var->accept(*this);

		} else return;

		std::unique_ptr<NodeBody> body_post_lowering = std::make_unique<NodeBody>(node.tok);

		auto node_array_declaration = statement_wrapper(std::make_unique<NodeSingleDeclareStatement>(std::move(node_ui_control->control_var), nullptr, node.tok),nullptr);
		node_array_declaration->update_parents(node.parent);
		// lowering of ndarray, turn DeclareStatement into NodeBody
		if(auto lowering = node_array_declaration->statement->get_lowering()) {
			node_array_declaration->statement->accept(*lowering);
		}

		body_post_lowering->statements.push_back(std::move(node_array_declaration));
		body_post_lowering->append_body(create_ui_controls(*m_ui_control_array, std::move(m_ui_array_size)));
		body_post_lowering->update_parents(node.parent);
		node.replace_with(std::move(body_post_lowering));
	}

	bool is_ui_control_array(NodeUIControl* node) {
		auto node_native_declaration = cast_node<NodeUIControl>(node->declaration);
		if(!node_native_declaration) return false;
		if(node->control_var->get_node_type() == NodeType::NDArray) return true;
		if(node->control_var->get_node_type() == NodeType::Array and node_native_declaration->control_var->get_node_type() != NodeType::Array) return true;
		return false;
	}

	void visit(NodeArray &node) override {
		auto error = CompileError(ErrorType::SyntaxError, "Unable to infer array size. Size of UI Control Array has to be determined at compile time.", node.tok.line, "initializer list", "", node.tok.file);
		if(node.sizes->params.empty()) error.exit();
		m_ui_array_size = node.sizes->params[0]->clone();
        // real array of ui control is saved in m_ui_control_array->sizes
        if(!m_ui_control_array->sizes->params.empty()) {
            m_ui_control_var_size = clone_as<NodeParamList>(m_ui_control_array->sizes.get());
        }
	}

	void visit(NodeNDArray &node) override {
		if(auto lowering = node.get_lowering()) {
			node.accept(*lowering);
		}
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
	std::unique_ptr<NodeBody> create_ui_controls(NodeUIControl& ui_control, std::unique_ptr<NodeAST> size) {
		auto node_body = std::make_unique<NodeBody>(ui_control.tok);
		// calculate array size
		SimpleInterpreter eval;
		auto array_size_res = eval.evaluate_int_expression(size);
		if (array_size_res.is_error()) {
			array_size_res.get_error().exit();
		}
		int array_size = array_size_res.unwrap();
		std::string new_control_name = ui_control.control_var->name;
		auto new_ui_control_template = clone_as<NodeUIControl>(ui_control.declaration);
        new_ui_control_template->is_reference = false;
        // if is ui_table array -> set sizes to ui_table array sizes
        if(auto node_array = cast_node<NodeArray>(new_ui_control_template->control_var.get())) {
            node_array->sizes = clone_as<NodeParamList>(m_ui_control_var_size.get());
            node_array->set_child_parents();
        }
		for (int i = 0; i < array_size; i++) {
			auto new_ui_control = clone_as<NodeUIControl>(new_ui_control_template.get());
			new_ui_control->control_var->name = new_control_name + std::to_string(i);
            new_ui_control->control_var->is_reference = false;
			new_ui_control->control_var->persistence = m_persistence;
			new_ui_control->params = clone_as<NodeParamList>(ui_control.params.get());
			auto new_node_declaration =
				std::make_unique<NodeSingleDeclareStatement>(std::move(new_ui_control), nullptr, ui_control.tok);
			node_body->statements.push_back(statement_wrapper(std::move(new_node_declaration), node_body.get()));
		}
		// reinstantiate control name for while loop after
		new_control_name = ui_control.control_var->name + std::to_string(0);

		auto node_iterator_var =
			std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, ui_control.tok);
        node_iterator_var->is_engine = true;
		auto node_while_body = std::make_unique<NodeBody>(ui_control.tok);

		auto node_ui_control_var = std::move(new_ui_control_template->control_var);
		if (auto node_ui_control_var_array = cast_node<NodeArray>(node_ui_control_var.get())) {
			node_ui_control_var_array->show_brackets = false;
		}
		node_ui_control_var->name = new_control_name;
		std::vector<std::unique_ptr<NodeAST>> func_args;
		func_args.push_back(std::move(node_ui_control_var));
		auto node_get_ui_id = std::move(make_function_call("get_ui_id", std::move(func_args), nullptr, ui_control.tok)->statement);

		auto node_while_body_expression = make_binary_expr(ASTType::Integer,"+",std::move(node_get_ui_id),node_iterator_var->clone(),nullptr,ui_control.tok);

		auto node_raw_array_copy = clone_as<NodeArray>(ui_control.control_var.get());
		node_raw_array_copy->is_reference = true;
		node_raw_array_copy->indexes->params.clear();
		node_raw_array_copy->indexes->params.push_back(node_iterator_var->clone());
		auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_raw_array_copy),
																		   std::move(node_while_body_expression),
																		   ui_control.tok);
		node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
		auto node_while_loop =
			make_while_loop(node_iterator_var.get(), 0, array_size, std::move(node_while_body), node_body.get());
		node_body->statements.push_back(statement_wrapper(std::move(node_while_loop), node_body.get()));
		node_body->update_parents(ui_control.parent);
		return node_body;
	}
};

