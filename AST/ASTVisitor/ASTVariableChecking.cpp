//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTVariableChecking.h"

ASTVariableChecking::ASTVariableChecking(DefinitionProvider* definition_provider, std::unordered_map<NodeAST *, std::unique_ptr<NodeStatement>> m_function_inlines)
	: m_def_provider(definition_provider), m_function_inlines(std::move(m_function_inlines)) {}


void ASTVariableChecking::visit(NodeProgram& node) {
	m_init_callback = node.callbacks[0].get();
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

void ASTVariableChecking::visit(NodeStatement& node) {
	if(!node.function_inlines.empty()) {
		auto node_body = std::make_unique<NodeBody>(node.function_inlines[0]->tok);
		node_body->parent = &node;
		for(auto & func : node.function_inlines) {
			auto it = m_function_inlines.find(func);
			node_body->statements.push_back(std::move(it->second));
		}
		node_body->statements.push_back(statement_wrapper(std::move(node.statement), &node));
		node_body->accept(*this);
		node.statement = std::move(node_body);
	} else {
		node.statement->accept(*this);
	}
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
	if(node.scope) m_def_provider->add_scope();
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	if(node.scope) m_def_provider->remove_scope();
}

void ASTVariableChecking::visit(NodeArray& node) {
	node.sizes->accept(*this);
	node.indexes->accept(*this);

	auto compile_error = CompileError(ErrorType::Variable, "","", node.tok);
	auto node_declaration = m_def_provider->get_declaration(&node, m_is_init_callback);
	if(!node_declaration and node.is_reference) {
		compile_error.m_message = "<Array> has not been declared: " + node.name;
		compile_error.m_expected = "Valid declaration";
		compile_error.m_got = node.name;
		compile_error.exit();
	}

	if(node.is_reference)
		m_def_provider->match_data_structure(&node, node_declaration);

	// match array specific information
	if(node.is_reference and node.sizes->params.empty()) {
		if(auto node_array = cast_node<NodeArray>(node_declaration)) {
			node.dimensions = node_array->dimensions;
			node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
			node.sizes->update_parents(&node);
		}
	}
}

void ASTVariableChecking::visit(NodeVariable& node) {
	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return or node.is_local) {
		node.is_used = true;
		return;
	}

	auto compile_error = CompileError(ErrorType::Variable, "","", node.tok);
	auto node_declaration = m_def_provider->get_declaration(&node, m_is_init_callback);
	if(!node_declaration and node.is_reference) {
		compile_error.m_message = "<Variable> has not been declared: " + node.name;
		compile_error.m_expected = "Valid declaration";
		compile_error.m_got = node.name;
		compile_error.exit();
	}

	if(node.is_reference)
		m_def_provider->match_data_structure(&node, node_declaration);

	// replace variable with array if incorrectly recognized by parser
	if(node.is_reference and node_declaration->get_node_type() == NodeType::Array) {
		auto node_array = make_array(node.name, 0, node.tok, node.parent);
		node_array->sizes->params.clear();
		node_array->show_brackets = false;
		node_array->accept(*this);
		node.replace_with(std::move(node_array));
		return;
	}
}

void ASTVariableChecking::visit(NodeFunctionCall &node) {
	node.function->accept(*this);
	// replace node variable when in get_ui_id and not ui_control
	if(node.function->name == "get_ui_id" and !node.function->args->params.empty()) {
		if(auto node_variable = cast_node<NodeVariable>(node.function->args->params[0].get())) {
			if(node_variable->data_type != DataType::UI_Control) {
				node.replace_with(std::move(node.function->args->params[0]));
			}
		}
	}
}

//void ASTVariableChecking::visit(NodeParamList &node) {
//    for(const auto & param : node.params) {
//        param->accept(*this);
//        node.type = param->type;
//    }
//}

std::unique_ptr<NodeFunctionCall> ASTVariableChecking::wrap_in_get_ui_id(std::unique_ptr<NodeAST> variable) {
	auto parent_tok = variable->parent->tok;
	auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(m_def_provider->get_builtin_function("get_ui_id", 1)->clone().release()));
	node_get_ui_id->args->params.clear();
	node_get_ui_id->args->params.push_back(std::move(variable));
	return std::make_unique<NodeFunctionCall>(false, std::move(node_get_ui_id), parent_tok);
}