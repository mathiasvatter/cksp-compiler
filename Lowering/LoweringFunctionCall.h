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
            auto node_body = inline_property_function(node.definition->header.get(), std::move(node.function->header));
            node_body->accept(*this);
            return node.replace_with(std::move(node_body));
        }
        if(node.kind == NodeFunctionCall::Kind::Builtin) {
            // get_ui_id lowering
            node.function->accept(*this);
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

    /// lowering of get control statements from property functions
	NodeAST * visit(NodeSingleAssignment &node) override {
		return node.lower(m_program);
	};



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
    static inline std::unique_ptr<NodeBlock> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
        auto node_body = std::make_unique<NodeBlock>(function_header->tok);
        for(int i = 1; i<function_header->args->params.size(); i++) {
            auto node_get_control = std::make_unique<NodeGetControl>(
                    function_header->args->params[0]->clone(),
                    property_function->args->params[i]->get_string(),
                    function_header->tok
            );
            auto node_assignment = std::make_unique<NodeSingleAssignment>(
                    std::move(node_get_control),
                    std::move(function_header->args->params[i]),
                    function_header->tok
            );
            node_body->add_stmt(
                    std::make_unique<NodeStatement>(std::move(node_assignment), node_body->tok)
            );
        }
        return std::move(node_body);
    }



};