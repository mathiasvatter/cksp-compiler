//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "ReferenceManagement/ASTCollectDeclarations.h"

class TypeInference final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeFunctionCall*>> m_function_calls;

	void do_monomorphization() {
		for (const auto&[fst, snd] : m_function_calls) {
			for (const auto& call : snd) {
				if (call->kind != NodeFunctionCall::Kind::UserDefined) continue;
				if (fst->header->has_union_params()) {
					const auto new_header = std::make_shared<NodeFunctionHeader>(*fst->header);
					const size_t param_count = new_header->params.size();
					for (size_t i = 0; i< param_count; i++) {
						auto& actual_param = call->function->get_arg(i);
						auto& formal_param = new_header->get_param(i);
						match_type(*formal_param, *actual_param);
					}
					new_header->create_function_type();

					// if (new_header->name == "scale.range") {
					//
					// }

					// if this was already monomorphized in the exact same way, skip this
					const auto func = m_program->look_up_exact({new_header->name, (int)new_header->params.size()}, new_header->ty);
					// if there is only one call site skip this
					if (func) {
						call->definition = func;
						call->function->declaration = func->header;
						continue;
					}
					if (snd.size() == 1) {
						fst->header = new_header;
						fst->header->parent = fst;
						continue;
					}

					std::cout << "Creating monomorphic function definition for " << new_header->name << std::endl;
					auto new_func_def = std::make_shared<NodeFunctionDefinition>(*fst);
					new_func_def->header = new_header;
					new_func_def->header->parent = new_func_def.get();
					new_func_def->remove_references();
					// add static class version because we do not want update_function_lookup
					// and the function lookup table to be updated
					static ASTCollectDeclarations collect(m_program);
					new_func_def->accept(collect);
					// new_func_def->collect_declarations(m_program);
					new_func_def->collect_references();
					new_func_def->accept(*this);

					const std::string func_name = m_def_provider->get_fresh_name(call->function->name);
					call->function->name = func_name;
					// new_func_def->header->name = func_name;
					call->function->declaration = new_func_def->header;
					call->definition = new_func_def;
					const auto func_ptr = m_program->add_function_definition(new_func_def);
					func_ptr->header->name = func_name;
				}
			}
		}

		m_program->merge_function_definitions();
		m_program->update_function_lookup();
	}

// #include <mutex>
// #include <execution>
// #include <algorithm>
// #include <omp.h>
//
// // Gemeinsamer Mutex zur Absicherung der kritischen Abschnitte.
// std::mutex global_mutex;
//
// void do_monomorphization() {
//     for (const auto& [fst, snd] : m_function_calls) {
// 		#pragma omp parallel for
//     	for (int idx = 0; idx < static_cast<int>(snd.size()); ++idx) {
//     		auto& call = snd[idx];
//             if (call->kind != NodeFunctionCall::Kind::UserDefined)
//                 return;
//
//             if (fst->header->has_union_params()) {
//                 auto new_header = std::make_shared<NodeFunctionHeader>(*fst->header);
//                 const size_t param_count = new_header->params.size();
//                 // Loop über Parameter (sequentiell, da der Overhead einer weiteren Parallelisierung oft nicht gerechtfertigt ist)
//                 for (size_t i = 0; i < param_count; ++i) {
//                     auto& actual_param = call->function->get_arg(i);
//                     auto& formal_param = new_header->get_param(i);
//                     match_type(*formal_param, *actual_param);
//                 }
//                 new_header->create_function_type();
//
//                 // Kritischer Abschnitt: Suche in m_program.
//                 {
//                     std::lock_guard<std::mutex> lock(global_mutex);
//                     const auto func = m_program->look_up_exact({ new_header->name, static_cast<int>(new_header->params.size()) }, new_header->ty);
//                     if (func) {
//                         call->definition = func;
//                         call->function->declaration = func->header;
//                         return;
//                     }
//                 }
//
//                 // Bei nur einem Call, wird direkt der Header ersetzt.
//                 if (snd.size() == 1) {
//                     std::lock_guard<std::mutex> lock(global_mutex);
//                     fst->header = new_header;
//                     fst->header->parent = fst;
//                     return;
//                 }
//
//                 // Erzeugen einer neuen Funktionsdefinition.
//                 auto new_func_def = std::make_shared<NodeFunctionDefinition>(*fst);
//                 new_func_def->header = new_header;
//                 new_func_def->header->parent = new_func_def.get();
//
//                 // Kritischer Abschnitt: Deklarationen sammeln.
//                 {
// 					new_func_def->remove_references();
//                     std::lock_guard<std::mutex> lock(global_mutex);
//                     static ASTCollectDeclarations collect(m_program);
//                     new_func_def->accept(collect);
// 	                new_func_def->collect_references();
// 	                new_func_def->accept(*this);
//                 }
//
//
//                 std::string func_name;
//                 {
//                     std::lock_guard<std::mutex> lock(global_mutex);
//                     func_name = m_def_provider->get_fresh_name(call->function->name);
//                 }
//                 call->function->name = func_name;
//                 call->function->declaration = new_func_def->header;
//                 call->definition = new_func_def;
//
//                 {
//                     std::lock_guard<std::mutex> lock(global_mutex);
//                     const auto func_ptr = m_program->add_function_definition(new_func_def);
//                     func_ptr->header->name = func_name;
//                 }
//             }
//         }
//     }
//
//     m_program->merge_function_definitions();
//     m_program->update_function_lookup();
// }



public:
	explicit TypeInference(NodeProgram* main) {
		if(main) m_def_provider = main->def_provider;
		m_program = main;
	}

	NodeAST* do_complete_traversal(NodeProgram& node) {
		m_function_calls.clear();
		m_def_provider->m_all_declarations.clear();
		m_def_provider->m_all_references.clear();
		m_def_provider->m_all_data_structures.clear();
		m_def_provider->m_all_assignments.clear();
		m_program->current_callback = nullptr;
		m_program = &node;
		node.accept(*this);
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}

		cast_data_structure_types(&node, true);
		// do cycle detection
		for(const auto & s : node.struct_definitions) {
			s->collect_recursive_structs(m_program);
		}
		// node.debug_print();
		do_monomorphization();
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* do_reachable_traversal(NodeProgram& node) {
		m_function_calls.clear();
		m_def_provider->m_all_declarations.clear();
		m_def_provider->m_all_references.clear();
		m_def_provider->m_all_data_structures.clear();
		m_def_provider->m_all_assignments.clear();
		m_program->current_callback = nullptr;
		m_program = &node;
		node.accept(*this);
		node.reset_function_visited_flag();
		// cast_data_structure_types(&node, true);
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



    /// iterates through all references and declarations and tries to match the types
    /// with cast set to true -> will cast types of data structures if no type could be infered
    static void cast_data_structure_types(const NodeProgram* program, bool cast= false);

    /// error if composite type was not added to the type registry
    static CompileError throw_composite_error(NodeReference* node) {
        auto error = CompileError(ErrorType::TypeError,"", "", node->tok);
        error.m_message = "Composite Type is referenced but does not exist: "+node->name;
        return error;
    }

    static CompileError throw_type_error(const NodeAST& node1, Type* type, const std::string& message="") {
        auto error = CompileError(ErrorType::TypeError,"", "", node1.tok);
        error.m_message = "Type mismatch: " + node1.ty->to_string() + " and " + type->to_string()+". ";
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
    	if (std::adjacent_find(func_type->m_params.begin(), func_type->m_params.end(), std::not_equal_to<>()) != func_type->m_params.end()) return false;
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
            if(ty->get_type_kind() == TypeKind::Composite) {
                error.m_message = "Composite types are not allowed in an initialization.";
                error.exit();
            }
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
            throw_type_error(node1, node2.ty, message).exit();
		}
		// if one of them is composite type -> the other will be too
		if(node2.ty->get_type_kind() == TypeKind::Composite and node1.ty == TypeRegistry::Unknown) {
			// stash elem_typ temporarily
			auto elem_type = node1.ty->get_element_type();
			node1.ty = node2.ty;
			node1.set_element_type(elem_type);
		} else if(node1.ty->get_type_kind() == TypeKind::Composite and node2.ty == TypeRegistry::Unknown) {
			// stash elem_typ temporarily
			auto elem_type = node2.ty->get_element_type();
			node2.ty = node1.ty;
			node2.set_element_type(elem_type);
		}

		// specialize types:
        node1.set_element_type(specialize_type(node1.ty, node2.ty));

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
            throw_type_error(node, declaration->ty).exit();
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
            throw_type_error(*declaration, node.ty).exit();
        }
        // match type from reference to declaration
        declaration->set_element_type(specialize_type(declaration_type, reference_type));
        return node.ty;
    }

	static Type* match_element_types(NodeAST& node1, NodeAST& node2) {
		Type* node2_type = node2.ty->get_element_type();
		Type* node1_type = node1.ty->get_element_type();
		if(!node1_type->is_compatible(node2_type)) {
			throw_type_error(node1, node2.ty).exit();
		}

		// match type from node2 to reference
		node1.set_element_type(specialize_type(node1_type, node2_type));

		// do not alter node2 type if it already has a type
		if(node2.ty->get_element_type() != TypeRegistry::Unknown) return node1.ty;

		node2_type = node2.ty->get_element_type();
		node1_type = node1.ty->get_element_type();
		if(!node2_type->is_compatible(node1_type)) {
			throw_type_error(node2, node1.ty).exit();
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
    	node1.set_element_type(generalize_type(node1.ty, type));
    	return node1.ty;
    }

};
