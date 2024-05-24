//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeCasting.h"

void TypeCasting::visit(NodeProgram& node) {
	m_program = &node;

	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(auto & function_definition : node.function_definitions) {
		function_definition->accept(*this);
	}
}

void TypeCasting::visit(NodeInt& node) {
	node.ty = TypeRegistry::Integer;
}

void TypeCasting::visit(NodeReal& node) {
	node.ty = TypeRegistry::Real;
}

void TypeCasting::visit(NodeString& node) {
	node.ty = TypeRegistry::String;
}

void TypeCasting::visit(NodeVariableRef& node) {
	// try to match type from variable to reference
	match_type(&node, node.declaration);
	// try to match type from reference to variable
	match_type(node.declaration, &node);
}

void TypeCasting::visit(NodeVariable& node) {
}

void TypeCasting::visit(NodeArray& node) {
	// if array is unknown type -> set to array of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::ArrayOfUnknown;
	}
}

void TypeCasting::visit(NodeArrayRef& node) {

	// if handed over without index -> as whole array structure type
	if(!node.index) {
		if(node.ty == TypeRegistry::Unknown) {
			node.ty = TypeRegistry::ArrayOfUnknown;
		}
	} else {
		// handed over as array element -> set to element type
		node.ty = node.ty->get_element_type();
	}
	// try to match type from variable to reference
	match_type(&node, node.declaration);
	// try to match type from reference to variable
	match_type(node.declaration, &node);
}



void TypeCasting::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->accept(*this);
	if(node.assignee) {
		node.assignee->accept(*this);
		match_type(node.to_be_declared.get(), node.assignee.get());
		match_type(node.assignee.get(), node.to_be_declared.get());
	}
}