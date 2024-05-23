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

void TypeCasting::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->accept(*this);



}