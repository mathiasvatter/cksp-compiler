//
// Created by Mathias Vatter on 08.12.23.
//

#include "ASTVariables.h"

ASTVariables::ASTVariables(const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions)
        : m_builtin_functions(m_builtin_functions) {}


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
    node.sizes->accept(*this);
    node.indexes->accept(*this);
}

void ASTVariables::visit(NodeVariable& node) {
}

void ASTVariables::visit(NodeFunctionCall &node) {
}

void ASTVariables::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);
    if(node.assignee)
        node.assignee -> accept(*this);
    // in case node.assignee is function substitution -> then this node gets replaced
    if(!node.to_be_declared and !node.assignee) return;

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    // check if is_persistent
    bool is_persistent = false;
    auto node_array = cast_node<NodeArray>(node.to_be_declared.get());
    if(node_array) is_persistent = node_array->is_persistent;
    auto node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    if(node_variable) is_persistent = node_variable->is_persistent;

    NodeParamList* param_list = nullptr;
    if(node.assignee) param_list = cast_node<NodeParamList>(node.assignee.get());

    // make param list if it is array declaration
    if(node_array) {
        if(node.assignee and !param_list) {
            auto node_param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.assignee->tok));
            node_param_list->params.push_back(std::move(node.assignee));
            node_param_list->parent = &node;
            node.assignee = std::move(node_param_list);
        }
        // array size is not empty
        if(!node_array->sizes->params.empty()) {
            node_array->dimensions = node_array->sizes->params.size();
            // multidimensional array
            if (node_array->dimensions > 1) {
                auto node_expression = create_right_nested_binary_expr(node_array->sizes->params, 0, "*", node_array->tok);
                node_expression->parent = node_array->sizes.get();
                for(int i = 0; i<node_array->sizes->params.size(); i++) {
                    auto node_var = std::make_unique<NodeVariable>(false, node_array->name+".SIZE_D"+std::to_string(i+1), Const, node.tok);
                    auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_var), node_array->sizes->params[i]->clone(), node.tok);
                    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), node.tok);
                    node_statement_list->statements.push_back(std::move(node_statement));
                }
                node_array->indexes->params.clear();
                node_array->indexes->params.push_back(std::move(node_expression));
            }
        } else {
            if(!node.assignee) {
                CompileError(ErrorType::SyntaxError,"Unable to infer array size.", node.tok.line, "initializer list", "",node.tok.file).exit();
            }
            // if size is empty -> get it from declaration
            if(param_list) {
                auto node_int = make_int((int32_t) param_list->params.size(), node_array->sizes.get());
                node_array->sizes->params.push_back(std::move(node_int));
            }
        }
    } else if(node_variable) {
        // a list of values is assigned to a declared variable
        if(param_list and param_list->params.size() > 1) {
            CompileError(ErrorType::SyntaxError,"Unable to assign a list of values to variable.", node.tok.line, "single value", "list of values",node.tok.file).exit();
        }
    }

    node_statement_list->statements.push_back(statement_wrapper(node.clone(), node.parent));
    // add make_persistent and read_persistent_var
    if(is_persistent) {
        add_vector_to_statement_list(node_statement_list, add_read_functions(node.to_be_declared.get(), node_statement_list.get()));
    }
    node_statement_list->update_parents(node.parent);

    node.replace_with(std::move(node_statement_list));

}

std::unique_ptr<NodeAST> ASTVariables::create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok) {
    // Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
    if (index >= nodes.size() - 1) {
        return nodes[index]->clone();
    }
    // Erstelle die rechte Seite der Expression rekursiv.
    auto right = create_right_nested_binary_expr(nodes, index + 1, op, tok);
    // Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
    return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), tok);
}

void ASTVariables::visit(NodeSingleAssignStatement &node) {
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

std::vector<std::unique_ptr<NodeStatement>> ASTVariables::add_read_functions(NodeAST* var, NodeAST* parent) {
    std::vector<std::unique_ptr<NodeStatement>> statements;

    std::string persistent1 = "make_persistent";
    std::string persistent2 = "read_persistent_var";
    auto node_function = get_builtin_function(persistent1)->clone();
    auto make_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    make_persistent->args->params.clear();
    make_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(make_persistent), parent));

    node_function = get_builtin_function(persistent2)->clone();
    auto read_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    read_persistent->args->params.clear();
    read_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(read_persistent), parent));
    return std::move(statements);
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
