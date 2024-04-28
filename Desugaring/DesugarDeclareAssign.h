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
    void inline visit(NodeDeclareStatement& node) override {
        // error handling
        if(node.to_be_declared.size() < node.assignee->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect declare statement syntax. There are more values to assign than to be declared.";
            error.m_expected = node.to_be_declared.size();
            error.m_got = node.assignee->params.size();
            error.exit();
        }

        std::vector<std::unique_ptr<NodeSingleDeclareStatement>> declare_statements;
        for(auto &declaration : node.to_be_declared) {
            auto node_single_declare_stmt = std::make_unique<NodeSingleDeclareStatement>(std::move(declaration), nullptr, node.tok);
            declare_statements.push_back(std::move(node_single_declare_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.assignee->params) {
            values.push_back(std::move(assignee));
        }
        // in case there is someting assigned
        if(!node.assignee->params.empty()) {
            // there were more variables given than values -> repeat the last value
            while (values.size() < declare_statements.size()) {
                values.push_back(values.back()->clone());
            }
        }
        auto node_body = std::make_unique<NodeBody>(node.tok);
        // get to_be_declared and their values together and put them in to NodeStatement
        for(int i = 0; i<declare_statements.size(); i++) {
            auto &stmt = declare_statements[i];
            // in case there is nothing assigned -> nullptr
            if (!node.assignee->params.empty()) {
                auto &val = values[i];
                stmt->assignee = std::move(val);
//                stmt->assignee->parent = stmt.get();
            }
            stmt->set_child_parents();
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
//        node_body->parent = node.parent;
//        node_body->update_parents(node.parent);
        node_body->set_child_parents();
//        node_body->accept(*this);
        replacement_node = std::move(node_body);
//        node.replace_with(std::move(node_body));
    }

    void inline visit(NodeAssignStatement &node) override {
        // error handling
        if(node.array_variable->params.size() < node.assignee->params.size()) {
            auto error = CompileError(ErrorType::SyntaxError,
                         "", node.tok.line, "", "", node.tok.file);
            error.m_message = "Found incorrect assign statement syntax. There are more values to assign than array variables.";
            error.m_expected = node.array_variable->params.size();
            error.m_got = node.assignee->params.size();
            error.exit();
        }

        std::vector<std::unique_ptr<NodeSingleAssignStatement>> assign_statements;
        for(auto &arr_var : node.array_variable->params) {
            auto node_single_assign_stmt = std::make_unique<NodeSingleAssignStatement>(std::move(arr_var), nullptr, node.tok);
            assign_statements.push_back(std::move(node_single_assign_stmt));
        }
        std::vector<std::unique_ptr<NodeAST>> values;
        for(auto &assignee : node.assignee->params) {
            values.push_back(std::move(assignee));
        }
        // there were more variables given than values -> repeat the last value
        while(values.size() < assign_statements.size()) {
            values.push_back(values.back()->clone());
        }

        auto node_body = std::make_unique<NodeBody>(node.tok);
        for(int i = 0; i<assign_statements.size(); i++) {
            auto &stmt = assign_statements[i];
            auto &val = values[i];
            stmt->assignee = std::move(val);
            stmt->set_child_parents();
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(stmt), node.tok));
        }
//        node_body->parent = node.parent;
//        node_body->update_parents(node.parent);
//        node_body->accept(*this);
        node_body->set_child_parents();
        replacement_node = std::move(node_body);
//        node.replace_with(std::move(node_body));
    }


};