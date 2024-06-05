//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeCasting.h"

void TypeCasting::visit(NodeProgram& node) {
	m_program = &node;

	// get all types of function params first
	for(auto & function_definition : node.function_definitions) {
		function_definition->accept(*this);
	}
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}

    for(auto & ref : m_references) {
        match_reference_declaration(ref);
    }
    for(auto & decl : m_declarations) {
        if(decl->assignee) {
			match_assignment_types(decl->to_be_declared.get(), decl->assignee.get());
//            match_type(decl->to_be_declared.get(), decl->assignee.get());
//            match_type(decl->assignee.get(), decl->to_be_declared.get());
        }
		// cast as Integer if still unknown
		decl->to_be_declared->cast_type();
    }
    for(auto & ref : m_references) {
        match_reference_declaration(ref);
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
    match_reference_declaration(&node);
    m_references.push_back(&node);
}

void TypeCasting::visit(NodeVariable& node) {
}

void TypeCasting::visit(NodeArray& node) {
	// if array is unknown type -> set to array of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, 1);
	}
}

void TypeCasting::visit(NodeArrayRef& node) {
	// if handed over without index -> as whole array structure type
	if(!node.index) {
		if(node.ty == TypeRegistry::Unknown) {
			node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, TypeRegistry::Unknown, 1);
            if(!node.ty) throw_composite_error(&node).exit();
		}
	} else {
		// handed over as array element -> set to element type
		if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
	}
    match_reference_declaration(&node);
    m_references.push_back(&node);

}

void TypeCasting::visit(NodeNDArray& node) {
    node.sizes->accept(*this);
    // if array is unknown type -> set to array of unknown
    if(node.ty == TypeRegistry::Unknown) {
        node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions);
    }
}

void TypeCasting::visit(NodeNDArrayRef& node) {
    // if handed over without index -> as whole array structure type
    if(!node.indexes) {
        if(node.ty == TypeRegistry::Unknown) {
            node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, TypeRegistry::Unknown, node.sizes->params.size());
            if(!node.ty) throw_composite_error(&node).exit();
        }
    } else {
        // handed over as array element -> set to element type
        if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
    }
    match_reference_declaration(&node);
    m_references.push_back(&node);
}

void TypeCasting::visit(NodeListStruct& node) {
	// if list is unknown type -> set to list of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::add_composite_type(CompoundKind::List, TypeRegistry::Unknown, node.size);
	}
    // check if all types are the same and try to infer list type from it
    std::vector<Type*> types;
    for(auto & b : node.body) {
        b->accept(*this);
        types.push_back(b->ty);
    }
    node.set_element_type(infer_initialization_types(types, &node));

}

void TypeCasting::visit(NodeListStructRef& node) {
    // if handed over without index -> as whole list structure type
    if(!node.indexes) {
        if(node.ty == TypeRegistry::Unknown) {
            node.ty = TypeRegistry::get_composite_type(CompoundKind::List, TypeRegistry::Unknown, node.sizes->params.size());
            if(!node.ty) throw_composite_error(&node).exit();
        }
    } else {
        // handed over as list element -> set to element type
        if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
    }
    match_reference_declaration(&node);
    m_references.push_back(&node);
}

void TypeCasting::visit(NodeParamList& node) {
    std::vector<Type*> types;
    for(const auto & param : node.params) {
        param->accept(*this);
        types.push_back(param->ty);
    }
    // enforce same type for every member only if in array declaration, assignment, liststruct
    auto node_declaration = node.parent->get_node_type() == NodeType::SingleDeclareStatement;
    auto node_assignment = node.parent->get_node_type() == NodeType::SingleAssignStatement;
	auto node_list_struct = node.parent->get_node_type() == NodeType::ListStruct;
    if(!node_declaration and !node_assignment and !node_list_struct) return;

    node.ty = infer_initialization_types(types, &node);
}

void TypeCasting::visit(NodeSingleDeclareStatement& node) {
	node.to_be_declared->accept(*this);

	if(node.assignee) {
		node.assignee->accept(*this);
		// cast node assignee to composite type if to_be_declared is composite type
		if(node.assignee ->get_node_type() == NodeType::ParamList and node.to_be_declared->ty->get_type_kind() == TypeKind::Composite) {
			node.assignee->ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(node.to_be_declared->ty)->get_compound_type(), node.assignee->ty->get_element_type(), node.to_be_declared->ty->get_dimensions());
		}
		match_assignment_types(node.to_be_declared.get(), node.assignee.get());
//        match_type(node.to_be_declared.get(), node.assignee.get());
//		match_type(node.assignee.get(), node.to_be_declared.get());
	}
    m_declarations.push_back(&node);
}

void TypeCasting::visit(NodeUIControl& node) {
    match_type(node.control_var.get(), node.declaration->control_var.get());
    node.control_var->accept(*this);

    for(int i = 0; i < node.params->params.size(); i++) {
        match_type(node.params->params[i].get(), node.declaration->params->params[i].get());
    }
    node.params->accept(*this);
}

void TypeCasting::visit(NodeSingleAssignStatement& node) {
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
	// cast node assignee to composite type if to_be_declared is composite type
	if(node.assignee ->get_node_type() == NodeType::ParamList and node.array_variable->ty->get_type_kind() == TypeKind::Composite) {
		node.assignee->ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(node.array_variable->ty)->get_compound_type(), node.assignee->ty->get_element_type(), node.array_variable->ty->get_dimensions());
	}
	match_assignment_types(node.array_variable.get(), node.assignee.get());
//    match_type(node.array_variable.get(), node.assignee.get());
//    match_type(node.assignee.get(), node.array_variable.get());

    // a second time to get the new types to the declaration pointer!
    node.array_variable->accept(*this);
    node.assignee->accept(*this);
}

void TypeCasting::visit(NodeFunctionCall& node) {
	node.get_definition(m_program);
	if(!node.definition) return;

	node.function->accept(*this);
    for(int i = 0; i < node.function->args->params.size(); i++) {
        match_type(node.function->args->params[i].get(), node.definition->header->args->params[i].get());
    }
    node.function->accept(*this);

    node.ty = node.definition->ty;
}

void TypeCasting::visit(NodeBinaryExpr& node) {
    node.left->accept(*this);
    node.right->accept(*this);

    bool is_compatible = node.left->ty->is_compatible(node.ty) || node.right->ty->is_compatible(node.ty);
    auto error = throw_type_error(node.left.get(), node.right.get());
    if(!is_compatible) error.exit();

	// do not infer type if together in string
	if(contains(STRING_TOKENS, node.op)) {
		node.ty = TypeRegistry::String;
		is_compatible = node.ty->is_compatible(node.left->ty) and node.ty->is_compatible(node.right->ty);
        if(is_compatible) return;
		return;
	}

    node.left->ty = specialize_type(node.left->ty, node.right->ty);
    node.right->ty = specialize_type(node.right->ty, node.left->ty);

    // check type of this node by looking at operator and node.left and node.right
    if (contains(MATH_TOKENS, node.op)) {
        // can only be int op int || float op float
        is_compatible = node.left->ty->is_compatible(TypeRegistry::Integer) and node.right->ty->is_compatible(TypeRegistry::Integer);
        if(is_compatible) node.ty = TypeRegistry::Integer;
        is_compatible |= node.left->ty->is_compatible(TypeRegistry::Real) and node.right->ty->is_compatible(TypeRegistry::Real);
        if(is_compatible and node.ty != TypeRegistry::Integer) node.ty = TypeRegistry::Real;
        error.m_message += "Please use real() and int() to use <Real> and <Integer> numbers in a single expression.";
    } else if (contains(BITWISE_TOKENS, node.op)) {
        node.ty = TypeRegistry::Integer;
        is_compatible = node.left->ty->is_compatible(node.ty) and node.right->ty->is_compatible(node.ty);
        error.m_message += "<Bitwise Operators> can only be used in between <Integer> values.";
    } else if (contains(BOOL_TOKENS, node.op)) {
        node.ty = TypeRegistry::Boolean;
        is_compatible = node.left->ty->is_compatible(node.ty) and node.right->ty->is_compatible(node.ty);
        error.m_message += "<Bool Operators> can only be used in between <Boolean> or <Comparison> values.";

    } else if (contains(COMPARISON_TOKENS, node.op)) {
        node.ty = TypeRegistry::Comparison;
        is_compatible = node.left->ty->is_compatible(node.ty) and node.right->ty->is_compatible(node.ty);
        error.m_message += "<Comparison Operators> can only be used in between <Integer> or <Real> values.";

    } else {
        error.exit();
    }

    if(!is_compatible) error.exit();
    node.left->set_element_type(specialize_type(node.left->ty, node.ty));
    node.right->set_element_type(specialize_type(node.right->ty, node.ty));

}

void TypeCasting::visit(NodeUnaryExpr& node) {
    node.operand->accept(*this);

    bool is_compatible = node.ty->is_compatible(node.operand->ty) && node.operand->ty->is_compatible(node.ty);
    auto error = throw_type_error(node.operand.get(), &node);
    if(!is_compatible) error.exit();

    if(node.op == token::SUB) {
        // can only be int or float
        is_compatible = node.operand->ty->is_compatible(TypeRegistry::Integer);
        if(is_compatible) node.ty = TypeRegistry::Integer;
        is_compatible |= node.operand->ty->is_compatible(TypeRegistry::Real);
        if(is_compatible and node.ty != TypeRegistry::Integer) node.ty = TypeRegistry::Real;
        error.m_message += "Please use real() and int() to use <Real> and <Integer> numbers in a single expression.";
    } else if (node.op == token::BIT_NOT) {
        node.ty = TypeRegistry::Integer;
        is_compatible = node.operand->ty->is_compatible(node.ty);
    } else if(node.op == token::BOOL_NOT) {
        node.ty = TypeRegistry::Boolean;
        is_compatible = node.operand->ty->is_compatible(node.ty);
        error.m_message += "<Bool Operators> can only be used in between <Boolean> or <Comparison> values.";
    } else {
        error.exit();
    }

    if(!is_compatible) error.exit();
    node.operand->ty = specialize_type(node.operand->ty, node.ty);

}
