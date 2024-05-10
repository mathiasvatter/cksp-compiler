//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * This class is responsible for desugaring for each loops into for loops
 * in the AST.
 * In this case, a for each loop is rewritten as a for loop.
 *
 * The transformation is as follows:
 *
 * Pre-desugaring:
 * declare array[10] := (3,4,6,8)
 * for _, val in array
 *     message(val)
 * end for
 *
 * Post-desugaring:
 * declare array[10] := (3,4,6,8)
 * for _ := 0 to num_elements(array) - 1
 *     message(array[_])
 * end for
 */
class DesugarForEachStatement : public ASTDesugaring {
private:
    std::vector<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_key_value_scope_stack;
    std::unique_ptr<NodeAST> get_key_value_substitute(const std::string& name) {
        for (auto rit = m_key_value_scope_stack.rbegin(); rit != m_key_value_scope_stack.rend(); ++rit) {
            auto it = rit->find(name);
            if(it != rit->end()) {
                return it->second->clone();
            }
        }
        return nullptr;
    }

public:
    void inline visit(NodeVariable& node) override {
        // range-based for-loop substitution
        if(!m_key_value_scope_stack.empty() and node.is_reference) {
            if(auto substitute = get_key_value_substitute(node.name)) {
                substitute->update_parents(node.parent);
                if(substitute->type == ASTType::Unknown)
                    substitute->type = node.type;
                node.replace_with(std::move(substitute));
                return;
            }
        }
    }

    void inline visit(NodeForEachStatement& node) override {
        auto error = CompileError(ErrorType::SyntaxError, "", node.tok.line, "", "", node.tok.file);
        error.m_message = "Found incorrect for-each-loop syntax.";
        // check if keys are either variable or array objects
        for(auto &var : node.keys->params) {
            if(!is_instance_of<NodeVariable>(var.get())) {
                error.m_message = "Found incorrect key in ranged-for-loop syntax.";
                error.m_expected = "<variable>";
                error.m_got = var->get_string();
                error.exit();
            }
        }
        if(node.keys->params.size() != 2) {
            error.m_expected = "key, value";
            error.m_got = node.keys->params[0]->get_string();
            error.exit();
        }
        if(!is_instance_of<NodeVariable>(node.range.get())) {
            error.m_expected = "<array_variable>";
            error.m_got = node.range->get_string();
            error.exit();
        }

        auto node_key_variable = static_cast<NodeVariable*>(node.keys->params[0].get());
        node_key_variable->is_local = true;
        node_key_variable->type = ASTType::Integer;
        auto node_key_declaration = std::make_unique<NodeSingleDeclareStatement>(clone_as<NodeVariable>(node_key_variable),nullptr, node.tok);
        node_key_declaration->to_be_declared->is_reference = false;
        auto node_key_iterator = std::make_unique<NodeSingleAssignStatement>(node.keys->params[0]->clone(), make_int(0, &node), node.tok);
        Token token_to = Token(token::TO, "to", node.tok.line, node.tok.pos, node.tok.file);
        std::vector<std::unique_ptr<NodeAST>> args;
        args.push_back(node.range->clone());
        auto node_num_elements = make_function_call("num_elements", std::move(args), &node, node.tok);
        auto node_end_range = make_binary_expr(ASTType::Integer, token::SUB, std::move(node_num_elements->statement), make_int(1, &node), &node, node.tok);
        auto node_value_array = make_array(node.range->get_string(), 1, node.tok, &node);
        node_value_array->size = nullptr;
        node_value_array->index = std::move(node.keys->params[0]);
        m_key_value_scope_stack.emplace_back();
        m_key_value_scope_stack.back().insert({node.keys->params[1]->get_string(), std::move(node_value_array)});

        auto node_for_statement = std::make_unique<NodeForStatement>(std::move(node_key_iterator), token_to, std::move(node_end_range), std::move(node.statements), node.tok);
        auto node_scope = std::make_unique<NodeBody>(node.tok);
        node_scope->statements.push_back(statement_wrapper(std::move(node_key_declaration), node_scope.get()));
        node_scope->statements.push_back(statement_wrapper(std::move(node_for_statement), node_scope.get()));
        node_scope->scope = true;
        node_scope->update_parents(node.parent);
        node_scope->accept(*this);

        m_key_value_scope_stack.pop_back();
        replacement_node = std::move(node_scope);
//        node.replace_with(std::move(node_scope));
    }

};