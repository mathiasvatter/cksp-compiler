//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTVariableChecking.h"

ASTVariableChecking::ASTVariableChecking(DefinitionProvider* definition_provider, bool fail)
	: m_def_provider(definition_provider), fail(fail) {}

void ASTVariableChecking::visit(NodeProgram& node) {
	m_program = &node;
	// update function lookup map because of altered param counts after lambda lifting
    node.update_function_lookup();
	// erase all previously saved scopes
	m_def_provider->refresh_scopes();

	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
		// reset visited flag
		func_def->visited = false;
	}
}

void ASTVariableChecking::visit(NodeCallback& node) {
	m_program->current_callback = &node;

	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);

	m_program->current_callback = nullptr;
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

	// if fail is set to false, return early. the rest is determined after lowering
	if(!fail) return;

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
	if(node.parent->get_node_type() != NodeType::Statement and !is_instance_of<NodeDataStructure>(node.parent)) {
		node.scope = true;
	}

	if(node.scope) m_def_provider->add_scope();
	// if body is in function definition, copy over last scope of header variables
	if(node.parent->get_node_type() == NodeType::FunctionDefinition) {
		m_def_provider->copy_last_scope();
	}
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	if(node.scope) m_def_provider->remove_scope();
}

void ASTVariableChecking::visit(NodeFunctionDefinition &node) {
	node.visited = true;
	m_program->function_call_stack.push(&node);
	m_def_provider->add_scope();
	node.header ->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->accept(*this);
	m_def_provider->remove_scope();
	m_program->function_call_stack.pop();
}

void ASTVariableChecking::visit(NodeFunctionCall &node) {
	node.function->accept(*this);

	node.get_definition(m_program);

	if(node.kind == NodeFunctionCall::UserDefined and node.definition) {
		if(!node.definition->visited) node.definition->accept(*this);
	}

//	// replace node variable when in get_ui_id and not ui_control
//	if(node.function->is_builtin and !node.function->args->params.empty() and node.function->name == "get_ui_id") {
//		if(auto node_variable = cast_node<NodeVariableRef>(node.function->args->params[0].get())) {
//			if(node_variable->data_type != DataType::UI_Control) {
//				node.replace_with(std::move(node.function->args->params[0]));
//			}
//		}
//	}
}

void ASTVariableChecking::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->determine_locality(m_program, m_current_body);

    node.to_be_declared->accept(*this);
    if(node.assignee) node.assignee->accept(*this);
}

void ASTVariableChecking::visit(NodeArray& node) {
	node.determine_locality(m_program, m_current_body);
	if(node.size) node.size->accept(*this);
	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTVariableChecking::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	// maybe declaration comes after lowering, do not throw error
	if(!node_declaration and !fail) return;
	if(!node_declaration) m_def_provider->throw_declaration_error(&node).exit();

    node.match_data_structure(node_declaration);
}

void ASTVariableChecking::visit(NodeNDArray& node) {
	node.determine_locality(m_program, m_current_body);
	node.sizes->accept(*this);
	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTVariableChecking::visit(NodeNDArrayRef& node) {
	node.indexes->accept(*this);
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Multidimensional array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}
	node.match_data_structure(node_declaration);
}

void ASTVariableChecking::visit(NodeVariable& node) {
	node.determine_locality(m_program, m_current_body);

	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		node.is_used = true;
		return;
	}
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !node.is_local);
}

void ASTVariableChecking::visit(NodeVariableRef& node) {
	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		return;
	}

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration and !fail) return;
	if(!node_declaration) m_def_provider -> throw_declaration_error(&node).exit();

    node.match_data_structure(node_declaration);
}

void ASTVariableChecking::visit(NodeListStruct& node) {
	node.determine_locality(m_program, m_current_body);
	for(auto &params : node.body) {
		params->accept(*this);
	}
	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTVariableChecking::visit(NodeListStructRef& node) {
	node.indexes->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration and !fail) return;
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "List has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}
}

NodeDataStructure* ASTVariableChecking::apply_type_annotations(NodeDataStructure* node) {
	if(node->ty == TypeRegistry::Unknown) return node;
	if(node->ty->get_type_kind() == TypeKind::Composite and node->get_node_type() == NodeType::Variable) {
		auto node_var = static_cast<NodeVariable*>(node);
		auto comp_type = static_cast<CompositeType*>(node->ty);
		if(comp_type->get_compound_type() == CompoundKind::Array) {
			auto node_array = static_cast<NodeVariable*>(node)->to_array();
			node_array->is_local = node->is_local;
			return static_cast<NodeDataStructure*>(node_var->replace_with(std::move(node_array)));
		}
	}
	return node;
}



