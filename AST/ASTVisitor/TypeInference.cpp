//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeInference.h"
#include "ASTVariableChecking.h"
#include "ASTSemanticAnalysis.h"

void TypeInference::visit(NodeProgram& node) {
	m_program = &node;
//	m_def_provider->refresh_data_vectors();
	m_def_provider->m_all_declarations.clear();
	m_def_provider->m_all_references.clear();
	m_def_provider->m_all_declarations.clear();
	m_program->global_declarations->accept(*this);
	for(auto & s : node.struct_definitions) {
		s->accept(*this);
	}
	// get all types of function params first
	for(auto & function_definition : node.function_definitions) {
		function_definition->accept(*this);
	}
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
}

void TypeInference::cast_data_structure_types(DefinitionProvider* def_provider, bool cast) {
//    for(auto & ref : def_provider->get_all_references()) {
//        match_reference_declaration(ref);
//    }
	for(auto& refs : def_provider->m_references_per_data_structure) {
		for(auto & ref : refs.second) {
			match_reference_declaration(ref);
		}
	}

    for(auto & decl : def_provider->get_all_declarations()) {
		if (decl->value) {
			match_assignment_types(decl->variable.get(), decl->value.get());
		}
		// cast as Integer if still unknown
		if (cast) decl->variable->cast_type();
	}

//    for(auto & ref : def_provider->get_all_references()) {
//        match_reference_declaration(ref);
//    }
	for(auto& refs : def_provider->m_references_per_data_structure) {
		for(auto & ref : refs.second) {
			match_reference_declaration(ref);
		}
	}
}

void TypeInference::visit(NodeCallback& node) {
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
}


void TypeInference::visit(NodeInt& node) {
    node.ty = TypeRegistry::Integer;
}

void TypeInference::visit(NodeString& node) {
    node.ty = TypeRegistry::String;
}

void TypeInference::visit(NodeReal& node) {
    node.ty = TypeRegistry::Real;
}

void TypeInference::visit(NodeNil& node) {
	node.ty = TypeRegistry::Nil;
}

void TypeInference::visit(NodeConst& node) {
	node.ty = TypeRegistry::Integer;
	for(auto & constant : node.constants->statements) {
		constant->accept(*this);
		if(constant->statement->get_node_type() == NodeType::SingleDeclaration) {
			auto decl = static_cast<NodeSingleDeclaration*>(constant->statement.get());
			if(decl->variable->ty == TypeRegistry::Unknown || decl->variable->ty == TypeRegistry::Integer) {
				decl->variable->ty = TypeRegistry::Integer;
			} else if(decl->variable->ty != TypeRegistry::ArrayOfInt) {
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
	if(node.declaration) {
		// manual replacement of node type if declaration is a pointer
		// 	declare this_list := nil
		//	this_list := List(42, nil)
		if(node.declaration->get_node_type() == NodeType::Pointer or node.declaration->ty->get_type_kind() == TypeKind::Object
		or node.ty->get_type_kind() == TypeKind::Object) {
			auto pointer_ref = std::make_unique<NodePointerRef>(node.name, node.tok);
			pointer_ref->match_data_structure(node.declaration);
			match_type(pointer_ref.get(), &node);
			m_def_provider->remove_reference(node.declaration, &node);
			auto new_node = static_cast<NodeReference*>(node.replace_with(std::move(pointer_ref)));
			//m_def_provider->add_reference(node.declaration, new_node);
			new_node->accept(*this);
			return;
		}
	}
    match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
}

void TypeInference::visit(NodeVariable& node) {
	// because of pointer node -> apply type annotations again
	if(node.ty->get_type_kind() == TypeKind::Object) {
		auto ptr = node.to_pointer();
		auto references = m_def_provider->get_references(&node);
		auto new_node = static_cast<NodeDataStructure*>(node.replace_with(std::move(ptr)));
		for(auto ref : references) {
			ref->declaration = new_node;
		}
		m_def_provider->add_to_data_structures(new_node);
		if(auto strct = node.is_member()) {
			strct->update_member_table();
		}
		return;
	}
//	auto new_node = ASTVariableChecking::apply_type_annotations(&node);
	m_def_provider->add_to_data_structures(&node);
}

void TypeInference::visit(NodePointerRef& node) {
	// replace declaration node with Pointer if it is Variable
	if(node.declaration->get_node_type() == NodeType::Variable) {
		auto node_var = static_cast<NodeVariable*>(node.declaration);
		auto ptr = node_var->to_pointer();
		auto references = m_def_provider->get_references(node.declaration);
		auto new_node = static_cast<NodeDataStructure*>(node.declaration->replace_with(std::move(ptr)));
		for(auto ref : references) {
			ref->declaration = new_node;
			ASTSemanticAnalysis::replace_incorrectly_detected_reference(m_def_provider, ref);
		}
		new_node->accept(*this);
		if(auto strct = new_node->is_member()) {
			strct->update_member_table();
		}
	}

	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
	match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
}

void TypeInference::visit(NodePointer& node) {
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
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
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
}

void TypeInference::visit(NodeNDArray& node) {
    if(node.sizes) node.sizes->accept(*this);
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
		auto num_wildcards = node.num_wildcards();
		// not handed over as array element
		if(num_wildcards > 0) {
			node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, node.ty->get_element_type(), num_wildcards);
        // handed over as array element -> set to element type
		} else {
        	if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
		}
    }
    match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
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
	m_def_provider->add_reference(node.declaration, &node);
}

void TypeInference::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	node.update_member_table();
	node.update_method_table();
};

void TypeInference::visit(NodeMethodChain& node) {
	for(int i = 0; i<node.chain.size(); i++) {
		auto& ptr = node.chain[i];
		auto error = CompileError(ErrorType::SyntaxError, "", "", ptr->tok);
		if(i == 0) {
			ptr->accept(*this);
		} else {
			auto prev = i-1;
			auto prev_type = node.chain[prev]->ty;
			if(prev_type->get_type_kind() != TypeKind::Object) {
				error.m_message = "Method chaining can only be used on <Object> types.";
				error.exit();
			}
			auto prev_obj = prev_type->to_string();
			if(ptr->get_node_type() == NodeType::FunctionCall) {
				auto func_call = static_cast<NodeFunctionCall*>(ptr.get());
				func_call->function->name = prev_obj+"."+func_call->function->name;
				ptr->accept(*this);
				if(!func_call->definition) {
					error.m_message = "Method "+func_call->function->name+" does not exist.";
					error.exit();
				}
			} else {
				auto reference = cast_node<NodeReference>(ptr.get());
				auto strct = reference->get_object_ptr(m_program, prev_obj);
				if(!strct) {
					if(prev_type == TypeRegistry::Nil) {
						error.m_message = "Method chaining can not be used on <Nil> types.";
					} else {
						error.m_message = "Struct "+prev_obj+" does not exist.";
					}
					error.exit();
				}
				auto node_declaration = strct->get_member(prev_obj+"."+reference->name);
				if(!node_declaration) {
					error.m_message = "Member "+reference->name+" does not exist in "+prev_obj+".";
					error.exit();
				}
				reference->declaration = node_declaration;

				// if declaration of this reference is unknown and it is not the end of the chain,
				// we can assume that it is also an object. we can check if the next reference is also in this struct
				// and then cast this reference to the object of the last
				if(reference->ty == TypeRegistry::Unknown and i+1 < node.chain.size()) {
					auto& next = node.chain[i+1];
					if(auto next_ref = cast_node<NodeReference>(next.get())) {
						// next reference is also in this object
						auto next_obj = strct->get_member(prev_obj+"."+next_ref->name);
						if(next_obj) {
							reference->ty = prev_type;
						}
					}
				}

				reference->accept(*this);
			}
		}

	}
	match_type(&node, node.chain.back().get());

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
		node.variable->accept(*this);
	}
	m_def_provider->add_to_declarations(&node);

	// if declaration is pointer -> always initialize with nil!
	if(node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		if(!node.value) {
			auto nil = std::make_unique<NodeNil>(node.tok);
			nil->parent = &node;
			nil->accept(*this);
			if(node.variable->get_node_type() == NodeType::Variable) {
				node.value = std::move(nil);
			// pack into param list if declaration is array type
			} else {
				auto param_list = std::make_unique<NodeParamList>(node.tok, std::move(nil));
				param_list->parent = &node;
				node.value = std::move(param_list);
			}
		}
	}
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
	if(!node.definition) {
		// if definition pre lowering not found -> could be struct __init__ func
		// this_list := List(42, nil)
		if(auto obj_type = TypeRegistry::get_object_type(node.function->name)) {
			node.ty = obj_type;
			return;
		}
		return;
	}
	node.function->accept(*this);
	for(int i = 0; i < node.function->args->params.size(); i++) {
		match_type(node.function->args->params[i].get(), node.definition->header->args->params[i].get());
	}

	match_type(&node, node.definition);
	match_type(node.definition, &node);
}

void TypeInference::visit(NodeFunctionDefinition& node) {
    node.header->accept(*this);
    if(node.return_variable.has_value()) node.return_variable.value()->accept(*this);

    node.body->accept(*this);
    // get possible return type of node.definition by looking at the return param
    if(node.return_variable.has_value())
		match_type(&node, node.return_variable.value().get());
}

void TypeInference::visit(NodeReturn& node) {
	for(auto &ret : node.return_variables) {
		ret->accept(*this);
	}
	if(node.return_variables.size() >=1 ) {
		if(node.definition) match_type(node.definition, node.return_variables[0].get());
	}
}


void TypeInference::visit(NodeBinaryExpr& node) {
	node.left->accept(*this);
	node.right->accept(*this);

	// do not infer type if together in string
	if(contains(STRING_TOKENS, node.op)) {
		node.ty = TypeRegistry::String;
		return;
	}

	bool is_compatible = true;
	auto error = throw_type_error(node.left.get(), node.right.get());


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
