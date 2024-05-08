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

std::unique_ptr<NodeBody> ASTTypeChecking::declare_return_vars() {
    Token tok = Token(token::KEYWORD, "compiler_variable", 0,0, "");
    auto node_body = std::make_unique<NodeBody>(tok);
    for(auto &arr_name : m_return_arrays) {
        auto node_array = make_array(arr_name.second, m_current_return_idx+1, tok, nullptr);
        node_array -> type = arr_name.first;
        node_array-> is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
		node_array->is_reference = false;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, tok);
        node_arr_declaration->to_be_declared->parent = node_arr_declaration.get();
        node_arr_declaration->accept(*this);
        node_body->statements.push_back(statement_wrapper(std::move(node_arr_declaration), node_body.get()));
    }
    return std::move(node_body);
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
//    if(cast_node<NodeDeadCode>(node.control_var.get())) {
////    if(&node == m_current_node_replaced) {
//        m_current_node_replaced = node.parent;
//        node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//        return;
//    }
    node.params->accept(*this);
}

void ASTTypeChecking::visit(NodeVariable& node) {
    auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
    if(node_declaration and node_declaration->to_be_declared.get() != &node) node_declaration = nullptr;
    auto node_ui_control = cast_node<NodeUIControl>(node.parent);
//    if(node_declaration || node_ui_control) {
//        if(!node.is_used) {
//            m_current_node_replaced = node.parent;
//            node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//            return;
//        }
//    }
    // only print error if it is in a declaration
    if(node.type == ASTType::Unknown) {
        if(node_declaration or node_ui_control) {
            CompileError(ErrorType::TypeError,"Could not infer variable type. Automatically casted as <Integer>. Consider using a variable identifier.", "", node.tok).print();
			node.type = ASTType::Integer;
		} else {
			node.type = node.declaration->type;
		}
    }
	if(node.type == ASTType::Unknown or node.type == ASTType::Number or node.type == ASTType::Any) {
        // no return_var information printed pls
        if(!node.is_compiler_return and !node.is_local and !node.is_engine)
		    CompileError(ErrorType::TypeError,"Could not infer variable type. Variable is Unknown/Number/Any", "valid type", node.tok).print();
	}

    if(node.is_compiler_return) {
        if(node.type == ASTType::Unknown) node.type = ASTType::Integer;
        auto return_var_name = m_return_arrays.find(node.type)->second;
        auto node_return_var = make_array(return_var_name, 0, node.tok, node.parent);
        auto callback_index = extract_last_number(node.name, &node);
        m_max_returns_in_current_callback = std::max(callback_index, m_max_returns_in_current_callback);
        node_return_var->index->params.push_back(make_int(m_current_return_idx+callback_index, node_return_var.get()));
        node_return_var->size->params.clear();
        node_return_var->type = node.type;
        node.replace_with(std::move(node_return_var));
    }
    if(node.is_local) {
        if(node.type == ASTType::Unknown) node.type = ASTType::Integer;
        auto local_var_name = m_local_var_arrays.find(node.type)->second;
        auto node_local_variable = make_array(local_var_name, 0, node.tok, node.parent);
        auto idx = extract_last_number(node.name, &node);
        node_local_variable->index->params.push_back(make_int(idx, node_local_variable.get()));
        node_local_variable->size->params.clear();
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

//    if(node_declaration || node_ui_control) {
//        if(!node.is_used) {
//            m_current_node_replaced = node.parent;
//            node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//            return;
//        }
//    }
	auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
    // only print error if it is in a declaration
    if(node.type == ASTType::Unknown) {
//        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
//        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
			error.m_message = "Could not infer array type. Automatically casted as <Integer>. Consider using a variable identifier.";
			error.m_got = node.name;
			error.print();
			node.type = ASTType::Integer;
        } else {
			node.type = node.declaration->type;
		}
    }
	if(node.type == ASTType::Unknown or node.type == ASTType::Number or node.type == ASTType::Any) {
		error.m_message = "Could not infer Array type. Array is Unknown/Number/Any";
		error.m_got = node.name;
		error.print();
	}

    node.index->accept(*this);
    node.size->accept(*this);
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);

    // unused variable declarations being replaced with node_dead_end
//    if(cast_node<NodeDeadCode>(node.to_be_declared.get())) {
////    if(&node == m_current_node_replaced) {
//        m_current_node_replaced = nullptr;
//        node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//        return;
//    }

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
        if(node_declare_variable and not(node_int or node_real or node_declare_variable->data_type == DataType::Const)) {
            auto node_body = std::make_unique<NodeBody>(node.tok);
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(), std::move(node.assignee), node.tok);
            node_body->statements.push_back(statement_wrapper(node.clone(),node_body.get()));
            node_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_body.get()));
            node_body->update_parents(node.parent);
            node_body->accept(*this);
            node.replace_with(std::move(node_body));
            return;
            // add assignment to declaration if variable is declared and assigned to a string
//        } else if (node_declare_variable and node_declare_variable->data_type != Const and (node.assignee->type == String || node.assignee->type == Unknown)) {
//            auto node_body = std::make_unique<NodeBody>(node.tok);
//            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(),std::move(node.assignee), node.tok);
//            auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node.to_be_declared),nullptr, node.tok);
//            node_body->statements.push_back(statement_wrapper(std::move(node_declaration), node_body.get()));
//            node_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_body.get()));
//            node_body->update_parents(node.parent);
//            node_body->accept(*this);
//            node.replace_with(std::move(node_body));
//            return;
            // initialize string array in a special way
        } else if (node_declare_array) {
            auto node_param_list = cast_node<NodeParamList>(node.assignee.get());
			// check if one element of param list is non const variable or array -> split up initlialization
			bool has_var = false;
			if(node_param_list) {
				for (auto &param : node_param_list->params) {
					auto node_var = cast_node<NodeVariable>(param.get());
					auto node_arr = cast_node<NodeArray>(param.get());
					if ((node_var and node_var->data_type != DataType::Const and !node_var->is_engine) or node_arr) {
						has_var = true;
						break;
					}
				}
			}
            if (node_param_list and (has_var || node.assignee->type == ASTType::String || node.assignee->type == ASTType::Unknown)) {
                auto node_declare_statement = std::unique_ptr<NodeSingleDeclareStatement>(static_cast<NodeSingleDeclareStatement *>(node.clone().release()));
                auto node_body = array_initialization(node_declare_array, node_param_list);
                // remove list assignment from declare_statement
                node_declare_statement->assignee.release();
                node_body->statements.insert(node_body->statements.begin(),statement_wrapper(std::move(node_declare_statement),node_body.get()));
                node_body->update_parents(node.parent);
                node_body->accept(*this);
                node.replace_with(std::move(node_body));
                return;
            }
        }
    }

}

void ASTTypeChecking::visit(NodeBody& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_body(&node));
}
