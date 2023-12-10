//
// Created by Mathias Vatter on 08.12.23.
//

#include "ASTVariables.h"

ASTVariables::ASTVariables(const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
                           const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables,
                           const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays)
        : m_builtin_functions(m_builtin_functions), m_builtin_variables(m_builtin_variables), m_builtin_arrays(m_builtin_arrays) {}


void ASTVariables::visit(NodeProgram& node) {
    // check for init callback; get pointer to init callback
    for(auto & callback : node.callbacks) {
        if(callback->begin_callback == "on init") m_init_callback = callback.get();
    }
    if(!m_init_callback) {
        CompileError(ErrorType::SyntaxError, "Unable to compile. Missing <init callback>.", -1, "", "", node.tok.file).exit();
    }
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTVariables::visit(NodeCallback& node) {
    if(node.callback_id)
        node.callback_id->accept(*this);
    node.statements->accept(*this);
}

void ASTVariables::visit(NodeArray& node) {

    if(is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
        if(get_builtin_array(&node)) {
            CompileError(ErrorType::SyntaxError,"Array declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(get_declared_array(node.name)) {
            CompileError(ErrorType::SyntaxError,"Array has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
            m_declared_arrays.push_back(&node);
        }
    } else {
        // in case the user wants the raw array
        bool has_compiler_identifier = node.name[0] == '_';
        if (has_compiler_identifier) node.name = node.name.erase(0,1);

        auto node_declaration = get_declared_array(node.name);
        if(node_declaration) node_declaration->is_used = true;
        if(node_declaration and not has_compiler_identifier) {
            node.declaration = node_declaration;
            node.dimensions = node_declaration->dimensions;
            // get var type from declaration because of List
            if(node_declaration->var_type == List) node.var_type = node_declaration->var_type;
            // only copy sizes from declaration if there is an index (passing arrays only by keyword)
            if(!node.indexes->params.empty()) {
                node.sizes = std::unique_ptr<NodeParamList>(
                        static_cast<NodeParamList *>(node_declaration->sizes->clone().release()));
                node.sizes->update_parents(&node);
            }

            // convert indexes of multidimensional array
            if(node.dimensions > 1 and !node.indexes->params.empty()) {
                auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0, node.tok);
                node.indexes->params.clear();
                node.indexes->params.push_back(std::move(node_expression));
                node.indexes->update_parents(&node);
            }

            // convert indexes of list
            if(node.var_type == List) {
                if(node.indexes->params.size() != 2) {
                    CompileError(ErrorType::SyntaxError,"Got wrong amount of indexes for <list>.", node.tok.line, "2", std::to_string(node.indexes->params.size()), node.tok.file).print();
                    exit(EXIT_FAILURE);
                }
                auto node_position_array = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(
                                                                              get_declared_array(node.name+".pos")->clone().release()));
                node_position_array->indexes->params.clear();
                node_position_array->indexes->params.push_back(std::move(node.indexes->params[0]));

                auto node_expression = make_binary_expr(ASTType::Integer, "+", std::move(node_position_array), std::move(node.indexes->params[1]), &node, node.tok);
                node.indexes->params.clear();
                node.indexes->params.push_back(std::move(node_expression));
                node.indexes->update_parents(&node);
            }

        } else if(auto builtin_var = get_builtin_array(&node)) {
            node.declaration = builtin_var;
        } else if (node_declaration and has_compiler_identifier) {
            node.declaration = node_declaration;
            node.dimensions = 1;
            node.var_type = Array;
            node.name = "_"+node.name;
        } else {
            CompileError(ErrorType::TypeError,"Array has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
    }
    node.sizes->accept(*this);
    node.indexes->accept(*this);
}

void ASTVariables::visit(NodeVariable& node) {
    auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declare_statement and node_declare_statement->to_be_declared.get() == &node) {
        if(get_builtin_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).exit();
        }
        if(get_declared_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
//            exit(EXIT_FAILURE);
        } else {
            m_declared_variables.push_back(&node);
        }
    } else if (auto node_ui_control = cast_node<NodeUIControl>(node.parent)) {
        if(get_builtin_variable(&node)) {
            CompileError(ErrorType::TypeError,"Variable declaration shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
        if(get_declared_control(node_ui_control)) {
            CompileError(ErrorType::TypeError,"Control Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
            m_declared_controls.push_back(node_ui_control);
            m_declared_variables.push_back(&node);
        }
    } else {
        // sometimes a variable can also be an array if notated without brackets -> replace with array node
        auto node_first_declaration = get_declared_variable(&node);
        auto node_first_array_declaration = get_declared_array(node.name);
        if(node_first_declaration || node_first_array_declaration) {
            if(node_first_declaration) {
                node.declaration = node_first_declaration;
                node_first_declaration->is_used = true;
            }
            if(node_first_array_declaration) {
                auto node_array = make_array(node.name, 0, node.tok, node.parent);
                node_array->sizes->params.clear();
                node_array->declaration = node_first_array_declaration;
                node_array->accept(*this);
                node.replace_with(std::move(node_array));
            }
        } else if(auto builtin_var = get_builtin_variable(&node)) {
            node.declaration = builtin_var;
        } else {
            CompileError(ErrorType::TypeError,"Variable has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        }
    }
}

void ASTVariables::visit(NodeFunctionCall &node) {
    node.function->accept(*this);
}

void ASTVariables::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);
    if(node.assignee)
        node.assignee -> accept(*this);

}


void ASTVariables::visit(NodeSingleAssignStatement &node) {
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}

void ASTVariables::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    for(int i=0; i<node.statements.size(); ++i) {
        if(auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get())) {
            // Wir speichern die Statements der inneren NodeStatementList
            auto& inner_statements = node_statement_list->statements;
            // Fügen Sie die inneren Statements an der aktuellen Position ein
            node.statements.insert(
                    node.statements.begin() + i + 1,
                    std::make_move_iterator(inner_statements.begin()),
                    std::make_move_iterator(inner_statements.end())
            );
            // Entfernen Sie das ursprüngliche NodeStatementList-Element
            node.statements.erase(node.statements.begin() + i);
            // Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
            i += inner_statements.size() - 1;
            // Die inneren Statements sind jetzt leer, da sie verschoben wurden
            inner_statements.clear();
        }
    }
    node.update_parents(&node);
}



NodeFunctionHeader* ASTVariables::get_builtin_function(const std::string &function) {
    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
                               return (func->name == function);
                           });
    if(it != m_builtin_functions.end()) {
        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
    }
    return nullptr;
}

NodeVariable* ASTVariables::get_builtin_variable(NodeVariable *var) {
    auto it = std::find_if(m_builtin_variables.begin(), m_builtin_variables.end(),
                           [&](const std::unique_ptr<NodeVariable> &variable) {
                               return variable->name == var->name;
                           });
    if(it != m_builtin_variables.end()) {
        return m_builtin_variables[std::distance(m_builtin_variables.begin(), it)].get();
    }
    return nullptr;
}

NodeArray* ASTVariables::get_builtin_array(NodeArray *arr) {
    auto it = std::find_if(m_builtin_arrays.begin(), m_builtin_arrays.end(),
                           [&](const std::unique_ptr<NodeArray> &array) {
                               return array->name == arr->name;
                           });
    if(it != m_builtin_arrays.end()) {
        return m_builtin_arrays[std::distance(m_builtin_arrays.begin(), it)].get();
    }
    return nullptr;
}

NodeVariable *ASTVariables::get_declared_variable(NodeVariable *var) {
    auto it = std::find_if(m_declared_variables.begin(), m_declared_variables.end(),
                           [&](NodeVariable* variable) {
                               return to_lower(variable->name) == to_lower(var->name);
                           });
    if(it != m_declared_variables.end()) {
        return m_declared_variables[std::distance(m_declared_variables.begin(), it)];
    }
    return nullptr;
}

NodeArray *ASTVariables::get_declared_array(const std::string& arr) {
    auto it = std::find_if(m_declared_arrays.begin(), m_declared_arrays.end(),
                           [&](NodeArray* array) {
                               return array->name == arr;
                           });
    if(it != m_declared_arrays.end()) {
        return m_declared_arrays[std::distance(m_declared_arrays.begin(), it)];
    }
    return nullptr;
}

NodeUIControl *ASTVariables::get_declared_control(NodeUIControl *ctr) {
    auto it = std::find_if(m_declared_controls.begin(), m_declared_controls.end(),
                           [&](NodeUIControl* control) {
                               return to_lower(control->get_string()) == to_lower(ctr->get_string());
                           });
    if(it != m_declared_controls.end()) {
        return m_declared_controls[std::distance(m_declared_controls.begin(), it)];
    }
    return nullptr;
}

std::unique_ptr<NodeAST> ASTVariables::calculate_index_expression(
        const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {

    // Basisfall: letztes Element in der Berechnung
    if (dimension == indices.size() - 1) {
        return indices[dimension]->clone();
    }

    // Produkt der Größen der nachfolgenden Dimensionen
    std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
    for (size_t i = dimension + 2; i < sizes.size(); ++i) {
        size_product = std::make_unique<NodeBinaryExpr>("*", std::move(size_product), sizes[i]->clone(), tok);
    }

    // Berechnung des aktuellen Teils der Formel
    std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
            "*", indices[dimension]->clone(), std::move(size_product), tok);

    // Rekursiver Aufruf für den nächsten Teil der Formel
    std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

    // Kombinieren des aktuellen Teils mit dem nächsten Teil
    return std::make_unique<NodeBinaryExpr>("+", std::move(current_part), std::move(next_part), tok);
}
