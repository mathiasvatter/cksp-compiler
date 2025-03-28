//
// Created by Mathias Vatter on 23.05.24.
//

#include "TypeInference.h"
#include "ASTSemanticAnalysis.h"

NodeAST * TypeInference::visit(NodeProgram& node) {
//	m_def_provider->refresh_data_vectors();
	m_def_provider->m_all_declarations.clear();
	m_def_provider->m_all_references.clear();
	m_def_provider->m_all_data_structures.clear();
	m_def_provider->m_all_assignments.clear();

	m_program->global_declarations->accept(*this);
	for(const auto & s : node.struct_definitions) {
		s->accept(*this);
	}
	for(const auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.reset_function_visited_flag();
	cast_data_structure_types(&node, true);

	for(const auto & s : node.struct_definitions) {
		s->collect_recursive_structs(m_program);
	}
	m_program->update_function_lookup();

	return &node;
}

void TypeInference::cast_data_structure_types(const NodeProgram* program, const bool cast) {
	const auto def_provider = program->def_provider;
	for(auto& refs : def_provider->get_all_data_structures()) {
		const auto data_struct = refs.lock();
		if(!data_struct) continue;
		for(auto & ref : data_struct->references) {
			match_reference_declaration(*ref, ref->get_declaration());
		}
	}
	for(auto& ass : def_provider->get_all_assignments()) {
		if(ass->l_value->ty->get_element_type() == TypeRegistry::Unknown ||
		ass->r_value->ty->get_element_type() == TypeRegistry::Unknown) {
			match_assignment_types(*ass->l_value, *ass->r_value);
		}
	}
    for(auto & decl : def_provider->get_all_declarations()) {
		if (decl->value) {
			match_assignment_types(*decl->variable, *decl->value);
		}
    	// cast as Integer if still unknown
    	if (cast) decl->variable->cast_type();
	}
	for(auto& refs : def_provider->get_all_data_structures()) {
		const auto data_struct = refs.lock();
		if(!data_struct) continue;
		for(auto & ref : data_struct->references) {
			match_reference_declaration(*ref, ref->get_declaration());
		}

	}
	def_provider->m_all_data_structures.clear();
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
		if(auto decl = constant->statement->cast<NodeSingleDeclaration>()) {
			if(decl->variable->ty == TypeRegistry::Unknown || decl->variable->ty == TypeRegistry::Integer) {
				decl->variable->ty = TypeRegistry::Integer;
			} else if(decl->variable->ty != TypeRegistry::ArrayOfInt) {
				auto error = throw_type_error(*decl->variable, node.ty);
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
	if(node.get_declaration()) {
		// manual replacement of node type if declaration is a pointer
		// 	declare this_list := nil
		//	this_list := List(42, nil)
		if(node.get_declaration()->get_node_type() == NodeType::Pointer or node.get_declaration()->ty->get_type_kind() == TypeKind::Object
		or node.ty->get_type_kind() == TypeKind::Object) {
			auto pointer_ref = node.to_pointer_ref();
			match_type(*pointer_ref, node);
			auto new_node = node.replace_reference(std::move(pointer_ref));
			return new_node->accept(*this);
		}
	}
    match_reference_declaration(node, node.get_declaration());
//	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeVariable& node) {
	// because of pointer node -> apply type annotations again
	if(node.ty->get_type_kind() == TypeKind::Object) {
		auto ptr = node.to_pointer();
		ptr->match_metadata(node.get_shared());
		auto new_node = node.replace_datastruct(std::move(ptr));
//		if(auto strct = node.is_member()) {
//			strct->rebuild_member_table();
//		}
		return new_node->accept(*this);
	}
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodePointerRef& node) {
	// replace declaration node with Pointer if it is Variable
	if(node.get_declaration()->get_node_type() == NodeType::Variable) {
		auto node_var = static_pointer_cast<NodeVariable>(node.get_declaration());
		auto ptr = node_var->to_pointer();
		ptr->match_metadata(node_var);
		auto new_node = node_var->replace_datastruct(std::move(ptr));
		auto &references = node.get_declaration()->references;
		for(auto ref : references) {
			ref->accept(*this);
//			ASTSemanticAnalysis::replace_incorrectly_detected_reference(m_program, ref);
		}
		new_node->accept(*this);
		node.get_declaration() = new_node->get_shared();
//		if(auto strct = new_node->is_member()) {
//			strct->rebuild_member_table();
//		}
	}

	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
	match_reference_declaration(node, node.get_declaration());
//	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodePointer& node) {
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::Nil;
	}
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodeArray& node) {
	// if array is unknown type -> set to array of unknown
	if(node.ty == TypeRegistry::Unknown) {
		node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, 1);
	}
    if(node.size) {
        node.size->accept(*this);
        node.size->ty = specialize_type(node.size->ty, TypeRegistry::Integer);
    }
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);
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
    match_reference_declaration(node, node.get_declaration());
//	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeNDArray& node) {
    if(node.sizes) node.sizes->accept(*this);
    // if array is unknown type -> set to array of unknown
    if(node.ty == TypeRegistry::Unknown) {
        node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions);
    }
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);
	if(node.sizes) node.sizes->accept(*this);
    // if handed over without index -> as whole array structure type
    if(!node.indexes) {
        if(node.ty == TypeRegistry::Unknown) {
			// in case sizes are not known -> get type from declaration -> if type is still unknown -> set array to dim size 0
			if(!node.sizes and node.get_declaration()) {
				node.ty = node.get_declaration()->ty;
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
			node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), num_wildcards);
        // handed over as array element -> set to element type
		} else {
        	if(node.ty->get_element_type()) node.ty = node.ty->get_element_type();
		}
    }
    match_reference_declaration(node, node.get_declaration());
//	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeList& node) {
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
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodeListRef& node) {
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
    match_reference_declaration(node, node.get_declaration());
//	m_def_provider->add_to_references(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	// needs to rebuild because has not been rebuilt since desugaring
	node.rebuild_member_table();
	node.rebuild_method_table();
	return &node;
}

NodeAST * TypeInference::visit(NodeNumElements& node) {
	node.array->accept(*this);
	match_against(*node.array, TypeRegistry::NDArrayOfUnknown, "<num_element> can only be used on <Composite> types like <Arrays> or <NDArrays>.");
	if(!node.array->ty->cast<CompositeType>()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.array->tok);
		error.m_message = "<num_elements> can only be used on <Composite> types like <Arrays> or <NDArrays>.";
		error.exit();
	}
	if(node.dimension) {
		node.dimension->accept(*this);
		match_against(*node.dimension, TypeRegistry::Integer);
	}
	match_against(node, TypeRegistry::Integer);
	return &node;
}

NodeAST * TypeInference::visit(NodePairs& node) {
	node.range->accept(*this);
	match_against(*node.range, TypeRegistry::NDArrayOfUnknown, "<pairs> can only be used on <Composite> types like <Arrays> or <NDArrays>.");
	match_against(node, TypeRegistry::add_composite_type(CompoundKind::Array, node.range->ty->get_element_type(), 2));
	return &node;
}

NodeAST * TypeInference::visit(NodeRange& node) {
	node.start->accept(*this);
	match_against(*node.start, TypeRegistry::Number);

	node.stop->accept(*this);
	match_against(*node.stop, TypeRegistry::Number);

	node.step->accept(*this);
	match_against(*node.step, TypeRegistry::Number);

	match_against(node, TypeRegistry::ArrayOfUnknown);
	return &node;
}

NodeAST * TypeInference::visit(NodeUseCount& node) {
	node.ref->accept(*this);
	if(!node.ref->ty->cast<ObjectType>()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.ref->tok);
		error.m_message = "<use_count> can only be used on <Object> types.";
		error.exit();
	}
	match_against(node, TypeRegistry::Integer);
	return &node;
};

NodeAST * TypeInference::visit(NodeSortSearch& node) {
	node.array->accept(*this);
	match_against(*node.array, TypeRegistry::NDArrayOfInt, "<search> can only be used on <Composite> types like <Arrays> or <NDArrays>.");
	if(!node.array->ty->cast<CompositeType>()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.array->tok);
		error.m_message = "<search> can only be used on <Composite> types like <Arrays> or <NDArrays>.";
		error.exit();
	}
	node.value->accept(*this);
	match_against(*node.value, TypeRegistry::Integer);
	if(node.from) {
		node.from->accept(*this);
		match_against(*node.from, TypeRegistry::Integer);
	}
	if(node.to) {
		node.to->accept(*this);
		match_against(*node.to, TypeRegistry::Integer);
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeForEach& node) {
	if(node.key) {
		node.key->accept(*this);
		match_against(*node.key->variable, TypeRegistry::Integer);
	}

	node.body->accept(*this);
	node.value->accept(*this);

	node.range->accept(*this);
	if(auto pairs = node.range->cast<NodePairs>()) {
		match_element_types(*pairs->range, *node.value->variable);
	} else {
		match_element_types(*node.range, *node.value->variable);
	}
	node.range->accept(*this);

	return &node;
};

NodeAST * TypeInference::visit(NodeAccessChain& node) {

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
			if(ptr->cast<NodeFunctionCall>()) {
				ptr->accept(*this);
			} else {
				auto reference = cast_node<NodeReference>(ptr.get());
				auto strct = reference->get_object_ptr(m_program, prev_obj);
				if(!strct) {
					if(prev_type == TypeRegistry::Nil) {
						error.m_message = "Method chaining can not be used on <Nil> types.";
					} else if(prev_ptr->cast<NodeFunctionCall>()) {
						error.m_message = prev_ptr->get_string()+" does not return <Object> type.";
					} else {
						error.m_message = "Struct "+prev_obj+" does not exist.";
					}
					error.exit();
				}
				auto node_declaration = strct->get_member(prev_obj+OBJ_DELIMITER+reference->name);
				// could be nullptr because refs in accessChain are not yet reference-collected
				if(!node_declaration) {
					// needs to be here because the struct member could have been replaced resulting in nullptr if
					// member is pointer and reference has not been reference-collected because of accessChain
					strct->rebuild_member_table();
					node_declaration = strct->get_member(prev_obj+OBJ_DELIMITER+reference->name);
				}
				if(!node_declaration) {
					error.m_message = "Member "+reference->name+" does not exist in "+prev_obj+".";
					error.exit();
				}
				reference->declaration = node_declaration;
				reference->collect_references();
				match_reference_declaration(*reference, reference->get_declaration());
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
	match_type(node, *node.chain.back());
	return &node;
}


NodeAST * TypeInference::visit(NodeParamList& node) {
    for(const auto & param : node.params) {
        param->accept(*this);
    }
	return &node;
}

NodeAST * TypeInference::visit(NodeInitializerList& node) {
	// check if in case we are in declaration or assignment, the list size is only 1, the l_value is of type composite
	// some_var := (3+4-5) <- would have still registered as initializer list
	if(node.size() == 1 and node.elem(0)->get_node_type() != NodeType::InitializerList) {
		if(auto decl = node.parent->cast<NodeSingleDeclaration>()) {
			if(decl->variable->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(auto assign = node.parent->cast<NodeSingleAssignment>()) {
			if(assign->l_value->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(auto ret = node.parent->cast<NodeReturn>()) {
			if(ret->get_definition() and ret->get_definition()->ty->get_type_kind() != TypeKind::Composite) {
				return node.replace_with(std::move(node.elem(0)))->accept(*this);
			}
		} else if(auto set = node.parent->cast<NodeSetControl>()) {
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
		match_assignment_types(*node.variable, *node.value);
		node.variable->accept(*this);
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeSingleDelete& node) {
	node.ptr->accept(*this);
	if(node.ptr->ty->get_element_type()->get_type_kind() != TypeKind::Object) {
		auto error = CompileError(ErrorType::InternalError, "", "", node.ptr->tok);
		error.m_message = "Delete can only be used with <Object> types.";
		error.exit();
	}
	node.num->accept(*this);
	match_type(node, *node.ptr);
	return &node;
}

NodeAST * TypeInference::visit(NodeSingleRetain& node) {
	node.ptr->accept(*this);
	if(node.ptr->ty->get_element_type()->get_type_kind() != TypeKind::Object) {
		auto error = CompileError(ErrorType::InternalError, "", "", node.ptr->tok);
		error.m_message = "Retain can only be used with <Object> types.";
		error.exit();
	}
	node.num->accept(*this);
	match_type(node, *node.ptr);
	return &node;
}


NodeAST * TypeInference::visit(NodeSingleDeclaration& node) {
	node.variable->accept(*this);
	if(node.value) {
		node.value->accept(*this);
		match_assignment_types(*node.variable, *node.value);
		node.variable->accept(*this);
	}
	m_def_provider->add_to_declarations(&node);

	// if declaration is pointer -> always initialize with nil!
	if(node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
		if(!node.value) {
			node.set_value(std::make_unique<NodeNil>(node.tok));
			node.value->accept(*this);
			// wrap nil in initializer list if variable is of composite type
			if(node.variable->ty->get_type_kind() == TypeKind::Composite) {
				return node.value
				->replace_with(std::make_unique<NodeInitializerList>(node.tok, std::make_unique<NodeNil>(node.tok)))
				->accept(*this);
			}
			return &node;
		}
	}

	return &node;
}

NodeAST * TypeInference::visit(NodeUIControl& node) {
	// check if type is same as provided as builtin
	auto declaration = node.get_declaration();
	if(node.ty != TypeRegistry::Unknown and node.ty->get_element_type() != declaration->control_var->ty->get_element_type()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Type Annotation of <UI Control> "+node.name+" does not match expected type: "+declaration->control_var->ty->to_string()+".";
		error.m_got = node.ty->to_string();
		error.exit();
	}

	node.control_var->accept(*this);

	// only to matching if node types are the same (because of ui control arrays)
	// eg. declare ui_label %lbl_sdf[3,1](1,1) -> $lbl_sdf
	if(node.control_var->get_node_type() == declaration->control_var->get_node_type()) {
		match_type(*node.control_var, *declaration->control_var);
	// in case of ui_arrays -> match against element type of declaration
	} else if(node.is_ui_control_array()) {
		if(node.control_var->ty->get_element_type()->is_compatible(declaration->control_var->ty->get_element_type())) {
			node.control_var->set_element_type(declaration->control_var->ty->get_element_type());
		} else {
			// provoke type error
			throw_type_error(*node.control_var, declaration->control_var->ty).exit();
		}
	}

	for(int i = 0; i < node.params->size(); i++) {
		match_type(*node.params->param(i), *declaration->params->param(i));
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
	match_assignment_types(node, *node.value);
	node.ui_id->accept(*this);
	node.value->accept(*this);
	return &node;
}

NodeAST * TypeInference::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	node.r_value->accept(*this);

	match_assignment_types(*node.l_value, *node.r_value);

	// a second time to get the new types to the declaration pointer!
	node.l_value->accept(*this);
	node.r_value->accept(*this);

	// check if l_value is a constant or function parameter
	if(auto declaration = node.l_value->get_declaration()) {
		if(declaration->data_type == DataType::Const) {
			auto error = get_raw_compile_error(ErrorType::VariableError, node);
			error.m_message = "<"+node.l_value->tok.val + "> was declared as a constant. Cannot assign to a constant variable.";
			error.exit();
		}
//		if(declaration->is_function_param() and declaration->data_type != DataType::Return) {
//			// specific array indexes are allowed if declaration is array
//			if(not(declaration->ty->cast<CompositeType>() and !node.l_value->ty->cast<CompositeType>())) {
//				auto error = get_raw_compile_error(ErrorType::VariableError, node);
//				error.m_message = "<"+node.l_value->tok.val + "> is a function parameter. Cannot assign to an immutable parameter.";
//				error.exit();
//			}
//		}
		if(node.l_value->ty->cast<FunctionType>()) {
			auto error = get_raw_compile_error(ErrorType::VariableError, node);
			error.m_message = "Cannot assign to a function.";
			error.exit();
		}
	}

	m_def_provider->add_to_assignments(&node);
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionCall& node) {
	node.bind_definition(m_program);
	node.function->accept(*this);

	if(node.is_destructive_builtin_func()) {
		if(node.function->get_arg(0)->is_constant()) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Destructive functions require a variable as an argument, but an immutable argument was given.";
			error.m_got = node.function->get_arg(0)->tok.val;
			error.exit();
		}
	}

	auto definition = node.get_definition();
	if(!definition) {
		// if definition pre lowering not found -> could be struct __init__ func
		// this_list := List(42, nil)
		if(auto obj_type = TypeRegistry::get_object_type(node.function->name)) {
			node.ty = obj_type;
			return &node;
		}
	}

	if(definition) {
		if (!definition->visited and !node.is_builtin_kind()) {
			definition->accept(*this);
			definition->visited = true;
		}

		for (int i = 0; i < node.function->get_num_args(); i++) {
			auto &func_arg = node.function->get_arg(i);
			auto &param = definition->get_param(i);
			const std::string error_message =
				"Found incorrect type in <Function Call>. Function <" + node.function->name + "> expects "
					+ param->ty->to_string() + " as argument type.";
			match_type(*func_arg, *param, error_message);
			// infer formal param type only if function is no builtin function
			// this throws errors with the-pulse
			if (!node.is_builtin_kind()) {
				const std::string error_message2 =
				"Found incorrect type in <Function Call>. Function <" + node.function->name + "> expects "
					+ func_arg->ty->to_string() + " as argument type.";
				match_parameters(*param, func_arg->ty, error_message2);
			}
		}

		match_type(node, *definition);
	}
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionHeaderRef& node) {
	if(node.args) node.args->accept(*this);
	// node.create_function_type(TypeRegistry::Unknown);
	// if declaration type has empty params -> get type from reference
	if(auto decl = node.get_declaration()) {
		auto decl_type = decl->ty->cast<FunctionType>();
		auto ref_type = node.ty->cast<FunctionType>();
		if(!decl_type) {
			decl->accept(*this);
		}
		decl_type = decl->ty->cast<FunctionType>();
		if(!decl_type) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Function type expected.";
			error.exit();
		} else if (decl_type->get_params().empty() and ref_type) {
			decl->ty = node.ty;
		}
		if(!ref_type) {
//			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
//			error.m_message = "Function type expected.";
//			error.exit();
			node.ty = decl->ty;
		} else if (ref_type->get_params().empty()) {
			node.ty = decl->ty;
		}

	}

	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionHeader& node) {
	for(auto &param : node.params) param->accept(*this);

	if(node.ty == TypeRegistry::Unknown) {
		node.create_function_type();
	}
	if(!node.ty->cast<FunctionType>()) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Function type expected.";
		error.exit();
	}
	m_def_provider->add_to_data_structures(node.weak_from_this());
	return &node;
}

NodeAST * TypeInference::visit(NodeFunctionDefinition& node) {
	m_program->function_call_stack.push(node.weak_from_this());

	// add data structures to provider
//	m_def_provider->add_to_data_structures(node.header);
    node.header->accept(*this);
    if(node.return_variable.has_value()) {
		node.return_variable.value()->accept(*this);
	}
    node.body->accept(*this);
	node.header->accept(*this);

	auto header_type = node.header->ty->cast<FunctionType>();
	if(!header_type) {
		auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
		error.m_message = "Function type expected.";
		error.exit();
	}
	node.set_element_type(specialize_type(node.ty, header_type->get_return_type()));
	node.header->ty = TypeRegistry::add_function_type(header_type->get_params(), node.ty);

	// try to update def type with return var type
	if(node.return_variable.has_value()) {
		match_type(node, *node.return_variable.value());
	}
	m_program->function_call_stack.pop();
	return &node;
}

NodeAST * TypeInference::visit(NodeReturn& node) {
	if(!node.get_definition()) node.definition = m_program->get_curr_function();
	for(auto &ret : node.return_variables) {
		ret->accept(*this);
	}
	if(!node.return_variables.empty() ) {
		if(node.get_definition())
			match_type(*node.get_definition(), *node.return_variables[0]);
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
			match_type(*node.right, *def->header->get_param(1), "Second argument of overloaded operator does not match expected type.");
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
	auto error = throw_type_error(*node.left, node.right->ty);

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
	auto error = throw_type_error(*node.operand, node.ty);
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
