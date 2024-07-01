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
	m_def_provider->refresh_data_vectors();

	// most func defs will be visited when called, keeping local scopes in mind
	m_program->global_declarations->accept(*this);
	for(auto & s : node.struct_definitions) {
		s->accept(*this);
	}
	m_program->init_callback->accept(*this);
	for(auto & callback : node.callbacks) {
		if(callback.get() != m_program->init_callback) callback->accept(*this);
	}
	for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
		// reset visited flag
		func_def->visited = false;
	}
}

void ASTVariableChecking::visit(NodeCallback& node) {
	m_program->current_callback = &node;

	if(node.callback_id) {
		node.callback_id->accept(*this);
		if(fail) check_callback_id_data_type(node.callback_id.get());
	}
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

void ASTVariableChecking::visit(NodeBlock &node) {
    node.cleanup_body();
	m_current_block = &node;
//	if(node.parent->get_node_type() != NodeType::Statement and !is_instance_of<NodeDataStructure>(node.parent)) {
//		node.scope = true;
//	}
	node.determine_scope();

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

}

void ASTVariableChecking::visit(NodeSingleDeclaration& node) {
	node.variable->determine_locality(m_program, m_current_block);

    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);
	m_def_provider->add_to_declarations(&node);
}

void ASTVariableChecking::visit(NodeArray& node) {
	check_annotation_with_expected(&node, TypeRegistry::ArrayOfUnknown);

	node.determine_locality(m_program, m_current_block);
	if(node.size) node.size->accept(*this);
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
}

void ASTVariableChecking::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	// maybe declaration comes after lowering, do not throw error
	if(!node_declaration) {
        if(!fail) return;
	    DefinitionProvider::throw_declaration_error(&node).exit();
    }

    node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
}

void ASTVariableChecking::visit(NodeNDArray& node) {
	check_annotation_with_expected(&node, std::make_unique<CompositeType>(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions).get());
	node.determine_locality(m_program, m_current_block);
	if(node.sizes) node.sizes->accept(*this);
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
}

void ASTVariableChecking::visit(NodeNDArrayRef& node) {
	node.indexes->accept(*this);
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Multidimensional array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}
	node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
}

void ASTVariableChecking::visit(NodeVariable& node) {
	check_annotation_with_expected(&node, TypeRegistry::Unknown);
	node.determine_locality(m_program, m_current_block);

	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		node.is_used = true;
		return;
	}
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
}

void ASTVariableChecking::visit(NodeVariableRef& node) {
	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		return;
	}
	auto node_declaration = m_def_provider->get_declaration(&node);
    if(!node_declaration) {
        if(!fail) return;
        DefinitionProvider::throw_declaration_error(&node).exit();
    }

    node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
}

void ASTVariableChecking::visit(NodePointer& node) {
	check_annotation_with_expected(&node, TypeRegistry::Unknown);
	node.determine_locality(m_program, m_current_block);

	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		node.is_used = true;
		return;
	}
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
}

void ASTVariableChecking::visit(NodePointerRef& node) {
	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		return;
	}
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		if(!fail) return;
		DefinitionProvider::throw_declaration_error(&node).exit();
	}

	node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
}

void ASTVariableChecking::visit(NodeList& node) {
	check_annotation_with_expected(&node, std::make_unique<CompositeType>(CompoundKind::List, TypeRegistry::Unknown, 1).get());
	node.determine_locality(m_program, m_current_block);
	for(auto &params : node.body) {
		params->accept(*this);
	}
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
}

void ASTVariableChecking::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "List has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}
	m_def_provider->add_to_references(&node);
}

void ASTVariableChecking::visit(NodeConstBlock& node) {
//	for(auto & constants : node.constants->statements) {
//		if(constants->statement->get_node_type() == NodeType::SingleDeclaration) {
//			auto decl = static_cast<NodeSingleDeclaration*>(constants->statement.get());
//			decl->variable
//
//		}
//	}
}

void ASTVariableChecking::visit(NodeStruct& node) {
	m_def_provider->add_scope();
	node.members->accept(*this);
	for(auto & m : node.methods) {
		m->accept(*this);
	}
	m_def_provider->remove_scope();
}


NodeDataStructure* ASTVariableChecking::apply_type_annotations(NodeDataStructure* node) {
	if(node->ty == TypeRegistry::Unknown) return node;
	auto error = CompileError(ErrorType::InternalError, "", "", node->tok);
	error.m_message = "Type Annotation cannot be applied to node: "+node->name+".";
	error.m_got = node->ty->to_string();
	if(node->ty->get_type_kind() == TypeKind::Composite) {
		auto comp_type = static_cast<CompositeType*>(node->ty);
		// if var is annotated as array, replace with array
		if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::Array and comp_type->get_dimensions() == 1) {
			auto node_array = node->to_array(nullptr);
			if(!node_array) error.exit();
			node_array->is_local = node->is_local;
			return static_cast<NodeDataStructure*>(node->replace_with(std::move(node_array)));
		} else if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::NDArray and comp_type->get_dimensions() > 1) {
			auto node_ndarray = node->to_ndarray();
			if(!node_ndarray) error.exit();
			node_ndarray->dimensions = comp_type->get_dimensions();
			node_ndarray->is_local = node->is_local;
			return static_cast<NodeDataStructure*>(node->replace_with(std::move(node_ndarray)));
		}
	} else if (node->ty->get_type_kind() == TypeKind::Basic) {
		auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
		// if var is annotated as variable but recognized as array by parser -> throw error
		if(node->get_node_type() == NodeType::Array or node->get_node_type() == NodeType::NDArray) {
			syntax_error.m_message += " Variable was annotated as <Variable> but recognized as <Array>: "+node->name+".";
			syntax_error.m_expected = "<Variable> Syntax";
			syntax_error.m_got = "<Array> Syntax";
			syntax_error.exit();
			return nullptr;
		}
	// var was annotated as object
	} else if (node->ty->get_type_kind() == TypeKind::Object and node->get_node_type() != NodeType::Pointer) {
		// throw error when node was not recognized as variable by parser
		auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
		if(node->get_node_type() != NodeType::Variable) {
			syntax_error.m_message += " Variable was annotated as <Object> but recognized as <Array>: "+node->name+".";
			syntax_error.m_expected = "<Object> Syntax";
			syntax_error.m_got = "<Array> Syntax";
			syntax_error.exit();
			return nullptr;
		} else {
			auto node_pointer = node->to_pointer();
			if(!node_pointer) error.exit();
			node_pointer->is_local = node->is_local;
			return static_cast<NodeDataStructure*>(node->replace_with(std::move(node_pointer)));
		}
	}
	return node;
}



