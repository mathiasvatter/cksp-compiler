//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTBuildDataStructures.h"

ASTBuildDataStructures::ASTBuildDataStructures(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTBuildDataStructures::visit(NodeProgram& node) {
    m_program = &node;
	check_unique_callbacks(node);
	m_init_callback = move_on_init_callback(node);

    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTBuildDataStructures::visit(NodeCallback& node) {
	if(&node == m_init_callback) m_is_init_callback = true;

	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);

	m_is_init_callback = false;
}


void ASTBuildDataStructures::visit(NodeBody &node) {
    if(node.parent->get_node_type() != NodeType::Body and !is_instance_of<NodeDataStructure>(node.parent)) {
        node.scope = true;
    }

    // add scope for body
	if(node.scope) m_def_provider->add_scope();
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	if(node.scope) m_def_provider->remove_scope();
}

void ASTBuildDataStructures::visit(NodeFunctionDefinition &node) {
    m_def_provider->add_scope();
    node.header ->accept(*this);
    if (node.return_variable.has_value())
        node.return_variable.value()->accept(*this);
    node.body->accept(*this);
    m_def_provider->remove_scope();
}

void ASTBuildDataStructures::visit(NodeArray &node) {
	if(node.size) node.size->accept(*this);
//	if(node.index) node.index->accept(*this);
	m_def_provider->set_declaration(&node, m_is_init_callback);
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

}

void ASTBuildDataStructures::visit(NodeNDArray& node) {
	node.sizes->accept(*this);
//	node.indexes->accept(*this);
	m_def_provider->set_declaration(&node, m_is_init_callback);
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
	m_def_provider->set_declaration(&node, m_is_init_callback);
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
	m_def_provider->set_declaration(&node, m_is_init_callback);
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
	auto it = std::find_if(node.callbacks.begin(), node.callbacks.end(), [](const std::unique_ptr<NodeCallback>& callback) {
	  return callback->begin_callback == "on init";
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



