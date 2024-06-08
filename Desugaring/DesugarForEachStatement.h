//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * @brief This class is responsible for desugaring for each loops into for loops in the AST.
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
    void inline visit(NodeVariableRef& node) override {
        // range-based for-loop substitution
        if(!m_key_value_scope_stack.empty()) {
            if(auto substitute = get_key_value_substitute(node.name)) {
                substitute->update_parents(node.parent);
                node.replace_with(std::move(substitute));
                return;
            }
        }
    }

    void inline visit(NodeForEachStatement& node) override {
        auto error = CompileError(ErrorType::SyntaxError, "", node.tok.line, "", "", node.tok.file);
        error.m_message = "Found incorrect for-each-loop syntax.";
        // check if keys are variable references
        for(auto &var : node.keys->params) {
            if(var->get_node_type() != NodeType::VariableRef) {
                error.m_message = "Found incorrect key in ranged-for-loop syntax.";
                error.m_expected = "<Variable>";
                error.m_got = var->get_string();
                error.exit();
            }
        }
        if(node.keys->params.size() != 2) {
            error.m_expected = "key, value";
            error.m_got = node.keys->params[0]->get_string();
            error.exit();
        }
        // range has to be an array that was parsed as variable ref
        if(node.range->get_node_type() != NodeType::VariableRef) {
            error.m_expected = "<Array>";
            error.m_got = node.range->get_string();
            error.exit();
        }
        auto node_key_ref = static_cast<NodeVariableRef*>(node.keys->params[0].get());
        auto node_key_variable = std::make_unique<NodeVariable>(std::nullopt, node_key_ref->name, TypeRegistry::Integer, DataType::Mutable, node.tok);
        node_key_variable->is_local = true;
        node_key_variable->ty = TypeRegistry::Integer;

        auto node_key_declaration = std::make_unique<NodeSingleDeclaration>(
                std::move(node_key_variable),
                nullptr, node.tok);
        auto node_key_iterator = std::make_unique<NodeSingleAssignment>(
                node.keys->params[0]->clone(),
                std::make_unique<NodeInt>(0, node.tok), node.tok);
        Token token_to = Token(token::TO, "to", node.tok.line, node.tok.pos, node.tok.file);

        auto node_num_elements = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"num_elements",
				std::make_unique<NodeParamList>(node.tok, node.range->clone()),
				node.tok
				),
			node.tok
		);
        auto node_end_range = std::make_unique<NodeBinaryExpr>(
			token::SUB,
			std::move(node_num_elements),
			std::make_unique<NodeInt>(1, node.tok),
			node.tok
        );
        node_end_range->ty = TypeRegistry::Integer;

        // add key-value pair to scope stack of array with <key> as index that needs to be substituted for <value>
        auto node_value_array = std::make_unique<NodeArrayRef>(node.range->get_string(), std::move(node.keys->params[0]), node.tok);
        m_key_value_scope_stack.emplace_back();
        m_key_value_scope_stack.back().insert({node.keys->params[1]->get_string(), std::move(node_value_array)});

        auto node_for_statement = std::make_unique<NodeForStatement>(
                std::move(node_key_iterator),
                token_to,
                std::move(node_end_range),
                std::move(node.statements), node.tok);
        node_for_statement->statements->accept(*this);
        auto node_scope = std::make_unique<NodeBody>(node.tok);
        node_scope->add_stmt(std::make_unique<NodeStatement>(std::move(node_key_declaration), node.tok));
        node_scope->add_stmt(std::make_unique<NodeStatement>(std::move(node_for_statement), node.tok));
        node_scope->scope = true;
//        node_scope->accept(*this);

        m_key_value_scope_stack.pop_back();
        replacement_node = std::move(node_scope);
    }

};