//
// Created by Mathias Vatter on 03.11.23.
//

#include "ASTMacros.h"
#include "ASTDesugar.h"
#include "../Preprocessor/Preprocessor.h"

void ASTMacros::visit(NodeProgram &node) {
    m_main_ptr = &node;
    m_macro_definitions = std::move(node.macro_definitions);
    m_callbacks = std::move(node.callbacks);
    m_function_definitions = std::move(node.function_definitions);
    m_macro_iterations = std::move(node.macro_iterations);
    m_macro_literations = std::move(node.macro_literations);
    for(auto & literate_macro : m_macro_literations)
        literate_macro->accept(*this);
    for(auto & iterate_macro : m_macro_iterations)
        iterate_macro->accept(*this);
    for(auto & macro_call : node.macro_calls)
        macro_call->accept(*this);
    for(auto & func_def : m_function_definitions)
        func_def->accept(*this);
    for(auto & callback : m_callbacks)
        callback->accept(*this);
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

void ASTMacros::visit(NodeVariable &node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (count_char(node.name, '#') == 2) {
            node.name = get_text_replacement(node.name);
        } else if (auto substitute = get_substitute(node.name)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void ASTMacros::visit(NodeArray &node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (count_char(node.name, '#') == 2) {
            node.name = get_text_replacement(node.name);
        } else if (auto substitute = get_substitute(node.name)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
}

void ASTMacros::visit(NodeFunctionCall &node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        auto substitute = get_substitute(node.function->name);
        node.function->name = get_text_replacement(node.function->name);
        if(is_instance_of<NodeArray>(substitute.get())) {
            CompileError(ErrorType::PreprocessorError,"Cannot substitute function name with macro argument of type <array>.", node.tok.line, "", node.function->name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
    }
    node.function->accept(*this);
}


void ASTMacros::visit(NodeString &node) {
    // substitution
    if (!m_substitution_stack.empty()) {
        if (count_char(node.value, '#') == 2) {
            node.value = get_text_replacement(node.value);
        }
    }
}

void ASTMacros::visit(NodeMacroCall &node) {
    if(std::find(m_macro_call_stack.begin(), m_macro_call_stack.end(), node.macro->name) != m_macro_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::PreprocessorError,"Recursive macro call detected. Calling macros inside their definition is not allowed.", node.tok.line, "", node.macro->name, node.tok.file).print();
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

void ASTMacros::visit(NodeIterateMacro& node) {
    if(is_instance_of<NodeIterateMacro>(node.parent->parent)) {
        CompileError(ErrorType::PreprocessorError,"Found nested <iterate_macro>.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    SimpleInterpreter evaluator;
    auto iterator_start = evaluator.evaluate_int_expression(node.iterator_start);
    if(iterator_start.is_error()) {
        iterator_start.get_error().print();
        exit(EXIT_FAILURE);
    }
    auto iterator_end = evaluator.evaluate_int_expression(node.iterator_end);
    if(iterator_end.is_error()) {
        iterator_end.get_error().print();
        exit(EXIT_FAILURE);
    }
    auto from = iterator_start.unwrap();
    auto to = iterator_end.unwrap();

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    node_statement_list->parent = node.parent;
    int i = from;
    while(node.to.type == DOWNTO ? i > to : i < to) {
        std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> substitution_vector;
        substitution_vector.emplace_back(std::pair("#n#", make_int(i, node.parent)));
        m_substitution_stack.push(std::move(substitution_vector));

        auto macro_call = node.macro_call->statement->clone();
        macro_call->update_parents(&node);
        // is real macro call
        if (auto node_macro_call = cast_node<NodeMacroCall>(macro_call.get())) {
            node_macro_call->macro->args->params.push_back(make_int(i, node.parent));
        } else if (node.parent == m_main_ptr) {
            CompileError(ErrorType::PreprocessorError,"Cannot iterate <statements> on program level.", node.tok.line, "", "", node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        auto node_statement = statement_wrapper(std::move(macro_call), &node);
        node_statement->accept(*this);
        node_statement->parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(node_statement));
        m_substitution_stack.pop();

        if(node.to.type == DOWNTO) i--; else i++;
    }

    node.replace_with(std::move(node_statement_list));
}

void ASTMacros::visit(NodeLiterateMacro& node) {

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
    for (auto &pair: m_substitution_stack.top()) {
        if (pair.first == name) {
            return pair.second->clone();
        }
    }
    return nullptr;
}

std::string ASTMacros::get_text_replacement(const std::string& name) {
    std::string substr = name;
    std::size_t start;
    std::size_t end;
    if(count_char(name, '#') == 2) {
        start = name.find('#');
        end = name.find('#', start + 1);
        substr = name.substr(start, end - start + 1);
    }
    for (auto &pair: m_substitution_stack.top()) {
        if (pair.first == substr) {
            std::string new_name ;
            if(auto node_variable = cast_node<NodeVariable>(pair.second.get())) {
                new_name = node_variable->name;
            } else if (auto node_array = cast_node<NodeArray>(pair.second.get())) {
                new_name = node_array->name;
            } else if (auto node_function_call = cast_node<NodeFunctionCall>(pair.second.get())) {
                new_name = node_function_call->function->name;
            } else if (auto node_int = cast_node<NodeInt>(pair.second.get())) {
                new_name = std::to_string(node_int->value);
            } else {
                CompileError(ErrorType::SyntaxError,
                 "Unable to substitute macro arguments. Only <keywords> can be substituted.", pair.second->tok.line, "<keyword>", pair.second->tok.val,pair.second->tok.file).print();
                exit(EXIT_FAILURE);
            }
            if(count_char(name, '#') == 2)
                return name.substr(0, start) + new_name + name.substr(end+1);
            else
                return new_name;
        }

    }
    return name;
}


//} else if(count_char(placeholders[i][0].val, '#') == 2 and contains(macro_body[j].val, placeholders[i][0].val)) {
//    std::string hashtag_replacement;
//for(auto & arg : new_args[i]) {
//    hashtag_replacement += arg.val;
//}
//macro_body[j].val.replace(macro_body[j].val.find(placeholders[i][0].val), placeholders[i][0].val.size(), hashtag_replacement);
//    tokenReplaced = true;
//    break; // break out of the inner loop since we've replaced the token
//}
//

bool ASTMacros::is_program_level_macro(NodeMacroCall *macro_call) {
    auto is_iterate_macro = is_instance_of<NodeIterateMacro>(macro_call->parent->parent) and macro_call->parent->parent->parent == m_main_ptr;
    return macro_call->parent == m_main_ptr or is_iterate_macro;
}



