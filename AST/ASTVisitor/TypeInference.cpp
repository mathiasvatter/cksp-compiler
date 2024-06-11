//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeInference.h"

void TypeInference::visit(NodeProgram& node) {
	m_program = &node;
	m_def_provider->refresh_data_vectors();
	// get all types of function params first
	for(auto & function_definition : node.function_definitions) {
		function_definition->accept(*this);
	}
	m_program->global_declarations->accept(*this);
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
}

void TypeInference::cast_data_structure_types(DefinitionProvider* def_provider, bool cast) {
    for(auto & ref : def_provider->get_all_references()) {
        match_reference_declaration(ref);
    }

    for(auto & decl : def_provider->get_all_declarations()) {
        if(decl->value) {
            match_assignment_types(decl->variable.get(), decl->value.get());
        }
        // cast as Integer if still unknown
        if(cast) decl->variable->cast_type();
    }
    for(auto & ref : def_provider->get_all_references()) {
        match_reference_declaration(ref);
    }
}

void TypeInference::visit(NodeCallback& node) {
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
}


void TypeInference::visit(NodeInt& node) {
    node.ty = TypeRegistry::Integer;
}

void TypeInference::visit(NodeReal& node) {
    node.ty = TypeRegistry::Real;
}

void TypeInference::visit(NodeString& node) {
    node.ty = TypeRegistry::String;
}

void TypeInference::visit(NodeConstStatement& node) {
	node.ty = TypeRegistry::Integer;
	for(auto & constant : node.constants->statements) {
		if(auto decl = cast_node<NodeSingleDeclaration>(constant->statement.get())) {
			if(decl->variable->ty == TypeRegistry::Unknown || decl->variable->ty == TypeRegistry::Integer) {
				decl->variable->ty = TypeRegistry::Integer;
			} else {
				auto error = throw_type_error(decl->variable.get(), &node);
				error.m_message += "Constant Blocks can only contain <Integer> Variables.";
				error.exit();
			}
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Constant Statement is not a declaration.";
			error.exit();
		}
	}
}


void TypeInference::visit(NodeVariableRef& node) {
    match_reference_declaration(&node);

    // check if callback id reference is ui_control
    if(node.parent->get_node_type() == NodeType::Callback) {
        auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
        if(node.data_type != DataType::UIControl) {
            error.m_message = "<Variable> needs to be of type <UI Control> to be referenced in <UI Callback>.";
            error.exit();
        } else {
            // var ref is ui control -> check if it is ui_label
            if(node.declaration and node.declaration->parent and node.declaration->parent->get_node_type() == NodeType::UIControl) {
                auto ui_control = static_cast<NodeUIControl*>(node.declaration->parent);
                if(ui_control->name == "ui_label") {
                    error.m_message = "<UI Label> cannot be referenced in <UI Callback>.";
                    error.exit();
                }
            }
        }
    }
	m_def_provider->add_to_references(&node);
}

void TypeInference::visit(NodeVariable& node) {
	m_def_provider->add_to_data_structures(&node);
}

void TypeInference::visit(NodeArray& node) {
	// if array is unknown type -> set to array of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, 1);
	}
    if(node.size) {
        node.size->accept(*this);
        node.size->ty = specialize_type(node.size->ty, TypeRegistry::Integer);
    }
	m_def_provider->add_to_data_structures(&node);
}

void TypeInference::visit(NodeArrayRef& node) {
	if(node.declaration) node.declaration->accept(*this);
	// if handed over without index -> as whole array structure type
	if(!node.index) {
		if(node.ty == TypeRegistry::Unknown) {
			node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
            if(!node.ty) throw_composite_error(&node).exit();
		}
	} else {
		// handed over as array element -> set to element type
		if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
	}
    match_reference_declaration(&node);

    // check if callback id reference is ui_control
    if(node.parent->get_node_type() == NodeType::Callback) {
        auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
        if (node.data_type != DataType::UIControl) {
            error.m_message = "<Array> needs to be of type <UI Control> to be referenced in <UI Callback>.";
            error.exit();
        }
    }
	m_def_provider->add_to_references(&node);
}

void TypeInference::visit(NodeNDArray& node) {
    node.sizes->accept(*this);
    // if array is unknown type -> set to array of unknown
    if(node.ty == TypeRegistry::Unknown) {
        node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions);
    }
	m_def_provider->add_to_data_structures(&node);
}

void TypeInference::visit(NodeNDArrayRef& node) {
	if(node.declaration) node.declaration->accept(*this);
    // if handed over without index -> as whole array structure type
    if(!node.indexes) {
        if(node.ty == TypeRegistry::Unknown) {
            node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, node.ty->get_element_type(), node.sizes->params.size());
            if(!node.ty) throw_composite_error(&node).exit();
        }
    } else {
        // handed over as array element -> set to element type
        if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
    }
    match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
}

void TypeInference::visit(NodeList& node) {
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
	m_def_provider->add_to_data_structures(&node);
}

void TypeInference::visit(NodeListRef& node) {
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
	m_def_provider->add_to_references(&node);
}

void TypeInference::visit(NodeParamList& node) {
    std::vector<Type*> types;
    for(const auto & param : node.params) {
        param->accept(*this);
        types.push_back(param->ty);
    }
    // enforce same type for every member only if in array declaration, assignment, liststruct
    auto node_declaration = node.parent->get_node_type() == NodeType::SingleDeclaration;
    auto node_assignment = node.parent->get_node_type() == NodeType::SingleAssignment;
	auto node_list_struct = node.parent->get_node_type() == NodeType::List;
    if(!node_declaration and !node_assignment and !node_list_struct) return;

    node.ty = infer_initialization_types(types, &node);
}

void TypeInference::visit(NodeSingleDeclaration& node) {
	node.variable->accept(*this);

	if(node.value) {
		node.value->accept(*this);
		// cast node r_value to composite type if variable is composite type
		if(node.value ->get_node_type() == NodeType::ParamList and node.variable->ty->get_type_kind() == TypeKind::Composite) {
			node.value->ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(node.variable->ty)->get_compound_type(), node.value->ty->get_element_type(), node.variable->ty->get_dimensions());
		}
		match_assignment_types(node.variable.get(), node.value.get());
	}
	m_def_provider->add_to_declarations(&node);
}

void TypeInference::visit(NodeUIControl& node) {
	// check if type is same as provided as builtin
	if(node.ty != TypeRegistry::Unknown and node.ty->get_element_type() != node.declaration->control_var->ty->get_element_type()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Type Annotation of <UI Control> "+node.name+" does not match expected type: "+node.declaration->control_var->ty->to_string()+".";
		error.m_got = node.ty->to_string();
		error.exit();
	}

	// only to matching if node types are the same (because of ui control arrays)
	// eg. declare ui_label %lbl_sdf[3,1](1,1) -> $lbl_sdf
	if(node.control_var->get_node_type() == node.declaration->control_var->get_node_type())
		match_type(node.control_var.get(), node.declaration->control_var.get());
	node.control_var->accept(*this);

	for(int i = 0; i < node.params->params.size(); i++) {
		match_type(node.params->params[i].get(), node.declaration->params->params[i].get());
	}
	node.params->accept(*this);
}

void TypeInference::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	node.r_value->accept(*this);
	// cast node r_value to composite type if variable is composite type
	if(node.r_value ->get_node_type() == NodeType::ParamList and node.l_value->ty->get_type_kind() == TypeKind::Composite) {
		node.r_value->ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(node.l_value->ty)->get_compound_type(), node.r_value->ty->get_element_type(), node.l_value->ty->get_dimensions());
	}
	match_assignment_types(node.l_value.get(), node.r_value.get());

	// a second time to get the new types to the declaration pointer!
	node.l_value->accept(*this);
	node.r_value->accept(*this);
}

void TypeInference::visit(NodeFunctionCall& node) {
	node.get_definition(m_program);
	if(!node.definition) return;
	node.function->accept(*this);
	for(int i = 0; i < node.function->args->params.size(); i++) {
		match_type(node.function->args->params[i].get(), node.definition->header->args->params[i].get());
	}

	node.ty = node.definition->ty;
}

void TypeInference::visit(NodeFunctionDefinition& node) {
    node.header->accept(*this);
    if(node.return_variable.has_value()) node.return_variable.value()->accept(*this);

    node.body->accept(*this);
    // get possible return type of node.definition by looking at the return param
    if(node.return_variable.has_value())
        for(auto & return_var : node.return_variable.value()->params)
            match_type(&node, return_var.get());

}

void TypeInference::visit(NodeBinaryExpr& node) {
	node.left->accept(*this);
	node.right->accept(*this);

	// do not infer type if together in string
	if(contains(STRING_TOKENS, node.op)) {
		node.ty = TypeRegistry::String;
//		is_compatible = node.ty->is_compatible(node.left->ty) and node.ty->is_compatible(node.right->ty);
//        if(is_compatible) return;
		return;
	}

	bool is_compatible = true; //node.left->ty->is_compatible(node.ty) || node.right->ty->is_compatible(node.ty);
	auto error = throw_type_error(node.left.get(), node.right.get());
//	if(!is_compatible)
//        error.exit();


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

	if(!is_compatible)
        error.exit();
	node.left->set_element_type(specialize_type(node.left->ty, node.ty));
	node.right->set_element_type(specialize_type(node.right->ty, node.ty));

}

void TypeInference::visit(NodeUnaryExpr& node) {
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
