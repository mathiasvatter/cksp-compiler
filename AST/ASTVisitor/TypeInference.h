//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "ReferenceManagement/ASTCollectDeclarations.h"
#include <chrono>

class TypeInference final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	std::vector<NodeFunctionCall*> m_func_calls;

	void do_monomorphization() {
		// copy m_func_calls to avoid modification during iteration (especially when visiting nodes again
		// and collecting the same function call twice!!)
		std::vector<NodeFunctionCall*> calls = m_func_calls;
		for (const auto& call : calls) {
			if (call->kind != NodeFunctionCall::Kind::UserDefined) continue;
			auto const def = call->get_definition();
			if (!def) continue;
			int method_idx = call->is_in_access_chain() ? 1 : 0;
			for (int i = 0; i < call->function->get_num_args(); i++) {
				auto& func_arg = call->function->get_arg(i);
				auto& param = def->get_param(i+method_idx);
				// infer formal param type only if function is no builtin function
				// this throws errors with the-pulse
				// const std::string error_message2 =
				// "Found incorrect type in <Function Call>. Function <" + node.function->name + "> expects "
				// 	+ func_arg->ty->to_string() + " as argument type.";
				match_parameters(*param, func_arg->ty, "");
				for (auto& ref : param->references) {
					match_reference_declaration(*ref, param);
				}
			}
		}

		m_program->reset_function_visited_flag();

		for (const auto& call : calls) {
			if (call->kind != NodeFunctionCall::Kind::UserDefined) continue;
			auto const def = call->get_definition();
			if (!def) continue;
			// visit function again to propagate correct formal param types
			if (!def->visited) {
				def->accept(*this);
				def->visited = true;
			}
			if (def->header->has_union_params()) {
				auto new_header = clone_as<NodeFunctionHeader>(def->header.get());
				int method_idx = call->is_in_access_chain() ? 1 : 0;
				for (size_t i = 0; i< call->function->get_num_args(); i++) {
					auto& actual_param = call->function->get_arg(i);
					auto& formal_param = new_header->get_param(i+method_idx);
					match_type(*formal_param, *actual_param);
					formal_param->cast_type();
				}
				new_header->create_function_type();

				// if this was already monomorphized in the exact same way, skip this
				const auto func = m_program->look_up_exact({new_header->name, (int)new_header->params.size()}, new_header->ty);
				// if there is only one call site skip this
				if (func) {
					call->definition = func;
					call->function->declaration = func->header;
					call->function->name = func->header->name;
					continue;
				}
				if (def->header->references.size() == 1) {
					// def->set_header(std::shared_ptr(std::move(new_header)));
					for (size_t i = 0; i<def->header->params.size(); i++) {
						auto const& param = def->header->get_param(i);
						param->ty = new_header->get_param(i)->ty;
					}
					def->header->ty = new_header->ty;
					call->function->name = def->header->name;
					continue;
				}

				// std::cout << "Creating monomorphic function definition for " << new_header->name << std::endl;

				// ----- Zeitmessung: Kopieren der Funktionsdefinition -----
				// auto start_copy = std::chrono::high_resolution_clock::now();
				auto new_func_def = clone_as<NodeFunctionDefinition>(def.get());
				new_func_def->set_header(std::shared_ptr(std::move(new_header)));
				new_func_def->remove_references();
				// auto end_copy = std::chrono::high_resolution_clock::now();
				// auto duration_copy = std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start_copy).count();
				// std::cout << "[Timing] Kopieren der function definition dauerte: " << duration_copy << " µs" << std::endl;

				// ----- Zeitmessung: collect_declarations Schritt -----
				// auto start_collect = std::chrono::high_resolution_clock::now();
				static ASTCollectDeclarations collect(m_program);
				new_func_def->accept(collect);
				// auto end_collect = std::chrono::high_resolution_clock::now();
				// auto duration_collect = std::chrono::duration_cast<std::chrono::microseconds>(end_collect - start_collect).count();
				// std::cout << "[Timing] collect_declarations dauerte: " << duration_collect << " µs" << std::endl;

				new_func_def->collect_references();
				new_func_def->accept(*this);
				// parallel_for_each(new_func_def->header->params.begin(), new_func_def->header->params.end(),
				// 	[](const auto& param) {
				// 		for (auto& ref : param->variable->references) {
				// 			match_reference_declaration(*ref, param->variable);
				// 		}
				// 	});


				const std::string func_name = m_def_provider->get_fresh_name(new_func_def->header->name);
				call->function->name = func_name;
				// new_func_def->header->name = func_name;
				call->function->declaration = new_func_def->header;
				const auto func_ptr = m_program->add_function_definition(std::move(new_func_def));
				call->definition = func_ptr->get_shared();
				func_ptr->header->name = func_name;
			}
		}
		// auto start_merge = std::chrono::high_resolution_clock::now();
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// auto end_merge = std::chrono::high_resolution_clock::now();
		// auto duration_merge = std::chrono::duration_cast<std::chrono::microseconds>(end_merge - start_merge).count();
		// std::cout << "[Timing] Mergen und updaten der function definitions dauerte: " << duration_merge << " µs" << std::endl;
	}

	void apply_types_to_data_structures() const {
		parallel_for_each(m_def_provider->m_all_data_structures.begin(),
			m_def_provider->m_all_data_structures.end(),
			[](const std::weak_ptr<NodeDataStructure>& data_struct_weak) {
				auto data_ptr = data_struct_weak.lock();
				if (!data_ptr) {
					return;
				}
				if (auto comp_type = data_ptr->ty->cast<CompositeType>()) {
					if (comp_type->get_dimensions() == 0) {
						data_ptr->ty = TypeRegistry::add_composite_type(CompoundKind::Array,
							comp_type->get_element_type(), 1);
					}
					std::unique_ptr<NodeDataStructure> new_node = nullptr;
					// if node is variable -> array or ndarray
					if (data_ptr->cast<NodeVariable>()) {
						auto dims = std::max(1, comp_type->get_dimensions());
						if (dims == 1) {
							new_node = data_ptr->to_array(nullptr);
						} else {
							new_node = data_ptr->to_ndarray();
						}
					}
					if (new_node != nullptr) {
						new_node -> ty = data_ptr->ty;
						data_ptr->replace_datastruct(std::move(new_node));
					}
				}
			});
		// for (int i = 0; i < m_def_provider->m_all_data_structures.size(); i++) {
		//
		// 	auto data_struct = m_def_provider->m_all_data_structures[i];
		// 	auto data_ptr = data_struct.lock();
		// 	if (!data_ptr) {
		// 		continue;
		// 	}
		//
		// 	if (auto comp_type = data_ptr->ty->cast<CompositeType>()) {
		// 		if (comp_type->get_dimensions() == 0) {
		// 			data_ptr->ty = TypeRegistry::add_composite_type(CompoundKind::Array,
		// 				comp_type->get_element_type(), 1);
		// 		}
		// 		std::unique_ptr<NodeDataStructure> new_node = nullptr;
		// 		// if node is variable -> array or ndarray
		// 		if (data_ptr->cast<NodeVariable>()) {
		// 			auto dims = std::max(1, comp_type->get_dimensions());
		// 			if (dims == 1) {
		// 				new_node = data_ptr->to_array(nullptr);
		// 			} else {
		// 				new_node = data_ptr->to_ndarray();
		// 			}
		// 		}
		// 		if (new_node != nullptr) {
		// 			new_node -> ty = data_ptr->ty;
		// 			data_ptr->replace_datastruct(std::move(new_node));
		// 		}
		// 	}
		// }

		m_program->def_provider->m_all_data_structures.clear();
	}


public:
	explicit TypeInference(NodeProgram* main) {
		if(main) m_def_provider = main->def_provider;
		m_program = main;
	}

	NodeAST* do_complete_traversal(NodeProgram& node) {
		m_func_calls.clear();
		m_def_provider->m_all_declarations.clear();
		m_def_provider->m_all_references.clear();
		m_def_provider->m_all_data_structures.clear();
		m_def_provider->m_all_assignments.clear();
		m_program->current_callback = nullptr;
		m_program = &node;
		node.accept(*this);

		cast_data_structure_types(&node, true);
		do_monomorphization();

		// its important that the visiting of unused function definitions is done after the monomorphization
		// because m_func_calls is gonna collect and monorphization could happen in an already replaced function
		// -> segfault
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		// do cycle detection
		for(const auto & s : node.struct_definitions) {
			s->collect_recursive_structs(m_program);
		}
		apply_types_to_data_structures();
		return &node;
	}

	NodeAST* do_reachable_traversal(NodeProgram& node) {
		m_func_calls.clear();
		m_def_provider->m_all_declarations.clear();
		m_def_provider->m_all_references.clear();
		m_def_provider->m_all_data_structures.clear();
		m_def_provider->m_all_assignments.clear();
		m_program->current_callback = nullptr;
		m_program = &node;
		node.accept(*this);
		// cast_data_structure_types(&node, true);
		// do_monomorphization();

		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST * visit(NodeProgram& node) override;

	/// Type casting with Base Types
	NodeAST * visit(NodeInt& node) override;
	NodeAST * visit(NodeString& node) override;
	NodeAST * visit(NodeReal& node) override;
	NodeAST * visit(NodeNil& node) override;

    NodeAST * visit(NodeCallback& node) override;

	NodeAST * visit(NodeFunctionParam& node) override;
	NodeAST * visit(NodeSingleDeclaration& node) override;
	NodeAST * visit(NodeSingleAssignment& node) override;
	NodeAST * visit(NodeUIControl& node) override;
	NodeAST * visit(NodeGetControl& node) override;
	NodeAST * visit(NodeSetControl& node) override;

	NodeAST * visit(NodeParamList& node) override;
    /// check if every member has same type
    NodeAST * visit(NodeInitializerList& node) override;

	NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodeVariable& node) override;
	NodeAST * visit(NodeArray& node) override;
	NodeAST * visit(NodeArrayRef& node) override;

    NodeAST * visit(NodeNDArray& node) override;
    NodeAST * visit(NodeNDArrayRef& node) override;

	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;

    NodeAST * visit(NodeList& node) override;
    NodeAST * visit(NodeListRef& node) override;

	NodeAST * visit(NodeFunctionHeaderRef& node) override;

	NodeAST * visit(NodeBinaryExpr& node) override;
	NodeAST * visit(NodeUnaryExpr& node) override;
    NodeAST * visit(NodeFunctionCall& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;
    NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeReturn& node) override;

	NodeAST * visit(NodeAccessChain& node) override;
	NodeAST * visit(NodeSingleDelete& node) override;
	NodeAST * visit(NodeSingleRetain& node) override;
	NodeAST * visit(NodeConst& node) override;
	NodeAST * visit(NodeStruct& node) override;
	NodeAST * visit(NodeNumElements& node) override;
	NodeAST * visit(NodePairs& node) override;
	NodeAST * visit(NodeRange& node) override;
	NodeAST * visit(NodeUseCount& node) override;
	NodeAST * visit(NodeSortSearch& node) override;
	NodeAST * visit(NodeForEach& node) override;
	NodeAST * visit(NodeTernary& node) override;



    /// iterates through all references and declarations and tries to match the types
    /// with cast set to true -> will cast types of data structures if no type could be infered
    static void cast_data_structure_types(const NodeProgram* program, bool cast= false);

    /// error if composite type was not added to the type registry
    static CompileError throw_composite_error(NodeReference* node) {
        auto error = CompileError(ErrorType::TypeError,"", "", node->tok);
        error.m_message = "Composite Type is referenced but does not exist: "+node->name;
        return error;
    }

    static CompileError throw_type_error(const NodeAST& node1, const Type* type, const std::string& message="") {
        auto error = CompileError(ErrorType::TypeError,"", "", node1.tok);
        error.m_message = "Type mismatch: " + node1.ty->to_string() + " and " + type->to_string()+". ";
		error.m_message += message;
        return error;
    }

	static CompileError throw_type_error(const NodeAST& node1, const NodeAST& node2, const std::string& message="") {
    	auto error = CompileError(ErrorType::TypeError,"", "", node1.tok);
    	error.m_message = "Type mismatch: The types of '"+ node1.tok.val+"' (<" + node1.ty->to_string() + ">) and '"+
    		node2.tok.val+"' (<" + node2.ty->to_string()+">) are incompatible.";
    	error.m_got = node1.ty->to_string();
    	error.m_expected = node2.ty->to_string();
    	error.m_message += message;
    	return error;
    }

	/// check for function that has same param types as return type
	static bool is_same_input_same_output_type(const NodeFunctionHeader& header) {
    	if (!header.ty) return false;
    	if (header.params.empty()) return false;
    	const auto func_type = header.ty->cast<FunctionType>();
    	if (!func_type) return false;
    	if (func_type->m_params.empty()) return false;
    	// check if all param types are the same;
    	if (std::ranges::adjacent_find(func_type->m_params, std::not_equal_to<>()) != func_type->m_params.end()) return false;
    	if (!func_type->m_params[0]->is_union_type()) return false;
    	// check if return type is the same as param type
    	if (func_type->m_return_type != func_type->m_params[0]) return false;
    	return true;
    }

    /// check types of initializations and try to infer overall element type
    static Type* infer_initialization_types(std::vector<Type*> &type_list, const NodeAST* node) {
        std::set<Type*> types(type_list.begin(), type_list.end());
        auto error = CompileError(ErrorType::TypeError, "", "", node->tok);
        if(types.empty()) CompileError(ErrorType::InternalError, "Found no types in list", "", node->tok).exit();
        // all types in param list are the same -> set new param list node type
        if(types.size() == 1) {
            auto ty = *types.begin();
            // if(ty->get_type_kind() == TypeKind::Composite) {
            //     error.m_message = "Composite types are not allowed in an initialization.";
            //     error.exit();
            // }
            return ty;
        // two types in param list -> only allowed if types are int|real and string
        } else if (types.size() == 2) {
	        if(const auto it = types.find(TypeRegistry::String); it != types.end()) {
                if(types.contains(TypeRegistry::Integer) or types.contains(TypeRegistry::Real)) {
                    return TypeRegistry::String;
                } else {
                    error.m_message = "Only <String> and <Integer> or <Real> types are allowed in an initialization.";
                    error.exit();
                }
            }
        }
		error.m_message = "Unable to infer type from initialization.";
		error.exit();
        return nullptr;
    }

	/// check node against a constant type -> with error handling
	static Type* match_against(NodeAST& node1, Type* type, const std::string& message="") {
		if(!node1.ty->is_compatible(type)) {
			throw_type_error(node1, type, message).exit();
		}
		// if type is composite and node1 is unknown, set type of node1 to type
		if(type->cast<CompositeType>() and node1.ty == TypeRegistry::Unknown) {
			// stash elem_typ temporarily
			const auto elem_type = node1.ty->get_element_type();
			node1.ty = type;
			node1.set_element_type(elem_type);
		}
		// specialize types:
		node1.set_element_type(specialize_type(node1.ty, type));
		return node1.ty;
	}

	/// tries to match the type of node 1 to the type of node2 after checking type compatibility
	static Type* match_type(NodeAST& node1, NodeAST& node2, const std::string& message="") {
		if(!node1.ty->is_compatible(node2.ty)) {
            throw_type_error(node1, node2, message).exit();
		}

    	match_composite_type(node1, node2);
		// specialize types:
        node1.set_element_type(specialize_type(node1.ty, node2.ty));

        return node1.ty;
	}

	// matches the outer type of node1 and node2 -> does not check for compatibility
	static Type* match_composite_type(NodeAST& node1, NodeAST& node2) {
    	// if one of them is composite type -> the other will be too
    	if(node2.ty->get_type_kind() == TypeKind::Composite and node1.ty == TypeRegistry::Unknown) {
    		// stash elem_typ temporarily
    		const auto elem_type = node1.ty->get_element_type();
    		node1.ty = node2.ty;
    		node1.set_element_type(elem_type);
    	} else if(node1.ty->get_type_kind() == TypeKind::Composite and node2.ty == TypeRegistry::Unknown) {
    		// stash elem_typ temporarily
    		const auto elem_type = node2.ty->get_element_type();
    		node2.ty = node1.ty;
    		node2.set_element_type(elem_type);
    	}
    	return node1.ty;
    }


	/// tries to match the types of l_value and r_value after checking type compatibility, skipping
	/// compatibility check of r_value to l_value because of string and integer
	static Type* match_assignment_types(NodeAST& l_value, NodeAST& r_value) {
		auto l_value_ty = match_type(l_value, r_value);
		if(l_value_ty == TypeRegistry::String) return l_value_ty;
		// specialize types:
		r_value.set_element_type(specialize_type(r_value.ty, l_value.ty));
		return r_value.ty;
	}

    /// tries to match the types of reference and declaration by also checking for element typ
    /// in case declaration is a composite type
    static Type* match_reference_declaration(NodeReference& node, const std::shared_ptr<NodeDataStructure>& declaration) {
		// before lowering, the declaration is not necessarily set, check before:
		if(!declaration) return node.ty;

        Type* declaration_type = declaration->ty->get_element_type();
        Type* reference_type = node.ty->get_element_type();
        if(!reference_type->is_compatible(declaration_type)) {
            throw_type_error(node, *declaration).exit();
        }

        // match type from declaration to reference
		node.set_element_type(specialize_type(reference_type, declaration_type));

		// do not alter declaration type if it already has a distinct type
		// if(declaration->ty->get_element_type() != TypeRegistry::Unknown) {
		// 	return node.ty;
		// }

        declaration_type = declaration->ty->get_element_type();
        reference_type = node.ty->get_element_type();
        if(!declaration_type->is_compatible(reference_type)) {
            throw_type_error(*declaration, node).exit();
        }
        // match type from reference to declaration
        declaration->set_element_type(specialize_type(declaration_type, reference_type));
        return node.ty;
    }

	static Type* match_element_types(NodeAST& node1, NodeAST& node2) {
		Type* node2_type = node2.ty->get_element_type();
		Type* node1_type = node1.ty->get_element_type();
		if(!node1_type->is_compatible(node2_type)) {
			throw_type_error(node1, node2).exit();
		}

		// match type from node2 to reference
		node1.set_element_type(specialize_type(node1_type, node2_type));

		// do not alter node2 type if it already has a type
		if(node2.ty->get_element_type() != TypeRegistry::Unknown) return node1.ty;

		node2_type = node2.ty->get_element_type();
		node1_type = node1.ty->get_element_type();
		if(!node2_type->is_compatible(node1_type)) {
			throw_type_error(node2, node1).exit();
		}
		// match type from reference to node2
		node2.set_element_type(specialize_type(node2_type, node1_type));
		return node1.ty;
	}

    /// tries to find the most specialized type for type1 by looking at type2
    static Type* specialize_type(const Type* type1, const Type* type2) {
        // get element types if composite type (element typ not nullptr):
        const auto node_1 = type1->get_element_type();
        const auto node_2 = type2->get_element_type();

		// if (node_2 == TypeRegistry::String) {
		// 	if (node_1 == TypeRegistry::String) return node_1;
		// 	if (node_1 == TypeRegistry::Unknown) return TypeRegistry::Any;
		// }

        // specialize types:
		// comparison is never the most specialized type
		if(node_2 == TypeRegistry::Comparison) return node_1;
		if(node_1 == TypeRegistry::Comparison) return node_2;

        // if node 2 is unknown, return type of node1
        if(node_2 == TypeRegistry::Unknown) return node_1;
        // if node 1 is unknown, return type of node2
        if(node_1 == TypeRegistry::Unknown) return node_2;

		// if node 2 is unknown, return type of node1
		if(node_2 == TypeRegistry::Nil) return node_1;
		// if node 1 is unknown, return type of node2
		if(node_1 == TypeRegistry::Nil) return node_2;

        // if node 1 is any, node2 has to be more specialized, return node_2
        if(node_1 == TypeRegistry::Any) return node_2;
        // if node 2 is any, node1 has to be more specialized, return node_1
        if(node_2 == TypeRegistry::Any) return node_1;

        // if node 1 is number, node 2 has to be number, int or real, return node2
        if(node_1 == TypeRegistry::Number) return node_2;
        // if node 2 is number, it is the other way around
        if(node_2 == TypeRegistry::Number) return node_1;

        // in all other cases, node_1 is already specialized
        return node_1;
    }

	/// tries to generalize type1 by looking at type2
	/// integer, float -> number
	/// string, integer, float -> any
	static Type* generalize_type(const Type* type1, const Type* type2) {
    	const auto node_1 = type1->get_element_type();
    	const auto node_2 = type2->get_element_type();

    	// if node 2 is unknown, return type of node1
    	if(node_2 == TypeRegistry::Unknown) return node_1;


    	if(node_1 == TypeRegistry::Unknown) {
    		if (node_2 == TypeRegistry::Integer) return TypeRegistry::Integer;
    		if (node_2 == TypeRegistry::Real) return TypeRegistry::Real;
    		if (node_2 == TypeRegistry::String) return TypeRegistry::String;
    	}

    	if (node_1 == TypeRegistry::Integer) {
    		if (node_2 == TypeRegistry::Integer) return node_1;
    		if (node_2 == TypeRegistry::Real) return TypeRegistry::Number;
    		if (node_2 == TypeRegistry::String) return TypeRegistry::Any;
    	}

    	if (node_1 == TypeRegistry::Real) {
    		if (node_2 == TypeRegistry::Integer) return TypeRegistry::Number;
    		if (node_2 == TypeRegistry::Real) return node_1;
    		if (node_2 == TypeRegistry::String) return TypeRegistry::Any;
    	}

    	if (node_1 == TypeRegistry::String) {
    		if (node_2 == TypeRegistry::String) {
    			return node_1;
    		} else {
				return TypeRegistry::Any;
			}
    	}

    	return node_1;
    }

	/// only there to match/generalize formal parameter to actual parameter
	static Type* match_parameters(NodeAST& node1, Type* type, const std::string& message="") {
    	// if(!node1.ty->is_compatible(type)) {
    	// 	throw_type_error(node1, type, message).exit();
    	// }
    	// if type is composite and node1 is unknown, set type of node1 to type
    	if(type->cast<CompositeType>() and node1.ty == TypeRegistry::Unknown) {
    		// stash elem_typ temporarily
    		const auto elem_type = node1.ty->get_element_type();
    		node1.ty = type;
    		node1.set_element_type(elem_type);
    	}
    	// specialize types:
    	node1.set_element_type(generalize_type(node1.ty, type));
    	return node1.ty;
    }

};
