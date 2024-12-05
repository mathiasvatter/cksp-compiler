//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * @class NormalizeArrayAssign
 * @brief This class is used for lowering single assign and declare statements considering special needs of vanilla ksp
 * syntax.
 *
 * It respects specific vanilla ksp rules for array initialization and assignment.
 * @SingleAssignment: if array ref is assigned a list of values, a function call to array.init.{type} or a
 * series of single index assignments is generated, the function definition is added.
 * @SingleDeclaration: if array is declared with an initializer list and of type string, a series of single index
 * assignments is generated.
 * Inherits from the ASTVisitor class.
 */
class NormalizeArrayAssign : public ASTVisitor {
private:

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for (auto &callback : node.callbacks) {
			callback->accept(*this);
		}
		node.merge_function_definitions();
		return &node;
	}

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			auto definition = node.get_definition();
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;

		}
		return &node;
	}

	inline NodeAST * visit(NodeBlock& node) override {
		for(auto &stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	}

	inline NodeAST * visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
		return &node;
	}

	inline NodeAST * visit(NodeSingleAssignment& node) override {
		if(node.l_value->get_node_type() == NodeType::ArrayRef) {
			auto node_array_ref = static_cast<NodeArrayRef*>(node.l_value.get());
			// if lhs is arrayref and has no index, check if array is initialized with a list of values or array copy
			if(!node_array_ref->index and not(node.r_value->get_node_type() == NodeType::InitializerList or node.r_value->get_node_type() == NodeType::ArrayRef)) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "<Array> can only be assigned with a list of values.";
				error.m_expected = "<InitializerList>";
				error.m_got = node.r_value->get_string();
				error.exit();
			} else if (node_array_ref->index and node.r_value->get_node_type() == NodeType::InitializerList) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Array element can only be assigned with a single value.";
				error.m_expected = "<Variable>";
				error.m_got = "<InitializerList>";
				error.exit();
			} else if (node_array_ref->index) {
				return &node;
			}

			if(node.r_value->get_node_type() == NodeType::InitializerList) {
				// if param list has only one value:
				auto init_list = static_cast<NodeInitializerList *>(node.r_value.get());
				if (init_list->size() == 1) {
					NormalizeArrayAssign::add_array_init_function_def(m_program, node.l_value->ty->get_element_type());
					return node.replace_with(get_array_init_function_call(node_array_ref, init_list->elem(0).get()));
				} else {
					return node.replace_with(get_array_init_from_list(node_array_ref, init_list));
				}
			} else if(node.r_value->get_node_type() == NodeType::ArrayRef) {
				auto node_val_array_ref = static_cast<NodeArrayRef*>(node.r_value.get());
				NormalizeArrayAssign::add_array_copy_function_def(m_program, node.l_value->ty->get_element_type());
				return node.replace_with(get_array_copy_function_call(node_array_ref, node_val_array_ref));
			}
		}
		return &node;
	}

	inline NodeAST * visit(NodeSingleDeclaration& node) override {
		if(!node.variable->ty->cast<CompositeType>()) return &node;
		if (!node.variable->is_local) return &node;

		// skip constant variables
		if(node.variable->data_type == DataType::Const)
			return &node;

		std::unique_ptr<NodeBlock> node_body = nullptr;
		if(node.variable->get_node_type() == NodeType::Array) {
			node_body = std::make_unique<NodeBlock>(node.tok);
			auto node_array = static_cast<NodeArray*>(node.variable.get());
			auto node_array_ref = node_array->to_reference();
			// if lhs is arrayref and has no index, check if array is initialized with a list of values
			if(node.value) {
				if (not(node.value->get_node_type() == NodeType::InitializerList or node.value->get_node_type() == NodeType::ArrayRef)) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
					error.m_message = "<Array> can only be declared with a list of values.";
					error.m_expected = "<InitializerList>";
					error.m_got = node.value->get_string();
					error.exit();
				}
			}

			if(!node.value) {
				NormalizeArrayAssign::add_array_init_function_def(m_program, node.variable->ty->get_element_type());
				node_body->add_stmt(std::make_unique<NodeStatement>(
					get_array_init_function_call(node_array_ref.get(), TypeRegistry::get_neutral_element_from_type(node.variable->ty->get_element_type()).get()),
					node.tok));
			} else {
				// declare local array: [] := (1) or (1,2,3,45)
				if (node.value->get_node_type() == NodeType::InitializerList) {
					// if param list has only one value:
					auto init_list = static_cast<NodeInitializerList *>(node.value.get());
					if (init_list->size() == 1) {
						NormalizeArrayAssign::add_array_init_function_def(m_program,
																		  node.variable->ty->get_element_type());
						node_body->add_stmt(std::make_unique<NodeStatement>(
							get_array_init_function_call(node_array_ref.get(), init_list->elem(0).get()),
							node.tok));
					} else {
						auto array_ref = static_cast<NodeArrayRef *>(node_array_ref.get());
						node_body = get_array_init_from_list(array_ref, init_list);
					}
				// copy assignment array to array
				} else if (node.value->get_node_type() == NodeType::ArrayRef) {
					auto node_val_array_ref = static_cast<NodeArrayRef*>(node.value.get());
					NormalizeArrayAssign::add_array_copy_function_def(m_program,
																	  node.variable->ty->get_element_type());
					node_body->add_stmt(std::make_unique<NodeStatement>(
						get_array_copy_function_call(node_array_ref.get(), node_val_array_ref),
						node.tok));
				}
			}
			node_body->prepend_stmt(std::make_unique<NodeStatement>(
									std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok),
									node.tok
									)
								);
		} else if (node.variable->get_node_type() == NodeType::Variable) {
//			auto node_var = static_cast<NodeVariable*>(node.variable.get());
//			// no string const variables are allowed???
//			node_var->data_type = DataType::Mutable;
//			if(node.value) {
//				node_body = std::make_unique<NodeBlock>(node.tok);
//				node_body->add_stmt(std::make_unique<NodeStatement>(std::make_unique<NodeSingleAssignment>(node_var->to_reference(), std::move(node.value), node.tok), node.tok));
//				node_body->prepend_stmt(std::make_unique<NodeStatement>(std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok), node.tok));
//			}
		}
		// if node_body is set, replace node with node_body
		if(node_body) {
			return node.replace_with(std::move(node_body));
		}
		return &node;
	}

public:
	explicit NormalizeArrayAssign(NodeProgram* program) {
		m_program = program;
	};

	static std::unique_ptr<NodeBlock> get_array_init_from_list(NodeArrayRef* array_ref, NodeInitializerList* init_list) {
		auto node_body = std::make_unique<NodeBlock>(array_ref->tok);
		for(int i = 0; i<init_list->size(); i++) {
			array_ref->index = std::make_unique<NodeInt>((int32_t)i, array_ref->tok);
			auto &elem = init_list->elem(i);
			auto node_assign = std::make_unique<NodeSingleAssignment>(
				clone_as<NodeReference>(array_ref),
				elem->clone(),
				elem->tok
			);
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign), elem->tok));
		}
		return node_body;
	}


	static inline std::unique_ptr<NodeBlock> get_array_init_function_call(NodeReference* array_ref, NodeAST* value) {
		std::string func_name = "array<-init["+array_ref->get_declaration()->ty->get_element_type()->to_string()+"]";
		auto node_iterator = std::make_shared<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, array_ref->tok);
		node_iterator->is_local = true;
		node_iterator->ty = TypeRegistry::Integer;
		auto node_iterator_ref = node_iterator->to_reference();
		node_iterator_ref->match_data_structure(node_iterator);
		node_iterator_ref->ty = TypeRegistry::Integer;
		auto node_body = std::make_unique<NodeBlock>(array_ref->tok);
		node_body->scope = true;

		std::unique_ptr<NodeAST> rhs_value = nullptr;
		if(!value) {
			rhs_value = TypeRegistry::get_neutral_element_from_type(array_ref->get_declaration()->ty->get_element_type());
		} else {
			rhs_value = value->clone();
		}

		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(
					array_ref->tok,
					array_ref->clone(),
					std::move(node_iterator_ref),
					std::move(rhs_value)
				),
				array_ref->tok
			),
			array_ref->tok
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeSingleDeclaration>(
				std::move(node_iterator),
				nullptr, array_ref->tok
			),
			array_ref->tok)
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_function_call), array_ref->tok));
		return node_body;
	}


	/**
	 * function array_init(array: [], iter: int, value: {type})
	 *  while(iter < num_elements(array))
	 *      array[iter] := {type}
	 *      inc(iter)
	 *  end while
	 * end function
	 */
	static inline bool add_array_init_function_def(NodeProgram* program, Type* type) {
		std::string func_name = "array<-init["+type->to_string()+"]";
		// check if function with this type already exists
		auto it = program->function_lookup.find({func_name, 3});
		if(it != program->function_lookup.end()) {
			return false;
		}

		auto node_array = std::make_shared<NodeArray>(std::nullopt, "array", TypeRegistry::add_composite_type(CompoundKind::Array, type), nullptr, Token());
		auto node_iterator = std::make_shared<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, Token());
		auto node_value = std::make_shared<NodeVariable>(std::nullopt, "value", type, DataType::Mutable, Token());
		auto node_function_def = std::make_shared<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				Token()
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token(), true),
			Token()
		);
		node_function_def->header->add_param(node_array);
		node_function_def->header->add_param(node_iterator);
		node_function_def->header->add_param(node_value);

		auto node_array_ref = unique_ptr_cast<NodeArrayRef>(node_array->to_reference());
		node_array_ref->set_index(node_iterator->to_reference());
		auto node_iterator_ref = node_iterator->to_reference();
		auto node_value_ref = node_value ->to_reference();

		auto node_while_body = std::make_unique<NodeBlock>(Token());
		node_while_body->scope = true;
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(node_array_ref.get()),
			std::move(node_value_ref),
			Token()
		);
		auto node_inc = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"inc",
				std::make_unique<NodeParamList>(Token(),node_iterator_ref->clone()),Token()),
			Token()
		);

		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), Token()));
		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_inc), Token()));

		// delete index of node_array_ref for its last usage in num_elements
		node_array_ref->index = nullptr;
		auto new_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_THAN,
				node_iterator_ref->clone(),
				node_array_ref->get_size(),
				Token()
			),
			std::move(node_while_body),
			Token()
		);
		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(std::move(new_while), Token()));
		node_function_def->header->create_function_type(TypeRegistry::Void);
		node_function_def->ty = TypeRegistry::Void;
		program->add_function_definition(node_function_def);
//		program->additional_function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
//		program->update_function_lookup();
		return true;
	}

	static inline std::unique_ptr<NodeBlock> get_array_copy_function_call(NodeReference* array_dest, NodeReference* array_src) {
		std::string func_name = "array.copy."+array_dest->get_declaration()->ty->get_element_type()->to_string();
		auto node_iterator = std::make_shared<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, array_dest->tok);
		node_iterator->is_local = true;
		auto node_iterator_ref = node_iterator->to_reference();
		node_iterator_ref->match_data_structure(node_iterator);
		auto node_body = std::make_unique<NodeBlock>(array_dest->tok);
		node_body->scope = true;


		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(
					array_dest->tok,
					array_dest->clone(),
					array_src->clone(),
					std::move(node_iterator_ref)
				),
				array_dest->tok
			),
			array_dest->tok
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeSingleDeclaration>(
				std::move(node_iterator),
				nullptr, array_dest->tok
			),
			array_dest->tok)
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_function_call), array_dest->tok));
		return node_body;
	}

	/**
	 * function array_copy(dest: [], src: [], _iter: int)
	 *  while(iter < num_elements(src))
	 *      dest[iter] := src[iter]
	 *      inc(iter)
	 *  end while
	 * end function
	 */
	static inline bool add_array_copy_function_def(NodeProgram* program, Type* type) {
		std::string func_name = "array.copy." + type->to_string();
		// check if function with this type already exists
		auto it = program->function_lookup.find({func_name, 3});
		if(it != program->function_lookup.end()) {
			return false;
		}

		auto node_dest = std::make_shared<NodeArray>(std::nullopt, "dest", TypeRegistry::add_composite_type(CompoundKind::Array, type), nullptr, Token());
		auto node_src = std::make_shared<NodeArray>(std::nullopt, "src", TypeRegistry::add_composite_type(CompoundKind::Array, type), nullptr, Token());
		auto node_iterator = std::make_shared<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, Token());
		auto node_iterator_ref = node_iterator->to_reference();
		auto node_dest_ref = unique_ptr_cast<NodeArrayRef>(node_dest->to_reference());
		auto node_src_ref = unique_ptr_cast<NodeArrayRef>(node_src->to_reference());
		auto node_function_def = std::make_shared<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				Token()
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token(), true),
			Token()
		);
		node_function_def->header->add_param(clone_as<NodeDataStructure>(node_dest.get()));
		node_function_def->header->add_param(clone_as<NodeDataStructure>(node_src.get()));
		node_function_def->header->add_param(clone_as<NodeDataStructure>(node_iterator.get()));

		// get declaration pointer right
//		node_dest_ref->match_data_structure(node_function_def->header->get_param(0).get());
//		node_src_ref->match_data_structure(node_function_def->header->get_param(1).get());
//		node_iterator_ref->match_data_structure(node_function_def->header->get_param(2).get());

		auto node_while_body = std::make_unique<NodeBlock>(Token());
		node_while_body->scope = true;
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(node_dest_ref.get()),
			node_src_ref->clone(),
			Token()
		);
		auto node_inc = DefinitionProvider::inc(clone_as<NodeReference>(node_iterator_ref.get()));

		node_while_body->add_as_stmt(std::move(node_assignment));
		node_while_body->add_as_stmt(std::move(node_inc));

		// delete index of node_array_ref for its last usage in num_elements
		node_src_ref->index = nullptr;
		auto new_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_THAN,
				node_iterator_ref->clone(),
				node_src_ref->get_size(),
				Token()
			),
			std::move(node_while_body),
			Token()
		);

		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(std::move(new_while), Token()));
		node_function_def->header->create_function_type(TypeRegistry::Void);
		node_function_def->ty = TypeRegistry::Void;
		program->add_function_definition(node_function_def);
//		program->additional_function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
//		program->update_function_lookup();
		return true;
	}


};