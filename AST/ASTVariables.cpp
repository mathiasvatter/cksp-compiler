//
// Created by Mathias Vatter on 08.12.23.
//

#include "ASTVariables.h"

ASTVariables::ASTVariables(const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &m_builtin_variables,
                           const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
                           const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &m_builtin_arrays,
                           const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets)
        : m_builtin_functions(m_builtin_functions), m_builtin_variables(m_builtin_variables),
		m_builtin_arrays(m_builtin_arrays), m_builtin_widgets(m_builtin_widgets) {}


void ASTVariables::visit(NodeProgram& node) {

    for(auto & callback : node.callbacks) {
//		std::cout << callback->begin_callback;
//		if(callback->callback_id) std::cout <<"("<< callback->callback_id->get_string() << ")";
//		std::cout << std::endl;
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTVariables::visit(NodeCallback& node) {
    if(node.callback_id) {
		node.callback_id->accept(*this);
	}
    node.statements->accept(*this);
}

void ASTVariables::visit(NodeUIControl& node) {
	auto engine_widget = get_builtin_widget(node.ui_control_type);
	if(!engine_widget) {
		CompileError(ErrorType::SyntaxError, "Did not recognize engine widget.", node.tok.line, "valid widget type", node.ui_control_type, node.tok.file).exit();
	}

	node.control_var->accept(*this);
	node.params->accept(*this);

	//check variable type
	auto node_variable = cast_node<NodeVariable>(node.control_var.get());
	auto node_array = cast_node<NodeArray>(node.control_var.get());
	if(node_array and !is_instance_of<NodeArray>(engine_widget->control_var.get())) {
		CompileError(ErrorType::SyntaxError, "Engine Widget Variable needs to be of type <Variable>", node.tok.line, "<Variable>", "<Array>", node.tok.file).exit();
	}
	if(node_variable and !is_instance_of<NodeVariable>(engine_widget->control_var.get())) {
		CompileError(ErrorType::SyntaxError, "Engine Widget Variable needs to be of type <Array>", node.tok.line, "<Array>", "<Variable>", node.tok.file).exit();
	}

	//check param size
	if(engine_widget->params->params.size() != node.params->params.size()) {
		CompileError(ErrorType::SyntaxError, "Got incorrect size of Engine Widget parameters.", node.tok.line, std::to_string(engine_widget->params->params.size()), std::to_string(node.params->params.size()), node.tok.file).exit();
	}

}

void ASTVariables::visit(NodeArray& node) {
	auto node_builtin_array = get_builtin_array(&node);
	auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
	auto node_ui_control = cast_node<NodeUIControl>(node.parent);
	if(node_builtin_array && (node_declare_statement || node_ui_control) ){
		CompileError(ErrorType::SyntaxError,"Array shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).exit();
	}
    if(node_declare_statement and node_declare_statement->to_be_declared.get() == &node) {
        if(get_declared_array(node.name)) {
            CompileError(ErrorType::SyntaxError,"Array has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
//			exit(EXIT_FAILURE);
        } else {
            m_declared_arrays[node.name] = &node;
        }
	} else if (node_ui_control) {
		if(get_declared_control(node_ui_control)) {
			CompileError(ErrorType::SyntaxError,"Control Widget Array has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
			exit(EXIT_FAILURE);
		} else {
			m_declared_controls[node_ui_control->get_string()] = node_ui_control;
			m_declared_arrays[node.name] = &node;
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
            if(node_declaration->var_type == List or node_declaration->var_type == UI_Control) node.var_type = node_declaration->var_type;
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

        } else if(node_builtin_array) {
            node.declaration = node_builtin_array;
        } else if (node_declaration and has_compiler_identifier) {
            node.declaration = node_declaration;
            node.dimensions = 1;
            node.var_type = Array;
            node.name = "_"+node.name;
        } else {
            CompileError(ErrorType::SyntaxError,"Array has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
//            exit(EXIT_FAILURE);
        }
    }
    node.sizes->accept(*this);
    node.indexes->accept(*this);
}

void ASTVariables::visit(NodeVariable& node) {
    auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
	auto node_builtin_variable = get_builtin_variable(&node);
	auto node_ui_control = cast_node<NodeUIControl>(node.parent);
	if(node_builtin_variable && (node_declare_statement || node_ui_control) ){
		CompileError(ErrorType::SyntaxError,"Variable shadows builtin variable. Try renaming the variable.", node.tok.line, "", node.name, node.tok.file).exit();
	}

    if(node_declare_statement and node_declare_statement->to_be_declared.get() == &node) {
        if(get_declared_variable(node.name)) {
            CompileError(ErrorType::SyntaxError,"Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
//            exit(EXIT_FAILURE);
        } else {
            m_declared_variables.insert({node.name, &node});
			m_variables_declared++;
        }
    } else if (node_ui_control) {
        if(get_declared_control(node_ui_control)) {
            CompileError(ErrorType::SyntaxError,"Control Variable has already been declared.", node.tok.line, "", node.name, node.tok.file).print();
            exit(EXIT_FAILURE);
        } else {
			m_declared_controls[node_ui_control->get_string()] = node_ui_control;
			m_declared_variables.insert({node.name, &node});
			m_variables_declared++;
        }
    } else {
		// no removing of raw array identifier for the variable search
        auto node_first_declaration = get_declared_variable(node.name);
		// in case the user wants the raw array
		bool has_compiler_identifier = node.name[0] == '_';
		if (has_compiler_identifier) node.name = node.name.erase(0,1);
        // sometimes a variable can also be an array if notated without brackets -> replace with array node
        auto node_first_array_declaration = get_declared_array(node.name);
		if (has_compiler_identifier) node.name = "_"+node.name;
        if(node_first_declaration || node_first_array_declaration) {
            if(node_first_declaration) {
				if(node_first_declaration->var_type == UI_Control) node.var_type = node_first_declaration->var_type;
                node.declaration = node_first_declaration;
                node_first_declaration->is_used = true;
            }
            if(node_first_array_declaration) {
				auto node_array = make_array(node.name, 0, node.tok, node.parent);
                node_array->sizes->params.clear();
                node_array->type = node.type;
                node_array->declaration = node_first_array_declaration;
                node_array->accept(*this);
                node.replace_with(std::move(node_array));
            }
        } else if(node_builtin_variable) {
            node.declaration = node_builtin_variable;
        } else {
            CompileError(ErrorType::SyntaxError,"Variable has not been declared.", node.tok.line, "", node.name, node.tok.file).print();
//			exit(EXIT_FAILURE);
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
            for (auto& stmt : inner_statements) {
                stmt->parent = &node;
            }
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
//    node.update_parents(&node);
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
    auto it = m_builtin_variables.find(var->name);
    if(it != m_builtin_variables.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeArray* ASTVariables::get_builtin_array(NodeArray *arr) {
    auto it = m_builtin_arrays.find(arr->name);
    if(it != m_builtin_arrays.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeUIControl* ASTVariables::get_builtin_widget(const std::string &ui_control) {
	auto it = m_builtin_widgets.find(ui_control);
	if(it != m_builtin_widgets.end()) {
        return it->second.get();
	}
	return nullptr;
}

NodeVariable *ASTVariables::get_declared_variable(const std::string& var) {
    auto it = m_declared_variables.find(var);
    if (it != m_declared_variables.end()) {
        return it->second;
    }
    return nullptr;
}

NodeArray *ASTVariables::get_declared_array(const std::string& arr) {
	auto it = m_declared_arrays.find(arr);
	if (it != m_declared_arrays.end()) {
		return it->second;
	}
	return nullptr;
}

NodeUIControl *ASTVariables::get_declared_control(NodeUIControl *ctr) {
	auto it = m_declared_controls.find(ctr->get_string());
	if (it != m_declared_controls.end()) {
		return it->second;
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
