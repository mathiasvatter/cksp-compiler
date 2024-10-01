//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTVariableChecking.h"
#include <future>

ASTVariableChecking::ASTVariableChecking(DefinitionProvider* definition_provider, bool fail)
	: m_def_provider(definition_provider), fail(fail) {}

NodeAST* ASTVariableChecking::visit(NodeProgram& node) {
	m_program = &node;
	// update function lookup map because of altered param counts after lambda lifting
    node.update_function_lookup();
	// erase all previously saved scopes
	m_def_provider->refresh_scopes();
	m_def_provider->refresh_data_vectors();

	// refresh call_sites of function definitions
	for(const auto & func_def : node.function_definitions) func_def->call_sites.clear();
	for(const auto & func_def : node.function_definitions) {
		// node header as data struct
		m_def_provider->set_declaration(func_def->header.get(), !func_def->header->is_local);
		m_def_provider->add_to_data_structures(func_def->header.get());
	}

	// most func defs will be visited when called, keeping local scopes in mind
	m_program->global_declarations->accept(*this);
	m_program->init_callback->accept(*this);
	for(const auto & s : node.struct_definitions) {
		s->accept(*this);
	}
	for(const auto & callback : node.callbacks) {
		if(callback.get() != m_program->init_callback) callback->accept(*this);
	}
	for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}

	node.reset_function_visited_flag();
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeCallback& node) {
	m_program->current_callback = &node;

	if(node.callback_id) {
		node.callback_id->accept(*this);
		check_callback_id_data_type(node.callback_id.get());
	}
	node.statements->accept(*this);

	m_program->current_callback = nullptr;
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeUIControl& node) {
	auto engine_widget = m_def_provider->get_builtin_widget(node.ui_control_type);
	if(!engine_widget) {
		auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
		error.m_message = "Unknown Engine Widget";
		error.m_got = node.ui_control_type;
		error.exit();
	}
	node.declaration = engine_widget;
	if(node.is_ui_control_array()) node.control_var->data_type = DataType::UIArray;

	node.control_var->accept(*this);
	node.params->accept(*this);

	// if fail is set to false, return early. the rest is determined after lowering
	if(!fail) return &node;

	//check param size
	if(engine_widget->params->params.size() != node.params->params.size()) {
		auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
		error.m_message = "Engine Widget has incorrect number of parameters.";
		error.m_expected = std::to_string(engine_widget->params->params.size());
		error.m_got = std::to_string(node.params->params.size());
		error.exit();
	}
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeBlock &node) {
	node.flatten();
	m_current_block = &node;

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
	return &node;
}

NodeAST * ASTVariableChecking::visit(NodeFunctionHeader& node) {
	if(node.is_function_param()) {
		// node header as data struct
		m_def_provider->set_declaration(&node, !node.is_local);
		m_def_provider->add_to_data_structures(&node);
	}
	if(node.args) node.args->accept(*this);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeFunctionDefinition &node) {
	node.visited = true;
	m_program->function_call_stack.push(&node);
	m_def_provider->add_scope();


	node.header ->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->accept(*this);
	m_def_provider->remove_scope();
	m_program->function_call_stack.pop();
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeAccessChain& node) {
	node.chain[0]->accept(*this);
	node.flatten();
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeFunctionCall &node) {
	node.function->accept(*this);

	if(!node.get_definition(m_program)) {
		if (auto access_chain = try_access_chain_transform(node.function->name, &node)) {
			access_chain->accept(*this);
			node.replace_with(std::move(access_chain));
			return &node;
		}
	}

	if(node.kind == NodeFunctionCall::UserDefined and node.definition) {
		node.definition->call_sites.emplace(&node);
		check_recursion(node.definition);
		if(!node.definition->visited) {
			m_functions_in_use.insert(node.definition);
			node.definition->accept(*this);
			m_functions_in_use.erase(node.definition);
		}
	}

	// check if we are in a function and the header is an arg of the current function
	if(!m_program->function_call_stack.empty() and !node.definition and node.kind == NodeFunctionCall::Undefined) {
		auto function_ref = std::make_unique<NodeFunctionVarRef>(node.function->name, nullptr, node.function->tok);
		auto node_declaration = m_def_provider->get_declaration(function_ref.get());
		if(node_declaration) {
			function_ref->header = std::move(node.function);
			function_ref->header->parent = function_ref.get();
			auto new_node = static_cast<NodeReference*>(node.replace_with(std::move(function_ref)));
			new_node->match_data_structure(node_declaration);
			m_def_provider->add_to_references(new_node);
			return new_node;
		}
	}

	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeSingleDeclaration& node) {
	node.variable->determine_locality(m_program, m_current_block);

    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);
	m_def_provider->add_to_declarations(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeArray& node) {
	check_annotation_with_expected(&node, TypeRegistry::ArrayOfUnknown);

//	set_as_function_param(&node);

	node.determine_locality(m_program, m_current_block);
	if(node.size) node.size->accept(*this);
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
	return new_node;
}

NodeAST* ASTVariableChecking::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	// maybe declaration comes after lowering, do not throw error
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			node_declaration = access_chain->declaration;
			node.declaration = node_declaration;
			if(node.is_list_sizes()) {
				// if it is a list constant, do not add to references
				node.declaration = nullptr;
				return &node;
			} else {
				access_chain->accept(*this);
				return node.replace_with(std::move(access_chain));
			}
		}
        if(!fail) return &node;
	    DefinitionProvider::throw_declaration_error(node).exit();
    }

    node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeNDArray& node) {
	check_annotation_with_expected(&node, std::make_unique<CompositeType>(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions).get());
//	set_as_function_param(&node);
	node.determine_locality(m_program, m_current_block);
	if(node.sizes) node.sizes->accept(*this);
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
	return new_node;
}

NodeAST* ASTVariableChecking::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			access_chain->accept(*this);
			return node.replace_with(std::move(access_chain));
		}
		CompileError(ErrorType::Variable, "Multidimensional array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return &node;
	}
	node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeFunctionVarRef& node) {
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Function Variable has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return &node;
	}
	node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeVariable& node) {
	check_annotation_with_expected(&node, TypeRegistry::Unknown);
//	set_as_function_param(&node);
	node.determine_locality(m_program, m_current_block);

	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
	return new_node;
}

NodeAST* ASTVariableChecking::visit(NodeVariableRef& node) {
	auto node_declaration = m_def_provider->get_declaration(&node);
    if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			// check if its maybe a nd_Array size constant like nda.SIZE_D1
			node_declaration = access_chain->declaration;
			node.declaration = node_declaration;
			if(node.is_ndarray_constant() || node.is_array_constant()) {
				// if it is a constant, do not add to references
				node.declaration = nullptr;
				node.data_type = DataType::Const;
				return &node;
			} else {
				access_chain->accept(*this);
				return node.replace_with(std::move(access_chain));
			}
		} else {
			// if data_type is const -> do not throw error since constant variable ref
			if(node.data_type == DataType::Const) return &node;

			// could still fail on ui control array values or raw list subarrays
			if(!fail)
				return &node;
			DefinitionProvider::throw_declaration_error(node).exit();
		}
    }

    node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodePointer& node) {
	check_annotation_with_expected(&node, TypeRegistry::Unknown);
//	set_as_function_param(&node);
	node.determine_locality(m_program, m_current_block);

	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
	return new_node;
}

NodeAST* ASTVariableChecking::visit(NodePointerRef& node) {
	// handle return_vars -> do not check if they have been declared
	if(node.is_compiler_return) {
		return &node;
	}
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			access_chain->accept(*this);
			return node.replace_with(std::move(access_chain));
		}
		// if fail is set to false, return early. the rest is determined after lowering
//		if(!fail) return;
		DefinitionProvider::throw_declaration_error(node).exit();
	}

	node.match_data_structure(node_declaration);
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeList& node) {
	check_annotation_with_expected(&node, std::make_unique<CompositeType>(CompoundKind::List, TypeRegistry::Unknown, 1).get());
//	set_as_function_param(&node);
	node.determine_locality(m_program, m_current_block);
	for(auto &params : node.body) {
		params->accept(*this);
	}
	auto new_node = apply_type_annotations(&node);
	m_def_provider->set_declaration(new_node, !new_node->is_local);
	m_def_provider->add_to_data_structures(new_node);
	return new_node;
}

NodeAST* ASTVariableChecking::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "List has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return &node;
	}
	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeConst& node) {
	for(auto& stmt : node.constants->statements) {
		stmt->accept(*this);
	}
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeStruct& node) {
	m_def_provider->add_scope();
	node.members->accept(*this);
	for(auto & m : node.methods) {
		m->accept(*this);
	}
	m_def_provider->remove_scope();
	return &node;
}


NodeDataStructure* ASTVariableChecking::apply_type_annotations(NodeDataStructure* node) {
	if(node->ty == TypeRegistry::Unknown) return node;

	NodeDataStructure* new_data_struct = nullptr;
	if(node->ty->get_type_kind() == TypeKind::Composite) {
		auto comp_type = static_cast<CompositeType*>(node->ty);
		// if var is annotated as array, replace with array
		if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::Array and comp_type->get_dimensions() == 1) {
			auto node_array = node->to_array(nullptr);
			if(!node_array) get_apply_type_annotations_error(node).exit();
			node_array->is_local = node->is_local;
			new_data_struct = static_cast<NodeDataStructure*>(node->replace_with(std::move(node_array)));
		} else if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::NDArray and comp_type->get_dimensions() > 1) {
			auto node_ndarray = node->to_ndarray();
			if(!node_ndarray) get_apply_type_annotations_error(node).exit();
			node_ndarray->dimensions = comp_type->get_dimensions();
			node_ndarray->is_local = node->is_local;
			new_data_struct = static_cast<NodeDataStructure*>(node->replace_with(std::move(node_ndarray)));
		}
	} else if (node->ty->get_type_kind() == TypeKind::Basic) {
		// if var is annotated as variable but recognized as array by parser -> throw error
		if(node->get_node_type() == NodeType::Array or node->get_node_type() == NodeType::NDArray) {
			auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
			syntax_error.m_message += " Variable was annotated as <Variable> but recognized as <Array>: "+node->name+".";
			syntax_error.m_expected = "<Variable> Syntax";
			syntax_error.m_got = "<Array> Syntax";
			syntax_error.exit();
			return nullptr;
		}
	// var was annotated as object
	} else if (node->ty->get_type_kind() == TypeKind::Object and node->get_node_type() != NodeType::Pointer) {
		// throw error when node was not recognized as variable by parser
		if(node->get_node_type() != NodeType::Variable) {
			auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
			syntax_error.m_message += " Variable was annotated as <Object> but recognized as <Array>: "+node->name+".";
			syntax_error.m_expected = "<Object> Syntax";
			syntax_error.m_got = "<Array> Syntax";
			syntax_error.exit();
			return nullptr;
		} else {
			auto node_pointer = node->to_pointer();
			if(!node_pointer) get_apply_type_annotations_error(node).exit();
			node_pointer->is_local = node->is_local;
			new_data_struct = static_cast<NodeDataStructure*>(node->replace_with(std::move(node_pointer)));
		}
	} else if(node->ty->get_type_kind() == TypeKind::Function and node->get_node_type() != NodeType::FunctionHeader and node->get_node_type() == NodeType::Variable) {
		auto node_function = std::make_unique<NodeFunctionHeader>(node->name, std::make_unique<NodeParamList>(node->tok), node->tok);
		if(!node_function) get_apply_type_annotations_error(node).exit();
		node_function->is_local = node->is_local;
		node_function->ty = node->ty;
		new_data_struct = static_cast<NodeDataStructure*>(node->replace_with(std::move(node_function)));
	}
	if(new_data_struct) return new_data_struct;
	return node;
}



