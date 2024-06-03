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
    m_init_callback->statements->prepend_body(std::move(node_return_vars));
}

std::unique_ptr<NodeBody> ASTTypeChecking::declare_return_vars() {
    Token tok = Token(token::KEYWORD, "compiler_variable", 0,0, "");
    auto node_body = std::make_unique<NodeBody>(tok);
    for(auto &arr_name : m_return_arrays) {
        auto node_array = std::make_unique<NodeArray>(
                std::nullopt,
                arr_name.second,
                arr_name.first,
                std::make_unique<NodeInt>(m_current_return_idx+1, tok), tok
                );
        node_array-> is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclareStatement>(
                std::move(node_array),
                nullptr, tok);
        node_arr_declaration->accept(*this);
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_arr_declaration), tok));
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
    node.params->accept(*this);
}

void ASTTypeChecking::visit(NodeVariableRef& node) {
    // only print error if it is no reference

    if(node.type == ASTType::Unknown) {
        node.type = node.declaration->type;
    }


    // replace variable ref with array ref if it is a compiler return
    if(node.is_compiler_return) {
        if(node.ty == TypeRegistry::Unknown) node.ty = TypeRegistry::Integer;
        auto return_var_name = m_return_arrays.find(node.ty)->second;
        auto callback_index = extract_last_number(node.name, &node);
        m_max_returns_in_current_callback = std::max(callback_index, m_max_returns_in_current_callback);
        auto node_return_var = std::make_unique<NodeArrayRef>(
                return_var_name,
                std::make_unique<NodeInt>(m_current_return_idx+callback_index, node.tok), node.tok
                );
        node_return_var->type = node.type;
        node.replace_with(std::move(node_return_var));
    }
    // replace local vars with array ref
//    if(node.is_local) {
//        if(node.type == ASTType::Unknown) node.type = ASTType::Integer;
//        auto local_var_name = m_local_var_arrays.find(node.type)->second;
//        auto idx = extract_last_number(node.name, &node);
//        auto node_local_variable = std::make_unique<NodeArrayRef>(
//                local_var_name,
//                std::make_unique<NodeInt>(idx, node.tok), node.tok
//                );
//        node_local_variable->type = node.type;
//        node.replace_with(std::move(node_local_variable));
//    }
}

void ASTTypeChecking::visit(NodeVariable& node) {
    auto error = CompileError(ErrorType::TypeError,"Could not infer variable type.", "valid type", node.tok);
    // only print error if it is in a declaration
    if(node.type == ASTType::Unknown) {
        error.m_message += " Automatically casted as <Integer>. Consider using a variable identifier.";
        error.m_got = type_to_string(node.type)+" Type.";
        error.print();
        node.type = ASTType::Integer;
    }
    if(node.type == ASTType::Unknown or node.type == ASTType::Number or node.type == ASTType::Any) {
        // no return_var information printed pls
        if(!node.is_compiler_return and !node.is_engine) {
            error.m_message += " Variable is Unknown/Number/Any";
            error.m_got = type_to_string(node.type);
            error.print();
        }
    }

    if(node.is_compiler_return) {
        error.m_message = "Compiler return variable should be a variable reference. Found declared variable.";
        error.m_expected = "";
        error.exit();
    }

//    if(node.is_local) {
//        error.m_message = "Local variable should be a variable reference. Found declared variable.";
//        error.m_expected = "";
//        error.exit();
//    }
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

void ASTTypeChecking::visit(NodeArrayRef& node) {
	auto error = CompileError(ErrorType::SyntaxError, "Could not infer Array type.", "", node.tok);
    // only print error if it is in a declaration
    if(node.type == ASTType::Unknown) {
        node.type = node.declaration->type;
    }
	if(node.type == ASTType::Unknown or node.type == ASTType::Number or node.type == ASTType::Any) {
		error.m_message += " Array "+node.name+" is "+type_to_string(node.type)+".";
		error.m_got = type_to_string(node.type)+" Type.";
		error.print();
	}

    if(node.index) node.index->accept(*this);
}

void ASTTypeChecking::visit(NodeArray& node) {
    auto error = CompileError(ErrorType::SyntaxError, "Could not infer Array type.", "", node.tok);
    // only print error if it is in a declaration
    if(node.type == ASTType::Unknown) {
        error.m_message += " Automatically casted "+node.name+" as <Integer>. Consider using a variable identifier.";
        error.m_got = type_to_string(node.type)+" Type.";
        error.print();
        node.type = ASTType::Integer;
    }
    if(node.type == ASTType::Unknown or node.type == ASTType::Number or node.type == ASTType::Any) {
        error.m_message += " Array "+node.name+" is "+type_to_string(node.type)+".";
        error.m_got = type_to_string(node.type)+" Type.";
        error.print();
    }

    if(node.size) node.size->accept(*this);
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);

    if(node.assignee) {
        node.assignee->accept(*this);

        auto node_int = node.assignee->get_node_type() == NodeType::Int;
        auto node_real = node.assignee->get_node_type() == NodeType::Real;

        auto node_declare_variable = node.to_be_declared->get_node_type() == NodeType::Variable;
        auto node_declare_array = node.to_be_declared->get_node_type() == NodeType::Array;

        // replace variable declaration with declaration and assignment when assignment is not single int or single real or variable is not const
        // replace instead when assignment is of type string, NodeBinaryExpr, NodeFunctionCall, etc.
        if(node_declare_variable and not(node_int or node_real or node.to_be_declared->data_type == DataType::Const)) {
            auto node_body = std::make_unique<NodeBody>(node.tok);
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(
                    node.to_be_declared->to_reference(),
                    std::move(node.assignee), node.tok
                    );
            node_body->statements.push_back(std::make_unique<NodeStatement>(node.clone(), node.tok));
            node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assignment), node.tok));
            node_body->set_child_parents();
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
					auto node_var = cast_node<NodeVariableRef>(param.get());
					auto node_arr = cast_node<NodeArrayRef>(param.get());
					if ((node_var and node_var->declaration->data_type != DataType::Const and !node_var->is_engine) or node_arr) {
						has_var = true;
						break;
					}
				}
			}
            if (node_param_list and (has_var || node.assignee->type == ASTType::String || node.assignee->type == ASTType::Unknown)) {
                auto node_declare_statement = clone_as<NodeSingleDeclareStatement>(&node);
                auto node_body = array_initialization(static_cast<NodeArray*>(node.to_be_declared.get()), node_param_list);
                // remove list assignment from declare_statement
                node_declare_statement->assignee = nullptr;
                auto node_declare_statement_body = std::make_unique<NodeBody>(node.tok);
                node_declare_statement_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_declare_statement), node.tok));
                node_body->prepend_body(std::move(node_declare_statement_body));
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
   node.cleanup_body();
}
