//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"
#include "../Interpreter/SimpleExprInterpreter.h"
#include <vector>

/// called bei NodeUIControl and NodeSingleDeclaration
class LoweringUIControlArray : public ASTLowering {
private:
    // size of e.g. ui_table array
    std::unique_ptr<NodeParamList> m_ui_control_var_size = nullptr;
    // size of the ui array -> number of ui_controls that need to be declared
	std::unique_ptr<NodeAST> m_ui_array_size = nullptr;
	std::unique_ptr<NodeUIControl> m_ui_control_array = nullptr;
	std::optional<Token> m_persistence;
public:
	explicit LoweringUIControlArray(NodeProgram* program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleDeclaration &node) override {
		if(node.variable->get_node_type() != NodeType::UIControl) return &node;

		auto node_ui_control = static_cast<NodeUIControl*>(node.variable.get());
		if(!node_ui_control->is_ui_control_array()) return &node;
		// move persistence information to not get a persistent array
		m_persistence = node_ui_control->control_var->persistence;
		node_ui_control->control_var->persistence = std::nullopt;
		m_ui_control_array = clone_as<NodeUIControl>(node_ui_control);
		if(m_ui_control_array->control_var->get_node_type() == NodeType::NDArray) {
			// get correct size for ui control array by lowering ndarray
			m_ui_control_array->control_var->accept(*this);
		}
		m_ui_control_array->control_var->accept(*this);


		std::unique_ptr<NodeBlock> body_post_lowering = std::make_unique<NodeBlock>(node.tok);
		auto node_array_declaration = std::make_unique<NodeSingleDeclaration>(
			std::move(node_ui_control->control_var),
			nullptr, node.tok
		);
		// not data typ ui_control anymore
		node_array_declaration->variable->data_type = DataType::Mutable;
		// wrap in statement to make use of replace_child
		auto node_statement = std::make_unique<NodeStatement>(std::move(node_array_declaration), node.tok);
		// lowering of ndarray, turn Declaration into NodeBlock
		// only lower if ndarray otherwise array size constant gets declared twice
		if(m_ui_control_array->control_var->get_node_type() == NodeType::NDArray) {
			node_statement->statement->lower(m_program);
		}
		body_post_lowering->statements.push_back(std::move(node_statement));
		body_post_lowering->append_body(create_ui_controls(*m_ui_control_array, std::move(m_ui_array_size)));
		return node.replace_with(std::move(body_post_lowering));
	}

	NodeAST * visit(NodeArray &node) override {
		auto error = CompileError(ErrorType::SyntaxError, "Unable to infer array size. Size of UI Control Array has to be determined at compile time.", node.tok.line, "initializer list", "", node.tok.file);
		if(!node.size) error.exit();
		m_ui_array_size = node.size->clone();
        // real array of ui control is saved in m_ui_control_array->size
        if(!m_ui_control_array->sizes->params.empty()) {
            m_ui_control_var_size = clone_as<NodeParamList>(m_ui_control_array->sizes.get());
        }
		return &node;
	}

	NodeAST * visit(NodeNDArray &node) override {
		return node.lower(m_program);
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
 * 	%_lbl_lbl[$preproc_i] := get_ui_id($_lbl_lbl0)+$preproc_i
 * 	inc($preproc_i)
 * end while
 */
	std::unique_ptr<NodeBlock> create_ui_controls(NodeUIControl& ui_control, std::unique_ptr<NodeAST> size) {
		auto node_body = std::make_unique<NodeBlock>(ui_control.tok);
		// calculate array size
		SimpleInterpreter eval;
		auto array_size_res = eval.evaluate_int_expression(size);
		if (array_size_res.is_error()) {
			array_size_res.get_error().exit();
		}
		int array_size = array_size_res.unwrap();
		std::string new_control_name = ui_control.control_var->name;
		auto new_ui_control_template = clone_as<NodeUIControl>(ui_control.declaration);
        // if is ui_table array -> set size to ui_table array size
        if(auto node_array = cast_node<NodeArray>(new_ui_control_template->control_var.get())) {
            node_array->size = clone_as<NodeParamList>(m_ui_control_var_size.get());
            node_array->set_child_parents();
        }
		for (int i = 0; i < array_size; i++) {
			auto new_ui_control = std::make_unique<NodeUIControl>(
				new_ui_control_template->ui_control_type,
				clone_as<NodeDataStructure>(new_ui_control_template->control_var.get()),
				clone_as<NodeParamList>(ui_control.params.get()),
				ui_control.tok
				);
			new_ui_control->control_var->name = new_control_name + std::to_string(i);
			new_ui_control->control_var->data_type = DataType::UIControl;
			new_ui_control->control_var->persistence = m_persistence;

			m_def_provider->set_declaration(new_ui_control->control_var.get(), true);
			auto new_node_declaration = std::make_unique<NodeSingleDeclaration>(
					std::move(new_ui_control),
					nullptr, ui_control.tok
					);
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(new_node_declaration), ui_control.tok));
		}
		// reinstantiate control name for while loop after
		new_control_name = ui_control.control_var->name + std::to_string(0);

		/*
		 * _iterator := 0
		 * while (_iterator<=7)
		 * 	%_lbl_lbl[_iterator] := get_ui_id($_lbl_lbl0)+_iterator
		 * 	inc(_iterator)
		 * end while
		 * -> or now with array assignments:
		 * %_lbl_lbl := (get_ui_id($_lbl_lbl0)+inc)
		 */
		auto node_iterator_var_ref = std::make_unique<NodeVariableRef>("_iter", ui_control.tok);
        node_iterator_var_ref->is_engine = true;
		auto node_while_body = std::make_unique<NodeBlock>(ui_control.tok);

		// this is the $_lbl_lbl0 from the above example
		auto node_ui_control_var_ref = new_ui_control_template->control_var->to_reference();
		node_ui_control_var_ref->name = new_control_name;

		auto node_while_body_expression = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			node_ui_control_var_ref->wrap_in_get_ui_id(),
			node_iterator_var_ref->clone(), ui_control.tok
		);
		node_while_body_expression->ty = TypeRegistry::Integer;

		// %_lbl_lbl[_iterator] := get_ui_id($_lbl_lbl0)+_iterator in above example
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			std::make_unique<NodeArrayRef>(
				ui_control.control_var->name,
				node_iterator_var_ref->clone(), ui_control.tok
			),
			std::move(node_while_body_expression),
		   	ui_control.tok
		);
		node_assignment->l_value->ty = TypeRegistry::Integer;

		node_while_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assignment), ui_control.tok));
		auto node_while_loop = make_while_loop(
			node_iterator_var_ref.get(),
			0,
			array_size,
			std::move(node_while_body), node_body.get());
		node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_while_loop), ui_control.tok));
		return node_body;
	}
};

