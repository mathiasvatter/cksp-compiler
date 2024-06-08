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
            // get_ui_id lowering
            node.function->accept(*this);
            return;
        }
        // message overloaded is not recognized as builtin
        if(node.kind == NodeFunctionCall::Kind::Undefined) {
            if(node.function->args->params.size() == 1) return;
            if(node.function->name != "message") return;
            // lowering of message parameters when separated by comma
            node.function->args = inline_message_parameters(node.function->args);
        }
	}

    /// lowering of get control statements from property functions
    void visit(NodeGetControl &node) override {
        if(auto lowering = node.get_lowering(m_def_provider)) {
            lowering->visit(node);
        }
    };

    void visit(NodeVariableRef &node) override {
        if(node.data_type == DataType::UI_Control and node.is_func_arg()) {
            auto node_get_ui_id = clone_as<NodeFunctionCall>(get_ui_id.get());
            node_get_ui_id->function->ty = TypeRegistry::Integer;
            node_get_ui_id->kind = NodeFunctionCall::Kind::Builtin;
            node_get_ui_id->update_token_data(node.tok);
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
            node_get_ui_id->update_token_data(node.tok);
            node_get_ui_id->function->args->params.push_back(std::move(node.clone()));
            node_get_ui_id->function->args->set_child_parents();
            node.replace_with(std::move(node_get_ui_id));
        }
    }

private:
    inline std::unique_ptr<NodeBody> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
        auto node_body = std::make_unique<NodeBody>(function_header->tok);
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

    inline std::unique_ptr<NodeParamList> inline_message_parameters(std::unique_ptr<NodeParamList>& params) {
        // it is already only one parameter
        if (params->params.size() == 1) return std::move(params);

        auto new_param = std::make_unique<NodeParamList>(params->tok);

        // initialize node_expr with last param
        auto node_expr = std::move(params->params.back());
        params->params.pop_back();

        // Durchlaufe die restlichen Parameter in umgekehrter Reihenfolge
        for (int i = params->params.size() - 1; i >= 0; --i) {
            node_expr = std::make_unique<NodeBinaryExpr>(
                    token::STRING_OP,
                    std::move(params->params[i]),
                    std::move(node_expr),
                    params->tok
            );
            node_expr->ty = TypeRegistry::String;
        }
        node_expr->parent = params->parent;
        // Füge das endgültige node_expr der neuen Parameterliste hinzu
        new_param->params.push_back(std::move(node_expr));

        return new_param;
    }
};