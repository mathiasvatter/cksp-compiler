//
// Created by Mathias Vatter on 07.06.24.
//

#pragma once

#include "ASTLowering.h"

///
class LoweringFunctionCall : public ASTLowering {
private:
public:
	explicit LoweringFunctionCall(NodeProgram *program) : ASTLowering(program) {}

    /// Determining if function is property function -> inline property function
	/// Determining if function parameter needs to be wrapped in get_ui_id because of ui control
	/// Determining if function call is method constructor -> rename
	NodeAST * visit(NodeFunctionCall &node) override {
		node.get_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            auto node_body = inline_property_function(node.definition->header.get(), std::move(node.function));
            node_body->accept(*this);
            return node.replace_with(std::move(node_body));
        }
        if(node.kind == NodeFunctionCall::Kind::Builtin) {
            // get_ui_id lowering
            node.function->accept(*this);

//			if(node.function->name == "num_elements") {
//				auto &arr_ref = node.function->header->params->params[0];
//				if(arr_ref->get_node_type() == NodeType::ArrayRef) {
//					auto node_arr = static_cast<NodeArrayRef*>(arr_ref.get());
//					return node.replace_with(static_cast<NodeArray*>(node_arr->declaration)->size->clone());
//				}
//			}

			return &node;
        }

//		if(node.kind == NodeFunctionCall::Kind::UserDefined) {
//			if(node.get_method_name() == "__init__") {
//				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
//				error.m_message = "Do not call constructor method directly.";
//				error.exit();
//			}
//		}


		return &node;
	}

    /// lowering of set control statements from property functions
	NodeAST * visit(NodeSetControl &node) override {
		node.ui_id->accept(*this);
		node.value->accept(*this);
		return node.lower(m_program);
	};

	NodeAST * visit(NodeGetControl &node) override {
		node.ui_id->accept(*this);
		return node.lower(m_program);
	}


    NodeAST * visit(NodeVariableRef &node) override {
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
    }

    NodeAST * visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
    }

	NodeAST * visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
	}

private:
    inline std::unique_ptr<NodeBlock> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeaderRef> header_ref) {
        auto node_body = std::make_unique<NodeBlock>(header_ref->tok);
        for(int i = 1; i<header_ref->args->size(); i++) {
            auto node_set_control = std::make_unique<NodeSetControl>(
				header_ref->get_arg(0)->clone(),
				property_function->get_param(i)->name,
				std::move(header_ref->get_arg(i)),
				header_ref->tok
            );
            node_body->add_as_stmt(std::move(node_set_control));
        }
        return std::move(node_body);
    }



};