//
// Created by Mathias Vatter on 02.04.24.
//

#include "ASTDefinitionChecker.h"
#include "ASTHandler.h"

ASTDefinitionChecker::ASTDefinitionChecker(
        const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &m_builtin_variables,
        const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions,
        const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &m_builtin_arrays,
        const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets,
        const std::vector<std::unique_ptr<NodeAST>> &m_external_variables) :
        m_builtin_variables(m_builtin_variables),
        m_builtin_functions(m_builtin_functions),
        m_builtin_arrays(m_builtin_arrays),
        m_builtin_widgets(m_builtin_widgets),
        m_external_variables(m_external_variables) {

}

void ASTDefinitionChecker::visit(NodeProgram& node) {
    for(auto & external_var : m_external_variables) {
        external_var->accept(*this);
    }
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTDefinitionChecker::visit(NodeVariable& node) {
    verify_variable(&node);
//    if(node.declaration) {
//        node.type = node.declaration->type;
//    }
}

void ASTDefinitionChecker::visit(NodeArray& node) {
    verify_array(&node);
    auto array_handler = node.get_handler();
    if(node.declaration) {
        auto node_lowered = array_handler->perform_lowering(node);
        node.replace_with(std::move(node_lowered));
//        // handle multidimensional array reference
//        // convert indexes of multidimensional array
//        if(node.var_type == Array and node.dimensions > 1 and !node.show_brackets) {
//            auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0, node.tok);
//            node.indexes->params.clear();
//            node.indexes->params.push_back(std::move(node_expression));
//            node.indexes->update_parents(&node);
//        }
//
//        // handle list reference
//        // convert indexes of list
//        if(node.var_type == List and !node.show_brackets) {
//            if(node.indexes->params.size() != 2) {
//                CompileError(ErrorType::SyntaxError,"Got wrong amount of indexes for <list>.", node.tok.line, "2", std::to_string(node.indexes->params.size()), node.tok.file).print();
//                exit(EXIT_FAILURE);
//            }
//            auto node_position_array = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(
//                                                                          get_declared_array(node.name+".pos")->clone().release()));
//            node_position_array->indexes->params.clear();
//            node_position_array->indexes->params.push_back(std::move(node.indexes->params[0]));
//
//            auto node_expression = make_binary_expr(ASTType::Integer, "+", std::move(node_position_array), std::move(node.indexes->params[1]), &node, node.tok);
//            node.indexes->params.clear();
//            node.indexes->params.push_back(std::move(node_expression));
//            node.indexes->update_parents(&node);
//            node.name = "_"+node.name;
//        }
    }
}

void ASTDefinitionChecker::visit(NodeUIControl &node) {
    auto engine_widget = get_builtin_widget(node.ui_control_type);
    if(!engine_widget) {
        CompileError(ErrorType::SyntaxError, "Did not recognize engine widget.", node.tok.line, "valid widget type", node.ui_control_type, node.tok.file).exit();
    }

    // build ui_control array ( make sure control array size is in node.size)
    auto node_array = cast_node<NodeArray>(node.control_var.get());
    auto node_widget_array = cast_node<NodeArray>(engine_widget->control_var.get());
    if(node_array and !node_widget_array) {
        // move array size to ui_control size
        std::swap(node.sizes, node_array->sizes);
    }

    node.control_var->accept(*this);
    node.params->accept(*this);

}

void ASTDefinitionChecker::visit(NodeStatementList& node) {
    if(node.scope) {
        m_declared_arrays.emplace_back();
        m_declared_variables.emplace_back();
        m_declared_controls.emplace_back();
    }
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(node.scope) {
        m_declared_arrays.pop_back();
        m_declared_variables.pop_back();
        m_declared_controls.pop_back();
    }

    // Ersetzen Sie die alte Liste durch die neue
    if(!node.scope) node.statements = std::move(cleanup_node_statement_list(&node));
}



void ASTDefinitionChecker::verify_array(NodeArray *arr, bool is_strict) {
    // return early if a declaration pointer already exists
    if (arr->declaration) {
        return;
    }
    // check if array is builtin, declared, ui_control or raw array
    auto node_builtin_array = get_builtin_array(arr);
    auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(arr->parent);
    if(node_declare_statement and node_declare_statement->to_be_declared.get() != arr) node_declare_statement = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(arr->parent);
    if(node_builtin_array && (node_declare_statement || node_ui_control) ){
        CompileError(ErrorType::SyntaxError,"Array shadows builtin variable. Try renaming the variable.", arr->tok.line, "", arr->name, arr->tok.file).exit();
    }
    if(node_declare_statement) {
        if(get_declared_array(arr->name)) {
            CompileError(ErrorType::SyntaxError,"Array has already been declared.", arr->tok.line, "", arr->name, arr->tok.file).print();
//			exit(EXIT_FAILURE);
        } else {
            m_declared_arrays.back().insert({arr->name, arr});
        }
    } else if (node_ui_control) {
        if(get_declared_control(node_ui_control)) {
            CompileError(ErrorType::SyntaxError,"Control Widget Array has already been declared.", arr->tok.line, "", arr->name, arr->tok.file).exit();
//			exit(EXIT_FAILURE);
        } else {
            m_declared_controls.back().insert({node_ui_control->get_string(), node_ui_control});
            m_declared_arrays.back().insert({arr->name, arr});
        }
        // array is reference -> is not in declaration -> get pointer to declaration
    } else {
        // get the pointer to the array declaration
        if(auto node_declaration = get_declared_array(arr->name)) {
            // copy everything important from declaration to reference
            node_declaration->is_used = true;
            arr->declaration = node_declaration;
//            match_types(arr->declaration, arr);
//            arr->type = arr->declaration->type;
            arr->dimensions = node_declaration->dimensions;
            // get var type from declaration because of List or UI Control
            arr->var_type = node_declaration->var_type;
            // only copy sizes from declaration if there is an index (passing arrays only by keyword)
            arr->sizes = std::unique_ptr<NodeParamList>(static_cast<NodeParamList *>(node_declaration->sizes->clone().release()));
            arr->sizes->update_parents(arr);

        } else if(node_builtin_array) {
            arr->declaration = node_builtin_array;
//            match_types(arr->declaration, arr);
//            arr->type = arr->declaration->type;
            arr->is_engine = node_builtin_array->is_engine;
        } else if (auto node_raw_declaration = get_raw_declared_array(arr->name)) {
            node_raw_declaration->is_used = true;
            arr->declaration = node_raw_declaration;
//            match_types(arr->declaration, arr);
//            arr->type = arr->declaration->type;
            arr->dimensions = 1;
            arr->var_type = Array;
        } else if (is_strict) {
            CompileError(ErrorType::SyntaxError,"Array has not been declared.", arr->tok.line, "", arr->name, arr->tok.file).exit();
//            exit(EXIT_FAILURE);
        }
    }
}

void ASTDefinitionChecker::verify_variable(NodeVariable *var, bool is_strict) {
    // return early if a declaration pointer already exists
    if (var->declaration) {
        return;
    }
    auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(var->parent);
    if(node_declare_statement and node_declare_statement->to_be_declared.get() != var) node_declare_statement = nullptr;
    auto node_builtin_variable = get_builtin_variable(var);
    auto node_ui_control = cast_node<NodeUIControl>(var->parent);
    if(node_builtin_variable && (node_declare_statement || node_ui_control) ){
        CompileError(ErrorType::SyntaxError,"Variable shadows builtin variable. Try renaming the variable.", var->tok.line, "", var->name, var->tok.file).exit();
    }
    if(node_declare_statement) {
        if(get_declared_variable(var->name)) {
            CompileError(ErrorType::SyntaxError,"Variable has already been declared.", var->tok.line, "", var->name, var->tok.file).print();
//            exit(EXIT_FAILURE);
        } else {
            m_declared_variables.back().insert({var->name, var});
            m_variables_declared++;
        }
    } else if (node_ui_control) {
        if(get_declared_control(node_ui_control)) {
            CompileError(ErrorType::SyntaxError,"Control Variable has already been declared.", var->tok.line, "", var->name, var->tok.file).exit();
//            exit(EXIT_FAILURE);
        } else {
            m_declared_controls.back().insert({node_ui_control->get_string(), node_ui_control});
            m_declared_variables.back().insert({var->name, var});
            m_variables_declared++;
        }
    } else {
        if(auto node_var_declaration = get_declared_variable(var->name)) {
            node_var_declaration->is_used = true;
            var->declaration = node_var_declaration;
//            match_types(var->declaration, var);
//            var->type = var->declaration->type;
            var->var_type = node_var_declaration->var_type;

            // can only be array if parent is param_list or callback
        } else if (auto node_array_declaration = get_declared_array(var->name)) {
            auto node_array = make_array(var->name, 0, var->tok, var->parent);
            node_array->sizes->params.clear();
//            node_array->type = var->type;
//            node_array->declaration = node_array_declaration;
            node_array->accept(*this);
            var->replace_with(std::move(node_array));
            return;
        } else if(node_builtin_variable) {
            var->declaration = node_builtin_variable;
//            match_types(var->declaration, var);
//            var->type = var->declaration->type;
            var->is_engine = node_builtin_variable->is_engine;
        } else if(is_strict){
            CompileError(ErrorType::SyntaxError,"Variable has not been declared in this scope.", var->tok.line, "", var->name, var->tok.file).exit();
        }
    }
}

NodeVariable* ASTDefinitionChecker::get_builtin_variable(NodeVariable *var) {
    auto it = m_builtin_variables.find(var->name);
    if(it != m_builtin_variables.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeArray* ASTDefinitionChecker::get_builtin_array(NodeArray *arr) {
    auto it = m_builtin_arrays.find(arr->name);
    if(it != m_builtin_arrays.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeUIControl* ASTDefinitionChecker::get_builtin_widget(const std::string &ui_control) {
    auto it = m_builtin_widgets.find(ui_control);
    if(it != m_builtin_widgets.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTDefinitionChecker::get_builtin_function(const std::string &function, int params) {
    auto it = m_builtin_functions.find({function, params});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeVariable *ASTDefinitionChecker::get_declared_variable(const std::string& var) {
    for (auto rit = m_declared_variables.rbegin(); rit != m_declared_variables.rend(); ++rit) {
        auto it = rit->find(var);
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }

}

NodeArray *ASTDefinitionChecker::get_declared_array(const std::string& arr) {
    for (auto rit = m_declared_arrays.rbegin(); rit != m_declared_arrays.rend(); ++rit) {
        auto it = rit->find(arr);
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }
}

NodeArray *ASTDefinitionChecker::get_raw_declared_array(const std::string& var) {
    std::string var_without_identifier = var;
    if (var[0] == '_' && var[1] != '_') {
        var_without_identifier = var_without_identifier.erase(0,1);
    } else if (var.ends_with(".raw")) {
        var_without_identifier = var_without_identifier.replace(var_without_identifier.size()-4, 4, "");
    }
    return get_declared_array(var_without_identifier);
}

NodeUIControl *ASTDefinitionChecker::get_declared_control(NodeUIControl *ctr) {
    for (auto rit = m_declared_controls.rbegin(); rit != m_declared_controls.rend(); ++rit) {
        auto it = rit->find(ctr->get_string());
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }
}

std::unique_ptr<NodeAST> ASTDefinitionChecker::calculate_index_expression(
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
