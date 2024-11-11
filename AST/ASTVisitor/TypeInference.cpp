//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeInference.h"
#include "ASTSemanticAnalysis.h"

NodeAST * TypeInference::visit(NodeProgram& node) {
	m_program = &node;
//	m_def_provider->refresh_data_vectors();
	m_def_provider->m_all_declarations.clear();
	m_def_provider->m_all_references.clear();
	m_def_provider->m_all_data_structures.clear();
	m_def_provider->m_all_assignments.clear();

	m_program->global_declarations->accept(*this);
	for(auto & s : node.struct_definitions) {
		s->accept(*this);
	}
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.reset_function_visited_flag();

	return &node;
}

void TypeInference::cast_data_structure_types(DefinitionProvider* def_provider, bool cast) {
//	std::cout << "TypeInference::match_reference_declaration" << std::endl;
	for(auto& refs : def_provider->m_references_per_data_structure) {
//		std::cout << refs.first->name << std::endl;
		for(auto & ref : refs.second) {
			match_reference_declaration(ref);
		}
	}
//	std::cout << "TypeInference::match_assignment_types" << std::endl;
	for(auto& ass : def_provider->get_all_assignments()) {
		if(ass->l_value->ty->get_element_type() == TypeRegistry::Unknown ||
		ass->r_value->ty->get_element_type() == TypeRegistry::Unknown) {
			match_assignment_types(ass->l_value.get(), ass->r_value.get());
		}
	}
//	std::cout << "TypeInference::match_assignment_types" << std::endl;
    for(auto & decl : def_provider->get_all_declarations()) {
		if (decl->value) {
			match_assignment_types(decl->variable.get(), decl->value.get());
		}
		// cast as Integer if still unknown
		if (cast) decl->variable->cast_type();
	}
//	std::cout << "TypeInference::match_reference_declaration" << std::endl;
	for(auto& refs : def_provider->m_references_per_data_structure) {
		for(auto & ref : refs.second) {
			match_reference_declaration(ref);
		}
	}
}

NodeAST * TypeInference::visit(NodeCallback& node) {
    if(node.callback_id) node.callback_id->accept(*this);
    node.statements->accept(*this);
	return &node;
}


NodeAST * TypeInference::visit(NodeInt& node) {
    node.ty = TypeRegistry::Integer;
	return &node;
}

NodeAST * TypeInference::visit(NodeString& node) {
    node.ty = TypeRegistry::String;
	return &node;
}

NodeAST * TypeInference::visit(NodeReal& node) {
    node.ty = TypeRegistry::Real;
	return &node;
}

NodeAST * TypeInference::visit(NodeNil& node) {
	node.ty = TypeRegistry::Nil;
	return &node;
}

NodeAST * TypeInference::visit(NodeConst& node) {
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
	return &node;
}

NodeAST * TypeInference::visit(NodeVariableRef& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	if(node.declaration) {
		// manual replacement of node type if declaration is a pointer
		// 	declare this_list := nil
		//	this_list := List(42, nil)
		if(node.declaration->get_node_type() == NodeType::Pointer or node.declaration->ty->get_type_kind() == TypeKind::Object
		or node.ty->get_type_kind() == TypeKind::Object) {
			auto pointer_ref = node.to_pointer_ref();
			match_type(pointer_ref.get(), &node);
//			pointer_ref->match_data_structure(node.declaration);
//			m_def_provider->remove_reference(node.declaration, &node);
//			auto new_node = static_cast<NodeReference*>(node.replace_with(std::move(pointer_ref)));
//			m_def_provider->add_reference(node.declaration, new_node);
			auto new_node = node.replace_reference(std::move(pointer_ref), m_def_provider);
			return new_node->accept(*this);
		}
	}
    match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
	return &node;
}

NodeAST * TypeInference::visit(NodeVariable& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	// because of pointer node -> apply type annotations again
	if(node.ty->get_type_kind() == TypeKind::Object) {
		auto ptr = node.to_pointer();
		auto new_node = node.replace_datastruct(std::move(ptr), m_def_provider);
		m_def_provider->add_to_data_structures(new_node->get_shared());
		if(auto strct = node.is_member()) {
			strct->update_member_table();
		}
		return new_node;
	}
	m_def_provider->add_to_data_structures(node.get_shared());
	return &node;
}

NodeAST * TypeInference::visit(NodePointerRef& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	// replace declaration node with Pointer if it is Variable
	if(node.declaration->get_node_type() == NodeType::Variable) {
		auto node_var = static_pointer_cast<NodeVariable>(node.declaration);
		auto ptr = node_var->to_pointer();
		ptr->match_metadata(node_var);
		auto new_node = node_var->replace_datastruct(std::move(ptr), m_def_provider);
		auto &references = m_def_provider->get_references(node.declaration);
		for(auto ref : references) {
			ASTSemanticAnalysis::replace_incorrectly_detected_reference(m_def_provider, ref);
		}
//		new_node->accept(*this);
		node.declaration = new_node->get_shared();
		if(auto strct = new_node->is_member()) {
			strct->update_member_table();
		}
//		return new_node;
	}

	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
	match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
	return &node;
}

NodeAST * TypeInference::visit(NodePointer& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
	m_def_provider->add_to_data_structures(node.get_shared());
	return &node;
}

NodeAST * TypeInference::visit(NodeArray& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	// if array is unknown type -> set to array of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, 1);
	}
    if(node.size) {
        node.size->accept(*this);
        node.size->ty = specialize_type(node.size->ty, TypeRegistry::Integer);
    }
	m_def_provider->add_to_data_structures(node.get_shared());
	return &node;
}

NodeAST * TypeInference::visit(NodeArrayRef& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	if(node.index) node.index->accept(*this);
//	if(node.declaration) node.declaration->accept(*this);
	// if handed over without index -> as whole array structure type
	if(!node.index) {
		if(node.ty == TypeRegistry::Unknown) {
			node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
            if(!node.ty) throw_composite_error(&node).exit();
		}
	} else {
		if(node.index->get_node_type() == NodeType::Wildcard) {
			node.ty = TypeRegistry::get_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		} else {
			// not handed over as array element
			// handed over as array element -> set to element type
			if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
		}
	}
    match_reference_declaration(&node);
	m_def_provider->add_to_references(&node);
	m_def_provider->add_reference(node.declaration, &node);
	return &node;
}

NodeAST * TypeInference::visit(NodeNDArray& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
    if(node.sizes) node.sizes->accept(*this);
    // if array is unknown type -> set to array of unknown
    if(node.ty == TypeRegistry::Unknown) {
        node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions);
    }
	m_def_provider->add_to_data_structures(node.get_shared());
	return &node;
}

NodeAST * TypeInference::visit(NodeNDArrayRef& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	if(node.indexes) node.indexes->accept(*this);
	if(node.sizes) node.sizes->accept(*this);
//	if(node.declaration) node.declaration->accept(*this);
    // if handed over without index -> as whole array structure type
    if(!node.indexes) {
        if(node.ty == TypeRegistry::Unknown) {
			// in case sizes are not known -> get type from declaration -> if type is still unknown -> set array to dim size 0
			if(!node.sizes and node.declaration) {
				node.ty = node.declaration->ty;
			}
			if(node.ty == TypeRegistry::Unknown) {
				int dim = node.sizes ? node.sizes->params.size() : 0;
				node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), dim);
			}
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
	return &node;
}

NodeAST * TypeInference::visit(NodeList& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
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
	m_def_provider->add_to_data_structures(node.get_shared());
	return &node;
}

NodeAST * TypeInference::visit(NodeListRef& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	if(node.indexes) node.indexes->accept(*this);
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
	return &node;
}

NodeAST * TypeInference::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	node.update_member_table();
	node.update_method_table();
	return &node;
}

NodeAST * TypeInference::visit(NodeNumElements& node) {
	node.array->accept(*this);
	if(node.array->ty->get_type_kind() != TypeKind::Composite) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "<num_elements> can only be used on <Composite> types like <Arrays> or <NDArrays>.";
		error.exit();
	}
	if(node.dimension) node.dimension->accept(*this);
	node.ty = TypeRegistry::Integer;
	return &node;
}

NodeAST * TypeInference::visit(NodeAccessChain& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.name << ", " << node.tok.line << std::endl;
	// in case an array size constant has been mistakenly converted to an access chain
//	if(auto size_const = node.is_size_constant()) {
//		// chain[0] was already written into reference map
//		auto nd_arr_ref = static_cast<NodeReference*>(node.chain[0].get());
//		m_def_provider->remove_reference(nd_arr_ref->declaration, nd_arr_ref);
//		return node.replace_with(std::move(size_const));
//	}

	for(int i = 0; i<node.chain.size(); i++) {
		auto& ptr = node.chain[i];
		auto error = CompileError(ErrorType::SyntaxError, "", "", ptr->tok);
		if(i == 0) {
			ptr->accept(*this);
		} else {
			auto prev = i-1;
			auto prev_ptr = node.chain[prev].get();
			auto prev_type = node.chain[prev]->ty;
			if(prev_type->get_type_kind() != TypeKind::Object) {
				error.m_message = "Method chaining can only be used on <Object> types.";
				error.exit();
			}
			auto prev_obj = prev_type->to_string();
			if(ptr->get_node_type() == NodeType::FunctionCall) {
//				auto func_call = static_cast<NodeFunctionCall*>(ptr.get());
//				func_call->function->name = prev_obj+"."+func_call->function->name;
				ptr->accept(*this);
//				if(!func_call->definition) {
//					error.m_message = "Method "+func_call->function->name+" does not exist.";
//					error.exit();
//				}
			} else {
				auto reference = cast_node<NodeReference>(ptr.get());
				auto strct = reference->get_object_ptr(m_program, prev_obj);
				if(!strct) {
					if(prev_type == TypeRegistry::Nil) {
						error.m_message = "Method chaining can not be used on <Nil> types.";
					} else if(prev_ptr->get_node_type() == NodeType::FunctionCall) {
						error.m_message = prev_ptr->get_string()+" does not return <Object> type.";
					} else {
						error.m_message = "Struct "+prev_obj+" does not exist.";
					}
					error.exit();
				}
				strct->update_member_table();
				auto node_declaration = strct->get_member(prev_obj+OBJ_DELIMITER+reference->name);
				if(!node_declaration) {
					error.m_message = "Member "+reference->name+" does not exist in "+prev_obj+".";
					error.exit();
				}
				reference->declaration = node_declaration;
				match_reference_declaration(reference);
				// if declaration of this reference is unknown and it is not the end of the chain,
				// we can assume that it is also an object. we can check if the next reference is also in this struct
				// and then cast this reference to the object of the last
				if(reference->ty == TypeRegistry::Unknown and i+1 < node.chain.size()) {
					auto& next = node.chain[i+1];
					if(auto next_ref = cast_node<NodeReference>(next.get())) {
						// next reference is also in this object
						auto next_obj = strct->get_member(prev_obj+OBJ_DELIMITER+next_ref->name);
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
	return &node;
}


NodeAST * TypeInference::visit(NodeParamList& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.tok.line << std::endl;
//    std::vector<Type*> types;
    for(const auto & param : node.params) {
        param->accept(*this);
//        types.push_back(param->ty);
    }
    // enforce same type for every member only if in array declaration, assignment, liststruct, return stmt
//    auto node_declaration = node.parent->get_node_type() == NodeType::SingleDeclaration;
//    auto node_assignment = node.parent->get_node_type() == NodeType::SingleAssignment;
//	auto node_list_struct = node.parent->get_node_type() == NodeType::List;
//	auto node_return = node.parent->get_node_type() == NodeType::Return;
//	auto node_function_init = node.parent->get_node_type() == NodeType::ParamList and
//		node.parent->parent->get_node_type() == NodeType::FunctionHeader;
//    if(!node_list_struct and !node_return and !node_function_init) return &node;
//
//    node.ty = infer_initialization_types(types, &node);
	return &node;
}

NodeAST * TypeInference::visit(NodeInitializerList& node) {
	// if type was already inferred -> return
//	if(node.ty != TypeRegistry::Unknown) {
//		return &node;
//	}

	// check if in case we are in declaration or assignment, the list size is only 1, the l_value is of type composite
	// some_var := (3+4-5) <- would have still registered as initializer list
	if(node.size() == 1 and node.elem(0)->get_node_type() != NodeType::InitializerList) {
		if(node.parent->get_node_type() == NodeType::SingleDeclaration) {
			auto decl = static_cast<NodeSingleDeclaration*>(node.parent);
			if(decl->variable->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(node.parent->get_node_type() == NodeType::SingleAssignment) {
			auto assign = static_cast<NodeSingleAssignment*>(node.parent);
			if(assign->l_value->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(node.parent->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(node.parent);
			if(ret->definition and ret->definition->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(node.parent->get_node_type() == NodeType::SetControl) {
			auto set = static_cast<NodeSetControl*>(node.parent);
			if(set->value->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		}

	}

	std::vector<Type*> types;
	for(const auto & param : node.elements) {
		param->accept(*this);
		types.push_back(param->ty);
	}
	// enforce same type for every member
	auto element_type = infer_initialization_types(types, &node);

	// only add composite type if we are at the topmost level
	if(node.parent->get_node_type() == NodeType::InitializerList or node.parent->get_node_type() == NodeType::List) {
		node.ty = element_type;
	} else {
		auto dims = node.get_dimensions();
		// if recognized multiple dimensions like ((1,2), (2,3)) -> set dimensions, otherwise set to 0
		int dimensions = dims.size() > 1 ? dims.size() : 0;
		node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, element_type, dimensions);
	}

	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionParam& node) {
	node.variable->accept(*this);
	if(node.value) {
		node.value->accept(*this);
		match_assignment_types(node.variable.get(), node.value.get());
		node.variable->accept(*this);
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeSingleDelete& node) {
	node.ptr->accept(*this);
	node.num->accept(*this);
	match_type(&node, node.ptr.get());
	return &node;
}


NodeAST * TypeInference::visit(NodeSingleDeclaration& node) {
	node.variable->accept(*this);
	if(node.value) {
		node.value->accept(*this);
		// cast node r_value to composite type if variable is composite type
//		if(node.value ->get_node_type() == NodeType::ParamList and node.variable->ty->get_type_kind() == TypeKind::Composite) {
//			node.value->ty = TypeRegistry::add_composite_type(static_cast<CompositeType*>(node.variable->ty)->get_compound_type(), node.value->ty->get_element_type(), node.variable->ty->get_dimensions());
//		}
		match_assignment_types(node.variable.get(), node.value.get());
		node.variable->accept(*this);
	}
	m_def_provider->add_to_declarations(&node);

	// if declaration is pointer -> always initialize with nil!
	if(node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		if(!node.value) {
			auto nil = std::make_unique<NodeNil>(node.tok);
			nil->parent = &node;
			node.value = std::move(nil);
			node.value->accept(*this);
			// wrap nil in initializer list if variable is of composite type
			if(node.variable->ty->get_type_kind() == TypeKind::Composite) {
				return node.value
				->replace_with(std::make_unique<NodeInitializerList>(node.tok, std::move(nil)))
				->accept(*this);
			}
			return &node;
		}
	}

	return &node;
}

NodeAST * TypeInference::visit(NodeUIControl& node) {
	// check if type is same as provided as builtin
	if(node.ty != TypeRegistry::Unknown and node.ty->get_element_type() != node.declaration->control_var->ty->get_element_type()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Type Annotation of <UI Control> "+node.name+" does not match expected type: "+node.declaration->control_var->ty->to_string()+".";
		error.m_got = node.ty->to_string();
		error.exit();
	}

	node.control_var->accept(*this);

	// only to matching if node types are the same (because of ui control arrays)
	// eg. declare ui_label %lbl_sdf[3,1](1,1) -> $lbl_sdf
	if(node.control_var->get_node_type() == node.declaration->control_var->get_node_type()) {
		match_type(node.control_var.get(), node.declaration->control_var.get());
	// in case of ui_arrays -> match against element type of declaration
	} else if(node.is_ui_control_array()) {
		if(node.control_var->ty->get_element_type()->is_compatible(node.declaration->control_var->ty->get_element_type())) {
			node.control_var->set_element_type(node.declaration->control_var->ty->get_element_type());
		} else {
			// provoke type error
			throw_type_error(node.control_var.get(), node.declaration->control_var.get()).exit();
		}
	}

	for(int i = 0; i < node.params->size(); i++) {
		match_type(node.params->param(i).get(), node.declaration->params->param(i).get());
	}
	node.params->accept(*this);
	return &node;
}

NodeAST * TypeInference::visit(NodeGetControl& node) {
	node.ui_id->accept(*this);
	node.ty = node.get_control_type();
	return &node;
}

NodeAST * TypeInference::visit(NodeSetControl& node) {
	node.ui_id->accept(*this);
	node.value->accept(*this);
	match_assignment_types(&node, node.value.get());
	node.ui_id->accept(*this);
	node.value->accept(*this);
	return &node;
}

NodeAST * TypeInference::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	node.r_value->accept(*this);

	match_assignment_types(node.l_value.get(), node.r_value.get());

	// a second time to get the new types to the declaration pointer!
	node.l_value->accept(*this);
	node.r_value->accept(*this);

	m_def_provider->add_to_assignments(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionCall& node) {
//	std::cout << __PRETTY_FUNCTION__ << ", " << node.function->name << ", " << node.tok.line << std::endl;
	node.get_definition(m_program);
	node.function->accept(*this);
	if(!node.definition) {
		// if definition pre lowering not found -> could be struct __init__ func
		// this_list := List(42, nil)
		if(auto obj_type = TypeRegistry::get_object_type(node.function->name)) {
			node.ty = obj_type;
			return &node;
		}
//		return &node;
	}

	if(node.definition) {
		if (!node.definition->visited) node.definition->accept(*this);
		for (int i = 0; i < node.function->get_num_args(); i++) {
			auto &func_arg = node.function->get_arg(i);
			auto &param = node.definition->get_param(i);
			const std::string error_message =
				"Found incorrect type in <Function Call>. Function <" + node.function->name + "> expects "
					+ param->ty->to_string() + " as argument type.";
			match_type(func_arg.get(), param.get(), error_message);
		}
	}
	if(node.definition) {
		match_type(&node, node.definition);
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionHeaderRef& node) {
	if(node.args) node.args->accept(*this);

	// if declaration type has empty params -> get type from reference
	if(node.declaration) {
		auto decl_type = static_cast<FunctionType *>(node.declaration->ty);
		auto ref_type = static_cast<FunctionType *>(node.ty);
		if (decl_type->get_params().empty()) {
			node.declaration->ty = node.ty;
		} else if (ref_type->get_params().empty()) {
			node.ty = node.declaration->ty;
		}
	}

//	match_reference_declaration(&node);
//	match_type(&node, node.header.get());
//	match_reference_declaration(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionHeader& node) {
	for(auto &param : node.params) param->accept(*this);

	if(node.ty == TypeRegistry::Unknown) {
		node.create_function_type();
	}
	if(node.ty->get_type_kind() != TypeKind::Function) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Function type expected.";
		error.exit();
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionDefinition& node) {
	node.visited = true;
	m_program->function_call_stack.push(&node);

	// add data structures to provider
	m_def_provider->add_to_data_structures(node.header);
    node.header->accept(*this);
    if(node.return_variable.has_value()) {
		node.return_variable.value()->accept(*this);
	}
    node.body->accept(*this);
	node.header->accept(*this);


	auto header_type = static_cast<FunctionType*>(node.header->ty);
	node.set_element_type(specialize_type(node.ty, header_type->get_return_type()));
	node.header->ty = TypeRegistry::add_function_type(header_type->get_params(), node.ty);

	// try to update def type with return var type
	if(node.return_variable.has_value()) {
		match_type(&node, node.return_variable.value().get());
	}
	m_program->function_call_stack.pop();
	return &node;
}

NodeAST * TypeInference::visit(NodeReturn& node) {
	if(!node.definition) node.definition = m_program->function_call_stack.top();
	for(auto &ret : node.return_variables) {
		ret->accept(*this);
	}
	if(!node.return_variables.empty() ) {
		if(node.definition)
			match_type(node.definition, node.return_variables[0].get());
	}
	// a second time to get the new types to the declaration pointer!
	for(auto &ret : node.return_variables) {
		ret->accept(*this);
	}
	return &node;
}


NodeAST * TypeInference::visit(NodeBinaryExpr& node) {
	node.left->accept(*this);
	node.right->accept(*this);

	// if type if object -> check for operator overloading
	if(node.left->ty->get_type_kind() == TypeKind::Object) {
		auto strct = NodeReference::get_object_ptr(m_program, node.left->ty->to_string());
		if(auto def = strct->get_overloaded_method(node.op)) {
			match_type(node.right.get(), def->header->get_param(1).get(), "Second argument of overloaded operator does not match expected type.");
			auto call = std::make_unique<NodeFunctionCall>(
				false,
				std::make_unique<NodeFunctionHeaderRef>(
					def->header->name,
					std::make_unique<NodeParamList>(node.left->tok, std::move(node.left), std::move(node.right)),
					node.tok
				),
				node.tok
			);
			// do not yet add definition to call -> will be done in function call
			return node.replace_with(std::move(call))->accept(*this);
		} else {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Operator <"+std::string(tokenStrings[(int)node.op]) +"> has not been overloaded for type "+node.left->ty->to_string()+".";
			error.exit();
		}

	}

	// do not infer type if together in string
	if(contains(STRING_TOKENS, node.op)) {
		node.ty = TypeRegistry::String;
		return &node;
	}

	bool is_compatible = true;
	auto error = throw_type_error(node.left.get(), node.right.get());

	node.left->ty = specialize_type(node.left->ty, node.right->ty);
	node.right->ty = specialize_type(node.right->ty, node.left->ty);

	// check type of this node by looking at operator and node.left and node.right
	if (contains(MATH_TOKENS, node.op)) {
		// can only be int op int || float op float
		is_compatible = node.left->ty->is_compatible(TypeRegistry::Integer) and node.right->ty->is_compatible(TypeRegistry::Integer);
		if(node.left->ty->get_element_type() == TypeRegistry::String || node.right->ty->get_element_type() == TypeRegistry::String) {
			error.m_message += "<Mathematical Operators> can only be used in between <Integer> or <Real> values.";
			error.exit();
		}
		if(is_compatible) node.ty = TypeRegistry::Number;
		if(node.left->ty == TypeRegistry::Integer and node.right->ty == TypeRegistry::Integer) {
			node.ty = TypeRegistry::Integer;
		} else if(node.left->ty == TypeRegistry::Real and node.right->ty == TypeRegistry::Real) {
			node.ty = TypeRegistry::Real;
		}
		if(node.left->ty != node.right->ty)
			is_compatible = false;
//		is_compatible |= node.left->ty->is_compatible(TypeRegistry::Real) and node.right->ty->is_compatible(TypeRegistry::Real);
//		if(is_compatible and node.ty != TypeRegistry::Integer) node.ty = TypeRegistry::Real;
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

	if(!is_compatible) {
        error.exit();
	}
	node.left->set_element_type(specialize_type(node.left->ty, node.ty));
	node.right->set_element_type(specialize_type(node.right->ty, node.ty));
	return &node;
}

NodeAST * TypeInference::visit(NodeUnaryExpr& node) {
	node.operand->accept(*this);

	bool is_compatible = node.ty->is_compatible(node.operand->ty) && node.operand->ty->is_compatible(node.ty);
	auto error = throw_type_error(node.operand.get(), &node);
	if(!is_compatible) error.exit();

	if(node.op == token::SUB) {
		// can only be int or float
		is_compatible = node.operand->ty->is_compatible(TypeRegistry::Integer);
		if(is_compatible) node.ty = TypeRegistry::Number;
		is_compatible |= node.operand->ty->is_compatible(TypeRegistry::Real);
		if(is_compatible and node.operand->ty == TypeRegistry::Integer) {
			node.ty = TypeRegistry::Integer;
		} else if(is_compatible and node.operand->ty == TypeRegistry::Real) {
			node.ty = TypeRegistry::Real;
		}
		error.m_message += "Please use real() and int() to use <Real> and <Integer> numbers in a single expression.";
	} else if (node.op == token::BIT_NOT) {
		node.ty = TypeRegistry::Integer;
		is_compatible = node.operand->ty->is_compatible(node.ty);
	} else if(node.op == token::BOOL_NOT) {
		node.ty = TypeRegistry::Boolean;
		is_compatible = node.operand->ty->is_compatible(node.ty);
		error.m_message += "<Bool Operators> can only be used in between <Boolean> or <Comparison> values. Be sure to use correct parentheses.";
	} else {
		error.exit();
	}

	if(!is_compatible)
		error.exit();
	node.operand->ty = specialize_type(node.operand->ty, node.ty);
	return &node;
}
