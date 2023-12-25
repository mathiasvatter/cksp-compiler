//
// Created by Mathias Vatter on 02.12.23.
//

#include "ASTTypeChecking.h"


void ASTTypeChecking::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTTypeChecking::visit(NodeCallback &node) {
    m_current_return_idx += m_max_returns_in_current_callback+1;
    m_max_returns_in_current_callback = 0;
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
}

void ASTTypeChecking::visit(NodeUIControl& node) {
    node.control_var->accept(*this);
    // unused variable declarations being replaced with node_dead_end, replace also parent
    if(&node == m_current_node_replaced) {
        m_current_node_replaced = node.parent;
        node.parent->replace_with(std::make_unique<NodeDeadEnd>(node.tok));
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
            node.parent->replace_with(std::make_unique<NodeDeadEnd>(node.tok));
            return;
        }
    }

    // only print error if it is in a declaration
    if(node.type == Unknown) {
        if(node_declaration or node_ui_control) {
            CompileError(ErrorType::TypeError,"Could not infer variable type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
			node.type = Integer;
		} else {
			node.type = node.declaration->type;
		}
    }

	if(node.type == Unknown or node.type == Number or node.type == Any) {
        // no return_var information printed pls
        if(!node.is_compiler_return and !node.is_local)
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
            node.parent->replace_with(std::make_unique<NodeDeadEnd>(node.tok));
            return;
        }
    }

    // only print error if it is in a declaration
    if(node.type == Unknown) {
//        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
//        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
			CompileError(ErrorType::TypeError,"Could not infer array type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
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
    if(&node == m_current_node_replaced) {
        m_current_node_replaced = nullptr;
        return;
    }

    if(node.assignee)
        node.assignee->accept(*this);

}
