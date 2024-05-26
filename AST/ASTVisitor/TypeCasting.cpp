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

void TypeCasting::visit(NodeParamList& node) {
    std::vector<Type*> types;
    for(const auto & param : node.params) {
        param->accept(*this);
        types.push_back(param->ty);
    }
    // enforce same type for every member only if in array declaration, assignment
    auto node_declaration = node.parent->get_node_type() == NodeType::SingleDeclareStatement;
    auto node_assignment = node.parent->get_node_type() == NodeType::SingleAssignStatement;
    if(!node_declaration and !node_assignment) return;

    // Use Sie std::adjacent_find to check if all types are the same
    auto it = std::adjacent_find(types.begin(), types.end(),
                                 [](const Type* a, const Type* b) { return a != b; });
    bool all_same = (it == types.end()); // true, if all types equal

    // TODO when uninitialized "UNKNOWN" var is in-> error, when initialized variable but not const -> different generation

    // if param list in array declaration or assignment return composite type
    if(all_same and !types.empty()) {
        node.ty = TypeRegistry::get_composite_type(CompositeType::Array, node.params[0]->ty);
    }
    if(!all_same) {
        size_t position = std::distance(types.begin(), it);
        throw_type_error(&node, node.params[position].get()).exit();
    }
}

void TypeCasting::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->accept(*this);
	if(node.assignee) {
		node.assignee->accept(*this);
		match_type(node.to_be_declared.get(), node.assignee.get());
		match_type(node.assignee.get(), node.to_be_declared.get());
	}
}

void TypeCasting::visit(NodeUIControl& node) {
    match_type(node.control_var.get(), node.declaration);
    node.control_var->accept(*this);

    for(int i = 0; i < node.params->params.size(); i++) {
        match_type(node.params->params[i].get(), node.declaration->params->params[i].get());
    }
    node.params->accept(*this);
}

void TypeCasting::visit(NodeSingleAssignStatement& node) {
    node.assignee->accept(*this);
    node.array_variable->accept(*this);

    match_type(node.array_variable.get(), node.assignee.get());
    match_type(node.assignee.get(), node.array_variable.get());

    // a second time to get the new types to the declaration pointer!
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}

void TypeCasting::visit(NodeFunctionCall& node) {
    for(int i = 0; i < node.function->args->params.size(); i++) {
        match_type(node.function->args->params[i].get(), node.definition->header->args->params[i].get());
    }
    node.function->accept(*this);

    node.ty = node.definition->ty;
}
