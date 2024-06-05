//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTBuildDataStructures.h"

ASTBuildDataStructures::ASTBuildDataStructures(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTBuildDataStructures::visit(NodeProgram& node) {
    m_program = &node;
	check_unique_callbacks(node);
	node.init_callback = move_on_init_callback(node);
	m_def_provider->refresh_scopes();

	// most func defs will be visited when called, replacing the var ref params with arrays
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
	// visit func defs that are not called. because of replacing incorrectly node params with array
    for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
		// reset visited flag
		func_def->visited = false;
	}
}

void ASTBuildDataStructures::visit(NodeCallback& node) {
	m_program->current_callback = &node;

	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);

	m_program->current_callback = nullptr;
}


void ASTBuildDataStructures::visit(NodeBody &node) {
    m_current_body = &node;
    if(node.parent->get_node_type() != NodeType::Statement and !is_instance_of<NodeDataStructure>(node.parent)) {
        node.scope = true;
    }

    // add scope for body
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

void ASTBuildDataStructures::visit(NodeFunctionDefinition &node) {
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

void ASTBuildDataStructures::visit(NodeFunctionCall& node) {
	node.function->accept(*this);

	node.get_definition(m_program);

	if(node.kind == NodeFunctionCall::UserDefined and node.definition) {
		if(!node.definition->visited) node.definition->accept(*this);
	}

	if(!m_program->function_call_stack.empty()) {
		m_program->function_call_stack.top()->header->is_thread_safe &= node.function->is_thread_safe;
	}
	if(m_program->current_callback) m_program->current_callback->is_thread_safe &= node.function->is_thread_safe;
	if(m_program->current_callback) node.function->is_thread_safe &= m_program->current_callback->is_thread_safe;

	// if definition parameters of this function have different node types as the call site -> update
	update_func_call_node_types(&node);
}

void ASTBuildDataStructures::update_func_call_node_types(NodeFunctionCall* func_call) {
	if(func_call->kind == NodeFunctionCall::Kind::UserDefined and func_call->definition) {
		for(int i=0; i<func_call->function->args->params.size(); i++) {
			auto & arg = func_call->function->args->params.at(i);
			auto & param = func_call->definition->header->args->params.at(i);
			if(arg->get_node_type() == NodeType::VariableRef and param->get_node_type() == NodeType::Array) {
				auto node_var_ref = static_cast<NodeVariableRef*>(arg.get());
				auto node_array_ref = std::make_unique<NodeArrayRef>(
					node_var_ref->name,
					nullptr,
					node_var_ref->tok);
				if(node_var_ref->declaration)
					node_array_ref->match_data_structure(node_var_ref->declaration);
				node_var_ref->replace_with(std::move(node_array_ref));
			}
		}
	}
}

void ASTBuildDataStructures::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->determine_locality(m_program, m_current_body);

    node.to_be_declared->accept(*this);
    if(node.assignee) node.assignee->accept(*this);
}

void ASTBuildDataStructures::visit(NodeArray &node) {
	node.determine_locality(m_program, m_current_body);

	if(node.size) node.size->accept(*this);
	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTBuildDataStructures::visit(NodeArrayRef &node) {
    if(node.index) node.index->accept(*this);

    auto node_declaration = m_def_provider->get_declaration(&node);
    // maybe declaration comes after lowering, do not throw error
    if(!node_declaration) return;

    node.match_data_structure(node_declaration);
    // check if it is NodeListStructRef
    if(node_declaration->get_node_type() == NodeType::ListStructRef) {
        std::unique_ptr<NodeParamList> indexes = std::make_unique<NodeParamList>(node.tok);
        indexes->params.push_back(std::move(node.index));
        auto node_list_reference = std::make_unique<NodeListStructRef>(node.name, std::move(indexes), node.tok);
        node_list_reference->declaration = node_declaration;
        node.replace_with(std::move(node_list_reference));
        return;
    }
	// if is in function definition and param is incorrectly recognized as Variable -> change to Array
	if(node_declaration->get_node_type() == NodeType::Variable and node_declaration->is_function_param()) {
		auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			node.name,
			node.ty,
			DataType::Array,
			std::make_unique<NodeInt>(1, node.tok),
			node.tok);
		node_array->parent = node_declaration->parent;
		m_def_provider->remove_from_current_scope(node.name);
		node_array->accept(*this);
		node_declaration->replace_with(std::move(node_array));
		return;
	}

}

void ASTBuildDataStructures::visit(NodeNDArray& node) {
	node.determine_locality(m_program, m_current_body);

	node.sizes->accept(*this);
	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTBuildDataStructures::visit(NodeNDArrayRef& node) {
    node.indexes->accept(*this);

    auto node_declaration = m_def_provider->get_declaration(&node);
    if(!node_declaration) {
        CompileError(ErrorType::Variable, "Multidimensional array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
        return;
    }

   	node.match_data_structure(node_declaration);
    // check if it is NodeListStructRef
    if(node_declaration->get_node_type() == NodeType::ListStruct) {
        auto node_list_reference = std::make_unique<NodeListStructRef>(node.name, std::move(node.indexes), node.tok);
        node_list_reference->declaration = node_declaration;
        node_list_reference->accept(*this);
        node.replace_with(std::move(node_list_reference));
        return;
    }

    if(auto node_array = cast_node<NodeNDArray>(node_declaration)) {
        node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
        node.sizes->update_parents(&node);
    } else {
        CompileError(ErrorType::Variable, "Incorrectly recognized as <ndarray>: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
    }
}

// get declaration of engine widget into declaration
// if ui control array -> get size(s) into size member
void ASTBuildDataStructures::visit(NodeUIControl &node) {
	auto engine_widget = m_def_provider->get_builtin_widget(node.ui_control_type);
	auto error = CompileError(ErrorType::SyntaxError, "", node.tok.line, "", node.ui_control_type, node.tok.file);
	if(!engine_widget) {
		error.m_message = "Did not recognize engine widget.";
		error.m_expected = "valid widget type";
		error.m_got = node.ui_control_type;
		error.exit();
	}
	node.declaration = engine_widget;

	node.control_var->accept(*this);
	node.params->accept(*this);
}


void ASTBuildDataStructures::visit(NodeVariable &node) {
	node.determine_locality(m_program, m_current_body);

	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTBuildDataStructures::visit(NodeVariableRef &node) {
    auto node_declaration = m_def_provider->get_declaration(&node);
    // return if no declaration found it might come after lowering
    if(!node_declaration) {
        return;
    }

	node.match_data_structure(node_declaration);
    // replace variable with array if incorrectly recognized by parser
    if(node_declaration->get_node_type() == NodeType::Array) {
        auto node_array = std::make_unique<NodeArrayRef>(node.name, nullptr, node.tok);
        node_array->accept(*this);
        node.replace_with(std::move(node_array));
        return;
    }
}

void ASTBuildDataStructures::visit(NodeListStruct& node) {
	node.determine_locality(m_program, m_current_body);

	for(auto &params : node.body) {
		params->accept(*this);
	}

	m_def_provider->set_declaration(&node, !node.is_local);
}

void ASTBuildDataStructures::visit(NodeListStructRef& node) {
	node.indexes->accept(*this);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "List has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}

}

NodeCallback* ASTBuildDataStructures::move_on_init_callback(NodeProgram& node) {
	// Finden des ersten (und einzigen) on init Callbacks
	auto it = std::find_if(node.callbacks.begin(), node.callbacks.end(), [&](const std::unique_ptr<NodeCallback>& callback) {
	  return callback.get() == node.init_callback;
	});
	// Move the callback to the first position
	if (it != node.callbacks.end()) {
		std::rotate(node.callbacks.begin(), it, std::next(it));
	}
	return it->get(); // Return the pointer to the init callback
}

bool ASTBuildDataStructures::check_unique_callbacks(NodeProgram& node) {
	auto error = CompileError(ErrorType::SyntaxError, "", -1, "", "", node.tok.file);
	std::unordered_map<std::string, int> callback_counts;
	// Zähle jede Callback-Bezeichnung, außer "on ui_control"
	for (const auto& callback : node.callbacks) {
		if (callback->begin_callback != "on ui_control") {
			callback_counts[callback->begin_callback]++;
		}
	}
	// Überprüfe die Anzahl jeder Bezeichnung, sollte genau 1 sein
	for (const auto& count : callback_counts) {
		if (count.second > 1) {
			error.m_message = "Unable to compile. Multiple <" + count.first + "> callbacks found.";
			error.m_expected = '1';
			error.m_got = std::to_string(count.second);
			error.exit();
			return false;
		}
	}
	return true;
}



