//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTBuildDataStructures.h"

ASTBuildDataStructures::ASTBuildDataStructures(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTBuildDataStructures::visit(NodeBody &node) {
	if(node.scope) m_def_provider->add_scope();
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	if(node.scope) m_def_provider->remove_scope();
}

void ASTBuildDataStructures::visit(NodeArray &node) {
    node.type = infer_type_from_identifier(node.name);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		return;
	}

	m_def_provider->match_data_structure(node_declaration, &node);

	// match array specific information
	if(node_declaration != &node and node.is_reference) {
		if(auto node_array = cast_node<NodeArray>(node_declaration)) {
			node.dimensions = node_array->dimensions;
			node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
			node.sizes->update_parents(&node);
		}
	}

}

void ASTBuildDataStructures::visit(NodeNDArray& node) {
    node.type = infer_type_from_identifier(node.name);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
	}

	m_def_provider->match_data_structure(node_declaration, &node);

	// match array specific information
	if(node_declaration != &node and node.is_reference) {
		if(auto node_array = cast_node<NodeNDArray>(node_declaration)) {
			node.dimensions = node_array->dimensions;
			node.sizes = clone_as<NodeParamList>(node_array->sizes.get());
			node.sizes->update_parents(&node);
		}
	}
}

void ASTBuildDataStructures::visit(NodeListStructReference& node) {
	node.type = infer_type_from_identifier(node.name);

	auto node_declaration = m_def_provider->get_declaration(&node);
	if(!node_declaration) {
		CompileError(ErrorType::Variable, "Array has not been declared: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
		return;
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

	// in case of nd array it will get lowered here:
	node.control_var->accept(*this);
	node.params->accept(*this);
}


void ASTBuildDataStructures::visit(NodeVariable &node) {
    node.type = infer_type_from_identifier(node.name);

	auto node_declaration = m_def_provider->get_declaration(&node);
	// return if no declaration found or node itself is declaration
	if(!node_declaration) {
		return;
	}
	m_def_provider->match_data_structure(node_declaration, &node);
	// replace variable with array if incorrectly recognized by parser
	if(node_declaration->get_node_type() == NodeType::Array) {
		auto node_array = make_array(node.name, 0, node.tok, node.parent);
		node_array->sizes->params.clear();
		node_array->show_brackets = false;
		node.replace_with(std::move(node_array));
		return;
	}

}


