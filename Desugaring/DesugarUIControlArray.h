//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTDesugaring.h"
#include "../Interpreter/SimpleExprInterpreter.h"

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
class DesugarUIControlArray final : public ASTDesugaring {
    // size of e.g. ui_table array
    std::unique_ptr<NodeParamList> m_ui_control_var_size = nullptr;
    // size of the ui array -> number of ui_controls that need to be declared
	std::unique_ptr<NodeAST> m_ui_array_size = nullptr;
	std::unique_ptr<NodeUIControl> m_ui_control_array = nullptr;
	std::optional<Token> m_persistence;
	std::string m_single_control_name;
	std::shared_ptr<NodeUIControl> m_engine_widget = nullptr;
public:
	explicit DesugarUIControlArray(NodeProgram* program) : ASTDesugaring(program) {}

	NodeAST * visit(NodeSingleDeclaration &node) override {
		const auto node_ui_control = node.variable->cast<NodeUIControl>();
		if(!node_ui_control) return &node;

		m_engine_widget = node_ui_control->get_builtin_widget(m_program);
		node_ui_control->declaration = m_engine_widget;
		if(!node_ui_control->is_ui_control_array()) return &node;

		// move persistence information to not get a persistent array
		m_persistence = node_ui_control->control_var->persistence;
		node_ui_control->control_var->persistence = std::nullopt;
		m_ui_control_array = clone_as<NodeUIControl>(node_ui_control);
		m_ui_control_array->control_var->clear_references();
		// visit array or ndarray -> and assign m_ui_control_size
		m_ui_control_array->control_var->accept(*this);

		auto node_block = std::make_unique<NodeBlock>(node.tok);
		// declare %_lbl_lbl[4*2]
		auto node_decl = std::make_unique<NodeSingleDeclaration>(
			node_ui_control->control_var,
			nullptr, node.tok
		);
		// not data typ ui_control anymore
		node_decl->variable->data_type = DataType::Mutable;
		// also not string/real type anymore (in case of ui_text_edit or ui_xy)
		node_decl->variable->ty = TypeRegistry::Unknown; //TypeRegistry::ArrayOfInt;
		node_block->add_as_stmt(std::move(node_decl));
		node_block->append_body(create_ui_controls(*m_ui_control_array, std::move(m_ui_array_size)));
		return node.replace_with(std::move(node_block));
	}

	NodeAST * visit(NodeArray &node) override {
		if(!node.size) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Unable to infer array size. Size of UI Control Array has to be determined at compile time.";
			error.m_expected = "<initializer list>";
			error.exit();
		}
		m_ui_array_size = node.size->clone();
		m_single_control_name = node.name;
        // real array of ui control is saved in m_ui_control_array->size
        if(!m_ui_control_array->sizes->params.empty()) {
            m_ui_control_var_size = clone_as<NodeParamList>(m_ui_control_array->sizes.get());
        }
		return &node;
	}

	NodeAST * visit(NodeNDArray &node) override {
		if(!node.sizes) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Unable to infer array size. Size of UI Control Array has to be determined at compile time.";
			error.m_expected = "<initializer list>";
			error.exit();
		}
		m_single_control_name = "_"+node.name;
		m_ui_array_size = NodeBinaryExpr::create_right_nested_binary_expr(node.sizes->params, 0, token::MULT);
		// declare ui_table array[3,4][128](0,127,1)
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
		auto ui_control_template = m_engine_widget;
        // if is ui_table array -> set size to ui_table array size
        if(auto node_array = ui_control_template->control_var->cast<NodeArray>()) {
            node_array->set_size(clone_as<NodeParamList>(m_ui_control_var_size.get()));
        }
		for (int i = 0; i < array_size; i++) {
			auto new_ui_control = std::make_unique<NodeUIControl>(
				ui_control_template->ui_control_type,
				clone_as<NodeDataStructure>(ui_control_template->control_var.get()),
				clone_as<NodeParamList>(ui_control.params.get()),
				ui_control.tok
			);
			new_ui_control->control_var->name = m_single_control_name + std::to_string(i);
			new_ui_control->control_var->data_type = DataType::UIControl;
			new_ui_control->control_var->persistence = m_persistence;

			node_body->add_as_stmt(std::make_unique<NodeSingleDeclaration>(
					std::move(new_ui_control),
					nullptr, ui_control.tok
					));
		}

		/*
		 * _iterator := 0
		 * while (_iterator<=7)
		 * 	%_lbl_lbl[_iterator] := get_ui_id($_lbl_lbl0)+_iterator
		 * 	inc(_iterator)
		 * end while
		 * -> or now with array assignments:
		 * %_lbl_lbl := (get_ui_id($_lbl_lbl0)+inc)
		 */

		auto node_iterator_var_ref = m_program->global_iterator->to_reference();
		auto node_while_body = std::make_unique<NodeBlock>(ui_control.tok);

		// this is the $_lbl_lbl0 from the above example
		auto node_ui_control_var_ref = ui_control_template->control_var->to_reference();
		node_ui_control_var_ref->name = m_single_control_name + std::to_string(0);

		// get_ui_id($_lbl_lbl0)+_iterator
		auto r_value = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			node_ui_control_var_ref->wrap_in_get_ui_id(),
			node_iterator_var_ref->clone(), ui_control.tok
		);
		r_value->ty = TypeRegistry::Integer;

		// %_lbl_lbl[_iterator] := get_ui_id($_lbl_lbl0)+_iterator in above example
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			std::make_unique<NodeArrayRef>(
				m_single_control_name,
				node_iterator_var_ref->clone(), ui_control.tok
			),
			std::move(r_value),
		   	ui_control.tok
		);
		node_assignment->l_value->ty = TypeRegistry::Integer;

		node_while_body->add_as_stmt(std::move(node_assignment));
		node_while_body->wrap_in_loop(m_program->global_iterator,
			std::make_unique<NodeInt>(0, ui_control.tok),
			std::make_unique<NodeInt>(array_size, ui_control.tok),
			false
		);
		node_body->add_as_stmt(std::move(node_while_body));
		return node_body;
	}
};

