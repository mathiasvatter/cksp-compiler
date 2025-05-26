//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"
#include "../Interpreter/SimpleExprInterpreter.h"
#include <vector>

/// called bei NodeUIControl and NodeSingleDeclaration

/**
 *	Lowers/Desugars ui control arrays into multiple ui controls and an ui array
 *	Pros of putting this in the lowering phase:
 *	 - size can have constant declarations
 *	Pros of putting this in the desugaring phase:
 *	 - better variable checking because the single ui controls can be linked via name
 *
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
class LoweringUIControlArray final : public ASTLowering {
    // size of e.g. ui_table array
    std::unique_ptr<NodeParamList> m_ui_control_var_size = nullptr;
    // size of the ui array -> number of ui_controls that need to be declared
	std::unique_ptr<NodeAST> m_ui_array_size = nullptr;
	std::unique_ptr<NodeUIControl> m_ui_control_array = nullptr;
	std::optional<Token> m_persistence;
public:
	explicit LoweringUIControlArray(NodeProgram* program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleDeclaration &node) override {
		const auto node_ui_control = node.variable->cast<NodeUIControl>();
		if(!node_ui_control)
			return &node;

		if(!node_ui_control->is_ui_control_array())
			return &node;

		// move persistence information to not get a persistent array
		m_persistence = node_ui_control->control_var->persistence;
		node_ui_control->control_var->persistence = std::nullopt;
		m_ui_control_array = clone_as<NodeUIControl>(node_ui_control);
		m_ui_control_array->control_var->clear_references();
		if(m_ui_control_array->control_var->cast<NodeNDArray>()) {
			// get correct size for ui control array by lowering ndarray
			// get correct single ui control names by lowering
			m_ui_control_array->control_var->data_lower(m_program);
		}
		m_ui_control_array->control_var->accept(*this);

		auto body_post_lowering = std::make_unique<NodeBlock>(node.tok);
		// declare %_lbl_lbl[4*2]
		auto node_array_declaration = std::make_unique<NodeSingleDeclaration>(
			node_ui_control->control_var,
			nullptr, node.tok
		);
		// not data typ ui_control anymore
		node_array_declaration->variable->data_type = DataType::Mutable;
		// also not string/real type anymore (in case of ui_text_edit or ui_xy)
		node_array_declaration->variable->ty = TypeRegistry::ArrayOfInt;
		body_post_lowering->add_as_stmt(std::move(node_array_declaration));
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

	std::unique_ptr<NodeBlock> create_ui_controls(NodeUIControl& ui_control, std::unique_ptr<NodeAST> size) {
		auto node_body = std::make_unique<NodeBlock>(ui_control.tok);
		// calculate array size
		static SimpleInterpreter eval;
		auto array_size_res = eval.evaluate_int_expression(size);
		if (array_size_res.is_error()) {
			array_size_res.get_error().exit();
		}
		int array_size = array_size_res.unwrap();
		std::string new_control_name = ui_control.control_var->name;
		auto new_ui_control_template = clone_as<NodeUIControl>(ui_control.get_declaration().get());
        // if is ui_table array -> set size to ui_table array size
        if(auto node_array = new_ui_control_template->control_var->cast<NodeArray>()) {
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

			m_def_provider->set_declaration(new_ui_control->control_var, true);
			auto new_node_declaration = std::make_unique<NodeSingleDeclaration>(
					std::move(new_ui_control),
					nullptr, ui_control.tok
					);
			node_body->add_as_stmt(std::move(new_node_declaration));
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

		auto node_iterator_var_ref = m_program->get_global_iterator()->to_reference();
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

		node_while_body->add_as_stmt(std::move(node_assignment));
		auto node_while_loop = make_while_loop(
			node_iterator_var_ref.get(),
			0,
			array_size,
			std::move(node_while_body), node_body.get());
		node_body->add_as_stmt(std::move(node_while_loop));
		return node_body;
	}
};

