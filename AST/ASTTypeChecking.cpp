//
// Created by Mathias Vatter on 02.12.23.
//

#include "ASTTypeChecking.h"


void ASTTypeChecking::visit(NodeProgram& node) {
    m_init_callback = node.callbacks[0].get();
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }

    // add return variables to beginning of init callback
    auto node_return_vars = declare_return_vars();
    m_init_callback->statements->statements.insert(m_init_callback->statements->statements.begin(),
                                                   std::make_move_iterator(node_return_vars->statements.begin()),
                                                   std::make_move_iterator(node_return_vars->statements.end()));
}

std::unique_ptr<NodeStatementList> ASTTypeChecking::declare_return_vars() {
    Token tok = Token(KEYWORD, "compiler_variable", 0,0, "");
    auto node_statement_list = std::make_unique<NodeStatementList>(tok);
    for(auto &arr_name : m_return_arrays) {
        auto node_array = make_array(arr_name.second, m_current_return_idx+1, tok, nullptr);
        node_array -> type = arr_name.first;
        node_array-> is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, tok);
        node_arr_declaration->to_be_declared->parent = node_arr_declaration.get();
        node_arr_declaration->accept(*this);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_arr_declaration), node_statement_list.get()));
    }
    return std::move(node_statement_list);
}


void ASTTypeChecking::visit(NodeCallback &node) {
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
    m_current_return_idx += m_max_returns_in_current_callback+1;
    m_max_returns_in_current_callback = 0;
}

void ASTTypeChecking::visit(NodeUIControl& node) {
    node.control_var->accept(*this);
    // unused variable declarations being replaced with node_dead_end, replace also parent
    if(cast_node<NodeDeadEnd>(node.control_var.get())) {
//    if(&node == m_current_node_replaced) {
        m_current_node_replaced = node.parent;
        node.replace_with(std::make_unique<NodeDeadEnd>(node.tok));
        return;
    }
    node.params->accept(*this);
}

void ASTTypeChecking::visit(NodeVariable& node) {
    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(node.parent);
    if(node_declaration || node_ui_control) {
        if(!node.is_used) {
            m_current_node_replaced = node.parent;
            node.replace_with(std::make_unique<NodeDeadEnd>(node.tok));
            return;
        }
    }
    // only print error if it is in a declaration
    if(node.type == Unknown) {
        if(node_declaration or node_ui_control) {
//            CompileError(ErrorType::TypeError,"Could not infer variable type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
            CompileError(ErrorType::TypeError,"Could not infer variable type. Automatically casted as <Integer>. Consider using a variable identifier.", "", node.tok).print();
			node.type = Integer;
		} else {
			node.type = node.declaration->type;
		}
    }
	if(node.type == Unknown or node.type == Number or node.type == Any) {
        // no return_var information printed pls
        if(!node.is_compiler_return and !node.is_local and !node.is_engine)
		    CompileError(ErrorType::TypeError,"Could not infer variable type. Variable is Unknown/Number/Any", node.tok.line, "valid type", node.name, node.tok.file).print();
	}

    if(node.is_compiler_return) {
        if(node.type == Unknown) node.type = Integer;
        auto return_var_name = m_return_arrays.find(node.type)->second;
        auto node_return_var = make_array(return_var_name, 0, node.tok, node.parent);
        auto callback_index = extract_last_number(node.name, &node);
        m_max_returns_in_current_callback = std::max(callback_index, m_max_returns_in_current_callback);
        node_return_var->indexes->params.push_back(make_int(m_current_return_idx+callback_index, node_return_var.get()));
        node_return_var->sizes->params.clear();
        node_return_var->type = node.type;
        node.replace_with(std::move(node_return_var));
    }
    if(node.is_local) {
        if(node.type == Unknown) node.type = Integer;
        auto local_var_name = m_local_var_arrays.find(node.type)->second;
        auto node_local_variable = make_array(local_var_name, 0, node.tok, node.parent);
        auto idx = extract_last_number(node.name, &node);
        node_local_variable->indexes->params.push_back(make_int(idx, node_local_variable.get()));
        node_local_variable->sizes->params.clear();
        node_local_variable->type = node.type;
        node.replace_with(std::move(node_local_variable));

    }
}

int ASTTypeChecking::extract_last_number(const std::string& str, NodeAST* var) {
    int lastNumberEnd = str.size();
    int lastNumberStart = -1;

    // Durchlaufen des Strings von hinten
    for (int i = str.size() - 1; i >= 0; --i) {
        if (str[i] == '_') {
            if (lastNumberStart == -1) {
                lastNumberStart = i + 1;
                break;
            }
        }
    }

    auto str_last = str.substr(lastNumberStart, lastNumberEnd - lastNumberStart);
    int last = std::stoi(str_last);

    return last;
}

void ASTTypeChecking::visit(NodeArray& node) {
    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(node.parent);

    if(node_declaration || node_ui_control) {
        if(!node.is_used) {
            m_current_node_replaced = node.parent;
            node.replace_with(std::make_unique<NodeDeadEnd>(node.tok));
            return;
        }
    }

    // only print error if it is in a declaration
    if(node.type == Unknown) {
//        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
//        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
//            CompileError(ErrorType::TypeError,"Could not infer array type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
            CompileError(ErrorType::TypeError,"Could not infer array type. Automatically casted as <Integer>. Consider using a variable identifier.", "", node.tok).print();
			node.type = Integer;
        } else {
			node.type = node.declaration->type;
		}
    }
	if(node.type == Unknown or node.type == Number or node.type == Any) {
		CompileError(ErrorType::TypeError,"Could not infer array type. Variable is Unknown/Number/Any", node.tok.line, "valid type", node.name, node.tok.file).print();
	}

    node.indexes->accept(*this);
    node.sizes->accept(*this);
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);

    // unused variable declarations being replaced with node_dead_end
    if(cast_node<NodeDeadEnd>(node.to_be_declared.get())) {
//    if(&node == m_current_node_replaced) {
        m_current_node_replaced = nullptr;
        node.replace_with(std::make_unique<NodeDeadEnd>(node.tok));
        return;
    }

    if(node.assignee) {
        node.assignee->accept(*this);

        auto node_int = cast_node<NodeInt>(node.assignee.get());
        auto node_real = cast_node<NodeReal>(node.assignee.get());

//        auto node_variable = cast_node<NodeVariable>(node.assignee.get());
//        auto node_array = cast_node<NodeArray>(node.assignee.get());
//        auto node_function = cast_node<NodeFunctionCall>(node.assignee.get());
        auto node_declare_variable = cast_node<NodeVariable>(node.to_be_declared.get());
        auto node_declare_array = cast_node<NodeArray>(node.to_be_declared.get());

        // replace variable declaration with declaration and assignment when assignment is not single int or single real or variable ist not const
        // replace instead when assignment is of type string, NodeBinaryExpr, NodeFunctionCall, etc.
        if(node_declare_variable and not(node_int or node_real or node_declare_variable->var_type == Const)) {
            auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(), std::move(node.assignee), node.tok);
            node_statement_list->statements.push_back(statement_wrapper(node.clone(),node_statement_list.get()));
            node_statement_list->statements.push_back(statement_wrapper(std::move(node_assignment), node_statement_list.get()));
            node_statement_list->update_parents(node.parent);
            node_statement_list->accept(*this);
            node.replace_with(std::move(node_statement_list));
            return;
            // add assignment to declaration if variable is declared and assigned to a string
//        } else if (node_declare_variable and node_declare_variable->var_type != Const and (node.assignee->type == String || node.assignee->type == Unknown)) {
//            auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
//            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(),std::move(node.assignee), node.tok);
//            auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node.to_be_declared),nullptr, node.tok);
//            node_statement_list->statements.push_back(statement_wrapper(std::move(node_declaration), node_statement_list.get()));
//            node_statement_list->statements.push_back(statement_wrapper(std::move(node_assignment), node_statement_list.get()));
//            node_statement_list->update_parents(node.parent);
//            node_statement_list->accept(*this);
//            node.replace_with(std::move(node_statement_list));
//            return;
            // initialize string array in a special way
        } else if (node_declare_array and (node.assignee->type == String || node.assignee->type == Unknown)) {
            auto node_param_list = cast_node<NodeParamList>(node.assignee.get());
            if (node_param_list) {
                auto node_declare_statement = std::unique_ptr<NodeSingleDeclareStatement>(static_cast<NodeSingleDeclareStatement *>(node.clone().release()));
                auto node_statement_list = array_initialization(node_declare_array, node_param_list);
                // remove list assignment from declare_statement
                node_declare_statement->assignee.release();
                node_statement_list->statements.insert(node_statement_list->statements.begin(),statement_wrapper(std::move(node_declare_statement),node_statement_list.get()));
                node_statement_list->update_parents(node.parent);
                node_statement_list->accept(*this);
                node.replace_with(std::move(node_statement_list));
                return;
            }
        }
    }

}

void ASTTypeChecking::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_statement_list(&node));
}
