//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTVariableChecking.h"

ASTVariableChecking::ASTVariableChecking(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}

void ASTVariableChecking::visit(NodeProgram& node) {
	m_init_callback = node.callbacks[0].get();
    for(auto & def : node.function_definitions) {
        m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
    }
	// erase all previously saved scopes
	m_def_provider->refresh_scopes();
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(auto & function_definition : node.function_definitions) {
		function_definition->accept(*this);
	}
}

void ASTVariableChecking::visit(NodeCallback& node) {
	if(&node == m_init_callback) m_is_init_callback = true;

	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);

	m_is_init_callback = false;
}

void ASTVariableChecking::visit(NodeUIControl& node) {
	auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
	auto engine_widget = m_def_provider->get_builtin_widget(node.ui_control_type);
	if(!engine_widget) {
		error.m_message = "Unknown Engine Widget";
		error.m_got = node.ui_control_type;
		error.exit();
	}
	node.declaration = engine_widget;

	node.control_var->accept(*this);
	node.params->accept(*this);

	auto node_type = node.control_var->get_node_type();
	auto engine_widget_type = engine_widget->control_var->get_node_type();
	//check variable type
	if(node_type != engine_widget_type) {
		error.m_message = "Engine Widget Variable is of wrong type.";
		if(node_type == NodeType::Array) {
			error.m_got = "<Array>";
			error.m_expected = "<Variable>";
		} else if (node_type == NodeType::Variable) {
			error.m_got = "<Variable>";
			error.m_expected = "<Array>";
		}
		error.exit();
	}

	//check param size
	if(engine_widget->params->params.size() != node.params->params.size()) {
		error.m_message = "Engine Widget has incorrect number of parameters.";
		error.m_expected = std::to_string(engine_widget->params->params.size());
		error.m_got = std::to_string(node.params->params.size());
		error.exit();
	}
}

void ASTVariableChecking::visit(NodeBody &node) {
    m_current_body = &node;
	if(node.scope) m_def_provider->add_scope();
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	if(node.scope) m_def_provider->remove_scope();
}

void ASTVariableChecking::visit(NodeSingleDeclareStatement& node) {
    if(m_current_body->scope and m_current_body->parent != m_init_callback and !node.to_be_declared->is_global) {
        node.to_be_declared->is_local = true;
    }

    node.to_be_declared->accept(*this);
    if(node.assignee) node.assignee->accept(*this);
}

void ASTVariableChecking::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) throw_declaration_error(&node).exit();

    node.match_data_structure(node_declaration);
}

void ASTVariableChecking::visit(NodeArray& node) {
	if(node.size) node.size->accept(*this);

    m_def_provider->set_declaration(&node, m_is_init_callback || !node.is_local);
}

void ASTVariableChecking::visit(NodeVariableRef& node) {
	// handle return_vars -> do not check if they have been declared
//	if(node.is_compiler_return or node.is_local) {
//		return;
//	}

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration)
        throw_declaration_error(&node).exit();

    node.match_data_structure(node_declaration);

	// replace variable with array if incorrectly recognized by parser
	if(node_declaration->get_node_type() == NodeType::Array) {
		auto node_array = std::make_unique<NodeArrayRef>(node.name, nullptr, node.tok);
		node_array->accept(*this);
		node.replace_with(std::move(node_array));
		return;
	}
}

void ASTVariableChecking::visit(NodeVariable& node) {
    if(node.name == "btn_chord_page_enter") {

    }
    // handle return_vars -> do not check if they have been declared
//    if(node.is_compiler_return or node.is_local) {
//        node.is_used = true;
//        return;
//    }
    m_def_provider->set_declaration(&node, m_is_init_callback || !node.is_local);
}

void ASTVariableChecking::visit(NodeFunctionCall &node) {
	node.function->accept(*this);

//    if(!node.definition and !node.function->is_builtin) {
//        if (auto function_def = get_function_definition(node.function.get())) {
//            node.definition = function_def;
//		    node.definition->accept(*this);
//        } else if (auto builtin_func = m_def_provider->get_builtin_function(node.function.get())) {
//            node.function->type = builtin_func->type;
//            node.function->has_forced_parenth = builtin_func->has_forced_parenth;
//            node.function->arg_ast_types = builtin_func->arg_ast_types;
//            node.function->arg_var_types = builtin_func->arg_var_types;
//            node.function->is_builtin = builtin_func->is_builtin;
//
//        } else {
//            CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
//        }
//    }

    // replace node variable when in get_ui_id and not ui_control
    if(node.function->is_builtin and !node.function->args->params.empty() and node.function->name == "get_ui_id") {
        if(auto node_variable = cast_node<NodeVariableRef>(node.function->args->params[0].get())) {
            if(node_variable->data_type != DataType::UI_Control) {
                node.replace_with(std::move(node.function->args->params[0]));
            }
        }
    }
}

void ASTVariableChecking::visit(NodeFunctionDefinition &node) {
    m_def_provider->add_scope();
    node.header ->accept(*this);
    if (node.return_variable.has_value())
        node.return_variable.value()->accept(*this);
    node.body->accept(*this);
    m_def_provider->remove_scope();
}


