//
// Created by Mathias Vatter on 29.03.24.
//

#include "ASTFunctionInlining.h"
#include <functional>

ASTFunctionInlining::ASTFunctionInlining(const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions,
                       const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &m_property_functions)
        : m_builtin_functions(m_builtin_functions), m_property_functions(m_property_functions) {
}

void ASTFunctionInlining::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & def : node.function_definitions) {
        m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
    }
    m_function_definitions = std::move(node.function_definitions);

    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            m_functions_in_use.insert({function->header->name, function.get()});
            function->body->accept(*this);
            m_functions_in_use.erase(function->header->name);
        }
    }
}

void ASTFunctionInlining::visit(NodeArray& node) {

    node.sizes->accept(*this);
    node.indexes->accept(*this);
    // function args substitution
    if(!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.name)) {
            if(is_instance_of<NodeArray>(substitute.get()) || is_instance_of<NodeVariable>(substitute.get())) {
                node.name = substitute->get_string();
            } else {
                node.replace_with(std::move(substitute));
                return;
            }
        }
    }
}

void ASTFunctionInlining::visit(NodeFunctionHeader& node) {
    node.args->accept(*this);
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.name)) {
            node.name = substitute->get_string();
            return;
        }
    }
}

void ASTFunctionInlining::visit(NodeVariable& node) {
    // substitution
    if(!m_substitution_stack.empty()) {
        if(auto substitute = get_substitute(node.name)) {
            substitute->update_parents(node.parent);
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void ASTFunctionInlining::visit(NodeFunctionCall& node) {
    if(node.is_call and !node.function->args->params.empty()) {
        CompileError(ErrorType::SyntaxError,
                     "Found incorrect amount of function arguments when using <call>.", node.tok.line, "0", std::to_string(node.function->args->params.size()), node.tok.file).exit();
    }
    if(m_functions_in_use.find(node.function->name) != m_functions_in_use.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive function call detected. Calling functions inside their definition is not allowed.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
    NodeAST* old_function_inline_statement = m_current_function_inline_statement;
    NodeStatement* nearest_statement = static_cast<NodeStatement*>(m_current_function_inline_statement);
    node.function->accept(*this);

    // substitution start
    if (auto function_def = get_function_definition(node.function.get())) {
        if(evaluating_functions and m_current_callback != m_init_callback and function_def->header->args->params.empty() and !function_def->return_variable.has_value())
            m_function_call_order.push_back(function_def);
        if(node.is_call and function_def->return_variable.has_value()) {
            CompileError(ErrorType::SyntaxError, "Found incorrect use of return variable when using <call>.", node.tok.line, "", "", node.tok.file).exit();
        }
        function_def->call.insert(&node);
        m_current_function = function_def;
        // inline if current call is <on init>
        if(m_current_callback != m_init_callback) {
            if (node.is_call) {
                if(function_def->is_compiled) {
                    return;
                } else {
                    function_def->is_compiled = true;
                }
            }

        } else if(node.is_call){
            CompileError(ErrorType::SyntaxError, "The usage of <call> keyword is not allowed in the <on init> callback. Automatically removed <call> and inlined function. Consider not using the <call> keyword.", node.tok.line, "", "<call>", node.tok.file).print();
            node.is_call = false;
        }

        auto node_function_def = std::unique_ptr<NodeFunctionDefinition>(static_cast<NodeFunctionDefinition*>(function_def->clone().release()));
        node_function_def->update_parents(nullptr);
        m_function_call_stack.push(node.function->name);
        m_functions_in_use.insert({node.function->name, function_def});
//        m_variable_scope_stack.emplace_back();
        node_function_def->parent = node.parent;
        if (!node.function->args->params.empty()) {
            auto substitution_map = get_substitution_map(node_function_def->header.get(), node.function.get());
            m_substitution_stack.push(std::move(substitution_map));
        }
        node_function_def->body->update_token_data(node.tok);
        node_function_def->body->accept(*this);
        if (!node.function->args->params.empty()) m_substitution_stack.pop();
//        m_variable_scope_stack.pop_back();
        m_functions_in_use.erase(node.function->name);
        m_function_call_stack.pop();
        function_def->call.erase(&node);
        // has return variable
        if(node_function_def->return_variable.has_value()) {
            if(is_instance_of<NodeStatementList>(node.parent->parent)) {
                CompileError(ErrorType::SyntaxError,"Function returns a value. Move function into assign statement.", node.tok.line, node_function_def->return_variable.value()->get_string(), "<assignment>", node.tok.file).exit();
            }
            if (!node_function_def->body->statements.empty()) {
                auto node_assignment = cast_node<NodeSingleAssignStatement>(node_function_def->body->statements[0]->statement.get());
                // strict inlining method only if function body is assign statement
                if (node_function_def->body->statements.size() == 1 and node_assignment) {
                    if (node_assignment->array_variable->get_string() == node_function_def->return_variable.value()->get_string()) {
                        node.replace_with(std::move(node_assignment->assignee));
                        return;
                    } else {
                        CompileError(ErrorType::SyntaxError,
                                     "Given return variable of function could not be found in function body.",
                                     node.tok.line, node_function_def->return_variable.value()->get_string(),
                                     node_assignment->array_variable->get_string(), node.tok.file).exit();
                    }
                    // new inlining method
                } else {
                    std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
                    auto node_return_var_name =
                            "_return_var_" + std::to_string(nearest_statement->function_inlines.size());
                    auto node_return_var = std::make_unique<NodeVariable>(std::optional<Token>(), node_return_var_name, Mutable,
                                                                          node.tok);
                    node_return_var->declaration = m_return_dummy_declaration;
                    node_return_var->is_compiler_return = true;
                    node_return_var->parent = node.parent;
                    substitution_map.insert({node_function_def->return_variable.value()->get_string(),
                                             node_return_var->clone()});
                    auto node_parent_parent = node.parent->parent;
                    node.replace_with(std::move(node_return_var));
                    m_substitution_stack.push(std::move(substitution_map));
                    node_function_def->body->accept(*this);
                    m_substitution_stack.pop();
                    node_function_def->body->parent = node_parent_parent;
                    auto node_statement = statement_wrapper(std::move(node_function_def->body), nullptr);
                    auto ptr = node_statement->statement.get();
                    m_function_inlines.insert({ptr, std::move(node_statement)});
                    nearest_statement->function_inlines.push_back(ptr);
                    m_current_function_inline_statement = old_function_inline_statement;
                    return;
                }
            }
        }
        // inline replacement if not call
        if(!node.is_call) {
            // Only replace if function has actually statements
            if (!node_function_def->body->statements.empty()) {
                node.replace_with(std::move(node_function_def->body));
            } else {
                node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
            }
        } else {
            m_program->function_definitions.push_back(std::move(node_function_def));
        }
    } else if (auto builtin_func = get_builtin_function(node.function.get())) {
        node.function->type = builtin_func->type;
        node.function->has_forced_parenth = builtin_func->has_forced_parenth;
        node.function->arg_ast_types = builtin_func->arg_ast_types;
        node.function->arg_var_types = builtin_func->arg_var_types;

        if(m_restricted_builtin_functions.find(builtin_func->name) != m_restricted_builtin_functions.end()) {
            if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
                CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.", node.tok.line, "", "<"+m_current_callback->begin_callback+">", node.tok.file).print();
            } else {
                if(m_current_function)
                    if(m_current_function->call.find(&node) != m_current_function->call.end())
                        CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks. Not in a called function.", node.tok.line, "", "<"+m_current_function->header->name+"> in <"+m_current_callback->begin_callback+">", node.tok.file).print();
            }
        }
        // add missing get_ui_id as first parameter if control_par function is used
//        if (node.function->name.contains("control_par")) {
//            if(auto get_ui_id = cast_node<NodeFunctionCall>(node.function->args->params[0].get())) {
//                if(get_ui_id->function->name != "get_ui_id") {
//                    node.function->args->params[0] = wrap_in_get_ui_id(std::move(node.function->args->params[0]));
//                }
//            } else {
//                node.function->args->params[0] = wrap_in_get_ui_id(std::move(node.function->args->params[0]));
//            }
//        }

    } else if (auto property_func = get_property_function(node.function.get())) {
        if(node.function->args->params.size() < 2)
            CompileError(ErrorType::SyntaxError,"Found Property Function with insufficient amount of arguments.", node.tok.line, "At least 2 arguments", std::to_string(node.function->args->params.size()), node.tok.file).exit();
        auto node_statement_list = inline_property_function(property_func, std::move(node.function));
        node_statement_list->accept(*this);
        node.replace_with(std::move(node_statement_list));
        return;
    } else {
        CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
}

void ASTFunctionInlining::visit(NodeStatement& node) {
//    m_current_function_inline_statement = &node;
    node.statement->accept(*this);
}

void ASTFunctionInlining::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }

    // Ersetzen Sie die alte Liste durch die neue
    if(!node.scope) node.statements = std::move(cleanup_node_statement_list(&node));
}

std::unique_ptr<NodeStatementList> ASTFunctionInlining::inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
    auto node_statement_list = std::make_unique<NodeStatementList>(function_header->tok);
    for(int i = 1; i<function_header->args->params.size(); i++) {
        auto node_get_control = std::make_unique<NodeGetControlStatement>(function_header->args->params[0]->clone(), property_function->args->params[i]->get_string(), function_header->tok);
        auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_get_control), std::move(function_header->args->params[i]), function_header->tok);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_assignment), node_statement_list.get()));
    }
    node_statement_list->update_parents(function_header->parent);
    return std::move(node_statement_list);
}


std::unordered_map<std::string, std::unique_ptr<NodeAST>> ASTFunctionInlining::get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call) {
    std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i];
        std::pair<std::string, std::unique_ptr<NodeAST>> pair;
        if(auto node_variable = cast_node<NodeVariable>(var.get())) {
            pair.first = node_variable->name;
        } else if (auto node_array = cast_node<NodeArray>(var.get())) {
            pair.first = node_array->name;
        } else {
            CompileError(ErrorType::SyntaxError,
                         "Unable to substitute function arguments. Only <keywords> can be substituted.", definition->tok.line, "<keyword>", var->tok.val,definition->tok.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.insert(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<NodeAST> ASTFunctionInlining::get_substitute(const std::string& name) {
    const auto & map = m_substitution_stack.top();
    auto it = map.find(name);
    if(it != map.end()) {
        return it->second->clone();
    }
    return nullptr;
}



std::unique_ptr<NodeFunctionCall> ASTFunctionInlining::wrap_in_get_ui_id(std::unique_ptr<NodeAST> variable) {
    auto parent_tok = variable->parent->tok;
    auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function("get_ui_id",1)->clone().release()));
    node_get_ui_id->args->params.clear();
    node_get_ui_id->args->params.push_back(std::move(variable));
    return std::make_unique<NodeFunctionCall>(false, std::move(node_get_ui_id), parent_tok);
}


NodeFunctionDefinition* ASTFunctionInlining::get_function_definition(NodeFunctionHeader *function_header) {
    auto it = m_function_lookup.find({function_header->name, (int)function_header->args->params.size()});
    if(it != m_function_lookup.end()) {
        it->second->is_used = true;
        return it->second;
    }
    return nullptr;
}

NodeFunctionHeader* ASTFunctionInlining::get_builtin_function(NodeFunctionHeader *function) {
    auto it = m_builtin_functions.find({function->name, (int)function->args->params.size()});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTFunctionInlining::get_builtin_function(const std::string &function, int params) {
    auto it = m_builtin_functions.find({function, params});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTFunctionInlining::get_property_function(NodeFunctionHeader *function) {
    auto it = m_property_functions.find(function->name);
    if(it != m_property_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> ASTFunctionInlining::get_function_inlines() {
    return std::move(m_function_inlines);
}

