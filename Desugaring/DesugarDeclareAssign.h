//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring for declare assign statement
 */
class DesugarDeclareAssign : public ASTDesugaring {
public:
	explicit DesugarDeclareAssign(NodeProgram* program) : ASTDesugaring(program) {};

    void inline visit(NodeDeclaration& node) override {
        // error handling
        if(node.variable.size() < node.value->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect declare statement syntax. There are more values to assign than to be declared.";
            error.m_expected = node.variable.size();
            error.m_got = node.value->params.size();
            error.exit();
        }

        std::vector<std::unique_ptr<NodeSingleDeclaration>> declare_statements;
        for(auto &declaration : node.variable) {
            auto node_single_declare_stmt = std::make_unique<NodeSingleDeclaration>(std::move(declaration), nullptr, node.tok);
            declare_statements.push_back(std::move(node_single_declare_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.value->params) {
            values.push_back(std::move(assignee));
        }
        // in case there is something assigned
        if(!node.value->params.empty()) {
            // there were more variables given than values -> repeat the last value
            while (values.size() < declare_statements.size()) {
                values.push_back(values.back()->clone());
            }
        }
        auto node_body = std::make_unique<NodeBlock>(node.tok);
        // get variable and their values together and put them in to NodeStatement
        for(int i = 0; i<declare_statements.size(); i++) {
            auto &stmt = declare_statements[i];
            // in case there is nothing assigned -> nullptr
            if (!node.value->params.empty()) {
                auto &val = values[i];
                stmt->value = std::move(val);
            }
            stmt->set_child_parents();
            node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
        replacement_node = std::move(node_body);
    }

    void inline visit(NodeAssignment &node) override {
        // error handling
        if(node.l_value->params.size() < node.r_value->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect assign statement syntax. There are more values to assign than array variables.";
            error.m_expected = node.l_value->params.size();
            error.m_got = node.r_value->params.size();
            error.exit();
        }

        std::vector<std::unique_ptr<NodeSingleAssignment>> assign_statements;
        for(auto &arr_var : node.l_value->params) {
            auto node_single_assign_stmt = std::make_unique<NodeSingleAssignment>(std::move(arr_var), nullptr, node.tok);
            assign_statements.push_back(std::move(node_single_assign_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.r_value->params) {
            values.push_back(std::move(assignee));
        }
        // there were more variables given than values -> repeat the last value
        while(values.size() < assign_statements.size()) {
            values.push_back(values.back()->clone());
        }

        auto node_body = std::make_unique<NodeBlock>(node.tok);
        for(int i = 0; i<assign_statements.size(); i++) {
            auto &stmt = assign_statements[i];
            auto &val = values[i];
            stmt->r_value = std::move(val);
            stmt->set_child_parents();
            node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
        replacement_node = std::move(node_body);
    }


};