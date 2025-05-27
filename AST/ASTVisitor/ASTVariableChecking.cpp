//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTVariableChecking.h"

ASTVariableChecking::ASTVariableChecking(NodeProgram* main)
	: m_def_provider(main->def_provider) {
	fail = false;
	m_program = main;
}

NodeAST* ASTVariableChecking::visit(NodeProgram& node) {
	m_program = &node;
	// add all function definition header variables to global scope
	for(const auto & func_def : node.function_definitions) {
		m_def_provider->set_declaration(func_def->header, true);
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
	node.declaration = node.get_builtin_widget(m_program);
	if(node.is_ui_control_array()) node.control_var->data_type = DataType::UIArray;

	node.control_var->accept(*this);
	node.params->accept(*this);

	// if fail is set to false, return early. the rest is determined after lowering
	if(!fail) return &node;

	//check param size
	const auto engine_widget = node.get_declaration()->cast<NodeUIControl>();
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
	m_current_block.push(&node);

	node.determine_scope();

	if(node.scope) {
		if(!node.parent->cast<NodeStruct>()) {
			m_def_provider->add_scope();
		}
	}
	// if body is in function definition, copy over last scope of header variables
	if(node.parent->cast<NodeFunctionDefinition>()) {
		m_def_provider->copy_last_scope();
	}
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}

	if(node.scope) {
		if(!node.parent->cast<NodeStruct>()) {
			m_def_provider->remove_scope();
		}
	}
	m_current_block.pop();
	return &node;
}

NodeAST * ASTVariableChecking::visit(NodeFunctionHeader& node) {
	node.determine_locality(m_program, get_current_block());
	for(auto &param : node.params) param->accept(*this);

	// global function headers from definition were already added initially
	if (node.is_local) {
		// node header as data struct
		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	}
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeFunctionDefinition &node) {
	node.visited = true;

	m_program->function_call_stack.push(node.weak_from_this());
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
	if(!node.bind_definition(m_program)) {
		if (auto access_chain = try_access_chain_transform(node.function->name, &node)) {
			access_chain->accept(*this);
			node.replace_with(std::move(access_chain));
			return &node;
		}
	}

	// const auto definition = node.get_definition();
	// if(node.kind == NodeFunctionCall::UserDefined and definition) {
	// 	if(!definition->visited) definition->accept(*this);
	// }
	node.function->accept(*this);

	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeSingleDeclaration& node) {
	node.variable->determine_locality(m_program, get_current_block());

	if(node.variable->cast<NodeUIControl>() and node.variable->is_local) {
		auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
		error.m_message = "<UIControls> cannot be declared as local variables or in local scopes. They have "
						  "to be declared in the <on init> callback.";
		error.exit();
	}
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);
	m_def_provider->add_to_declarations(&node);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	node.r_value->accept(*this);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeArray& node) {
	node.determine_locality(m_program, get_current_block());
	if(node.size) node.size->accept(*this);
	if (node.num_elements) node.num_elements->accept(*this);
	m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);

	if(node.get_declaration()) return &node;
	auto node_declaration = m_def_provider->get_declaration(node);
	// maybe declaration comes after lowering, do not throw error
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			node_declaration = access_chain->get_declaration();
			node.declaration = node_declaration;
			if(node.is_list_sizes()) {
				// if it is a list constant, do not add to references
				node.declaration.reset();
				return &node;
			} else {
				access_chain->accept(*this);
				return node.replace_with(std::move(access_chain));
			}
		}
		if(m_current_struct) {
			auto msg = "When referencing a struct member, remember to use the 'self' keyword to access it. Example: <self."+node.tok.val+">.";
			DefinitionProvider::throw_declaration_error(node, msg).exit();
		}
        if(!fail) return &node;
	    DefinitionProvider::throw_declaration_error(node).exit();
    }

    node.match_data_structure(node_declaration);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeNDArray& node) {
	node.determine_locality(m_program, get_current_block());
	if(node.sizes) node.sizes->accept(*this);
	if (node.num_elements) node.num_elements->accept(*this);
	m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);

	if(node.get_declaration()) return &node;
	auto node_declaration = m_def_provider->get_declaration(node);
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			access_chain->accept(*this);
			return node.replace_with(std::move(access_chain));
		}
		if(m_current_struct) {
			const auto msg = "When referencing a struct member, remember to use the 'self' keyword to access it. Example: <self."+node.tok.val+">.";
			DefinitionProvider::throw_declaration_error(node, msg).exit();
		}
		DefinitionProvider::throw_declaration_error(node).exit();
		return &node;
	}
	node.match_data_structure(node_declaration);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeFunctionHeaderRef& node) {
	if(node.args) node.args->accept(*this);

	// for builtin commands get header variable from its definition
	if(const auto func_call = node.parent->cast<NodeFunctionCall>()) {
		if(const auto def = func_call->get_definition()) {
			node.declaration = def->header;
		}
		return &node;
	}

	if(node.get_declaration()) return &node;
	const auto node_declaration = m_def_provider->get_declaration(node);
	if(!node_declaration) {
		// if (!fail) return &node;
		auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
		error.m_message = "Function Variable has not been declared: "+node.name;
		error.exit();
		return &node;
	}
	node.match_data_structure(node_declaration);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeVariable& node) {
	node.determine_locality(m_program, get_current_block());

	m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeVariableRef& node) {
	if(node.get_declaration()) return &node;
	auto node_declaration = m_def_provider->get_declaration(node);

	// check for array constants
	if(auto nd_constant = node.transform_ndarray_constant()) {
		return node.replace_with(std::move(nd_constant))->accept(*this);
	} else if(auto array_constant = node.transform_array_constant()) {
		return node.replace_with(std::move(array_constant))->accept(*this);
	}
    if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			// check if its maybe a nd_Array size constant like nda.SIZE_D1
			node_declaration = access_chain->get_declaration();
			node.declaration = node_declaration;

			access_chain->accept(*this);
			return node.replace_with(std::move(access_chain));
		} else {

			if(m_current_struct) {
				const auto msg = "When referencing a struct member, remember to use the 'self' keyword to access it. Example: <self."+node.tok.val+">.";
				DefinitionProvider::throw_declaration_error(node, msg).exit();
			}

			// could still fail on ui control array values or raw list subarrays
			if(!fail)
				return &node;
			DefinitionProvider::throw_declaration_error(node).exit();
		}
    }

    node.match_data_structure(node_declaration);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodePointer& node) {
	node.determine_locality(m_program, get_current_block());

	m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodePointerRef& node) {
	if(node.get_declaration()) return &node;
	auto node_declaration = m_def_provider->get_declaration(node);
	if(!node_declaration) {
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			access_chain->accept(*this);
			return node.replace_with(std::move(access_chain));
		}
		// if fail is set to false, return early. the rest is determined after lowering
		DefinitionProvider::throw_declaration_error(node).exit();
	}

	node.match_data_structure(node_declaration);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeList& node) {
	node.determine_locality(m_program, get_current_block());
	for(auto &params : node.body) {
		params->accept(*this);
	}
	m_def_provider->set_declaration(node.get_shared(), !node.is_local);
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeListRef& node) {
	node.indexes->accept(*this);

	if(node.get_declaration()) return &node;
	auto node_declaration = m_def_provider->get_declaration(node);
	if(!node_declaration) {
		CompileError(ErrorType::VariableError, "List has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return &node;
	}
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeForEach& node) {
	m_def_provider->add_scope();

	if(node.key) node.key->accept(*this);
	if(node.value) node.value->accept(*this);
	node.range->accept(*this);
	node.body->accept(*this);

	m_def_provider->remove_scope();

	return &node;
};

NodeAST* ASTVariableChecking::visit(NodeConst& node) {
	for(auto& stmt : node.constants->statements) {
		stmt->accept(*this);
	}
	return &node;
}

NodeAST* ASTVariableChecking::visit(NodeStruct& node) {
	m_current_struct = &node;
	m_def_provider->add_scope();
	// add extra members scope
	m_def_provider->add_scope();
	node.members->accept(*this);
	for(const auto & m : node.methods) {
		m->accept(*this);
	}
	// remove the members scope
	m_def_provider->remove_scope();
	m_def_provider->remove_scope();
	m_current_struct = nullptr;
	return &node;
}




