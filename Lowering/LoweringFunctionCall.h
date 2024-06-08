//
// Created by Mathias Vatter on 07.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringFunctionCall : public ASTLowering {
private:
    std::unique_ptr<NodeFunctionCall> get_ui_id = std::make_unique<NodeFunctionCall>(
            false,
            std::make_unique<NodeFunctionHeader>(
                    "get_ui_id",
                    std::make_unique<NodeParamList>(Token()),
                    Token()
            ),
            Token()
    );


public:
	explicit LoweringFunctionCall(DefinitionProvider *def_provider) : ASTLowering(def_provider) {}

    /// Determining if function is property function -> inline property function
	/// Determining if function parameter needs to be wrapped in get_ui_id because of ui control
	void visit(NodeFunctionCall &node) override {
        if(node.kind == NodeFunctionCall::Kind::Property) {
            auto node_body = inline_property_function(node.definition->header.get(), std::move(node.function));
            node_body->accept(*this);
            node.replace_with(std::move(node_body));
            return;
        }
        if(node.kind == NodeFunctionCall::Kind::Builtin) {
            node.function->accept(*this);
            return;
        }
	}

    /// lowering of get control statements from property functions
    void visit(NodeGetControlStatement &node) override {
        if(auto lowering = node.get_lowering(m_def_provider)) {
            lowering->visit(node);
        }
    };

    void visit(NodeVariableRef &node) override {
        if(node.data_type == DataType::UI_Control and node.is_func_arg()) {
            auto node_get_ui_id = clone_as<NodeFunctionCall>(get_ui_id.get());
            node_get_ui_id->function->ty = TypeRegistry::Integer;
            node_get_ui_id->kind = NodeFunctionCall::Kind::Builtin;
            node_get_ui_id->function->args->params.push_back(std::move(node.clone()));
            node_get_ui_id->function->args->set_child_parents();
            node.replace_with(std::move(node_get_ui_id));
        }
    }

    void visit(NodeArrayRef &node) override {
        if(node.data_type == DataType::UI_Control and node.is_func_arg()) {
            auto node_get_ui_id = clone_as<NodeFunctionCall>(get_ui_id.get());
            node_get_ui_id->function->ty = TypeRegistry::Integer;
            node_get_ui_id->kind = NodeFunctionCall::Kind::Builtin;
            node_get_ui_id->function->args->params.push_back(std::move(node.clone()));
            node_get_ui_id->function->args->set_child_parents();
            node.replace_with(std::move(node_get_ui_id));
        }
    }

private:
    inline std::unique_ptr<NodeBody> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
        auto node_body = std::make_unique<NodeBody>(function_header->tok);
        for(int i = 1; i<function_header->args->params.size(); i++) {
            auto node_get_control = std::make_unique<NodeGetControlStatement>(
                    function_header->args->params[0]->clone(),
                    property_function->args->params[i]->get_string(),
                    function_header->tok
            );
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(
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