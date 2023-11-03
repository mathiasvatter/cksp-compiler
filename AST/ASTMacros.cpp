//
// Created by Mathias Vatter on 03.11.23.
//

#include "ASTMacros.h"
#include "ASTDesugar.h"

void ASTMacros::visit(NodeProgram &node) {
    m_main_ptr = &node;
    m_macro_definitions = std::move(node.macro_definitions);
    m_callbacks = std::move(node.callbacks);
    m_function_definitions = std::move(node.function_definitions);
    for(auto & macro_call : node.macro_calls)
        macro_call->accept(*this);
    for(auto & func_def : m_function_definitions) {
        func_def->accept(*this);
    }
    for(auto & callback : m_callbacks) {
        callback->accept(*this);
    }
    node.callbacks = std::move(m_callbacks);
    node.function_definitions = std::move(m_function_definitions);
}

void ASTMacros::visit(NodeCallback &node) {
    node.statements->accept(*this);

}

void ASTMacros::visit(NodeFunctionDefinition &node) {
    node.header->accept(*this);
    for(auto &stmt : node.body) {
        stmt->accept(*this);
    }
    if(node.return_variable.has_value()) node.return_variable.value()->accept(*this);

}

void ASTMacros::visit(NodeMacroCall &node) {
    if(std::find(m_macro_call_stack.begin(), m_macro_call_stack.end(), node.macro->name) != m_macro_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive macro call detected. Calling macros inside their definition is not allowed.", node.tok.line, "", node.macro->name, node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    node.macro->accept(*this);
    // substitution
    if(auto node_macro_definition = get_macro_definition(node.macro.get())) {
        m_macro_call_stack.push_back(node.macro->name);
        node_macro_definition->parent = node.parent;
        auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
        node_statement_list->parent = node.parent;
        auto substitution_vec = get_substitution_vector(node_macro_definition->header.get(), node.macro.get());
        m_substitution_stack.push(std::move(substitution_vec));
        for(auto& stmt: node_macro_definition->body) {
            stmt->accept(*this);
            if(is_program_level_macro(&node)) {
                if(auto node_function_def = cast_node<NodeFunctionDefinition>(stmt.get())) {
                    m_function_definitions.push_back(std::unique_ptr<NodeFunctionDefinition>(static_cast<NodeFunctionDefinition*>(stmt.release())));
                } else if (auto node_callback = cast_node<NodeCallback>(stmt.get())) {
                    m_callbacks.push_back(std::unique_ptr<NodeCallback>(static_cast<NodeCallback*>(stmt.release())));
                } else {
                    CompileError(ErrorType::PreprocessorError,"Found <statement> on program level. Maybe an incorrectly called macro outside a callback?", stmt->tok.line, "", node.macro->name, node.macro->tok.file).print();
                    exit(EXIT_FAILURE);
                }
            } else {
                stmt->parent = node_statement_list.get();
                if(auto node_statement = cast_node<NodeStatement>(stmt.get())) {
                    node_statement_list->statements.push_back(std::unique_ptr<NodeStatement>(static_cast<NodeStatement*>(stmt.release())));
                } else {
                    CompileError(ErrorType::PreprocessorError,"Found nested callbacks. Maybe an incorrectly called macro inside a callback?", stmt->tok.line, "", node.macro->name, node.macro->tok.file).print();
                    exit(EXIT_FAILURE);
                }
            }
        }
        m_substitution_stack.pop();
        if(!is_program_level_macro(&node)) node.replace_with(std::move(node_statement_list));
        m_macro_call_stack.pop_back();

    }

}

std::unique_ptr<NodeMacroDefinition> ASTMacros::get_macro_definition(NodeMacroHeader *macro_header) {
    for(auto & macro_def : m_macro_definitions) {
        if(macro_def->header->name == macro_header->name) {
            if(macro_def->header->args->params.size() == macro_header->args->params.size()) {
                auto copy = macro_def->clone();
                copy->update_parents(nullptr);
                return std::unique_ptr<NodeMacroDefinition>(static_cast<NodeMacroDefinition*>(copy.release()));
            }
        }
    }
    return nullptr;
}

std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> ASTMacros::get_substitution_vector(NodeMacroHeader* definition, NodeMacroHeader* call) {
    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i];
        std::pair<std::string, std::unique_ptr<NodeAST>> pair;
        if(auto node_variable = cast_node<NodeVariable>(var.get())) {
            pair.first = node_variable->name;
        } else if (auto node_array = cast_node<NodeArray>(var.get())) {
            pair.first = node_array->name;
        } else if (auto node_function_call = cast_node<NodeFunctionCall>(var.get())) {
            pair.first = node_function_call->function->name;
        } else if (auto node_int = cast_node<NodeInt>(var.get())) {
            pair.first = std::to_string(node_int->value);
        } else {
            CompileError(ErrorType::SyntaxError,
         "Unable to substitute macro arguments. Only <keywords> can be substituted.", definition->tok.line, "<keyword>", var->tok.val,definition->tok.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.push_back(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<NodeAST> ASTMacros::get_substitute(const std::string& name) {
    for(auto & pair : m_substitution_stack.top()) {
        if(pair.first == name) {
            return pair.second->clone();
        }
    }
    return nullptr;
}

bool ASTMacros::is_program_level_macro(NodeMacroCall *macro_call) {
    return macro_call->parent == m_main_ptr;
}

