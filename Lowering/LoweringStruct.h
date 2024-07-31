//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringStruct : public ASTLowering {
private:
	NodeFunctionDefinition* m_current_func = nullptr;
	NodeStruct* m_current_struct = nullptr;
	std::unique_ptr<NodeVariableRef> m_max_structs_ref = std::make_unique<NodeVariableRef>("MAX_STRUCTS", Token());
	inline bool in_constructor() {
		return m_current_func and m_current_struct and m_current_func == m_current_struct->constructor;
	}
	/// returns free_idx as reference if in constructor, self as reference if not
	inline std::unique_ptr<NodeReference> get_index_ref() {
		if(in_constructor()) {
			return m_current_struct->free_idx_var->to_reference();
		}
		return m_current_struct->node_self->to_reference();
	}
public:
	explicit LoweringStruct(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeStruct& node) override {
		m_current_struct = &node;

		// check if all member node types are allowed
		for(auto & m: node.member_table) {
			if(NodeStruct::allowed_member_node_types.find(m.second->get_node_type()) == NodeStruct::allowed_member_node_types.end()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", m.second->tok);
				error.m_message = "Member type not allowed in struct. Only <Variables>, <Arrays>, <NDArrays>, <Pointers> and <Structs> are allowed.";
				error.m_got = m.second->ty->to_string();
				error.exit();
			}
			node.member_node_types.insert(m.second->get_node_type());
		}

		// add compiler struct vars
		auto struct_vars = add_compiler_struct_vars();
		// remove "self" from member declarations
		node.members->statements.erase(node.members->statements.begin());
		node.members->accept(*this);
		m_current_struct = &node;
		node.members->prepend_body(std::move(struct_vars));
		for(auto & m: node.methods) {
			m->accept(*this);
		}

		m_current_struct = nullptr;
		m_current_func = nullptr;
		return &node;
	}

	std::unique_ptr<NodeBlock> add_compiler_struct_vars() {
		auto node_block = std::make_unique<NodeBlock>(Token());
		auto max_structs_var = std::make_unique<NodeVariable>(
			std::nullopt,
			m_current_struct->name+".MAX_STRUCTS",
			TypeRegistry::Integer,
			DataType::Const,
			Token()
		);
		auto max_structs_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(max_structs_var),
			get_max_individual_structs_size(m_current_struct),
			Token()
		);
		m_current_struct->max_individual_struts_var = static_cast<NodeVariable*>(max_structs_decl->variable.get());
		// add free_idx variable and allocation array to struct
		auto free_idx_var = std::make_unique<NodeVariable>(
			std::nullopt,
			m_current_struct->name+".free_idx",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		auto free_idx_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(free_idx_var),
			std::make_unique<NodeInt>(0, Token()),
			Token()
		);
		m_current_struct->free_idx_var = static_cast<NodeVariable*>(free_idx_decl->variable.get());
		auto allocation_var = std::make_unique<NodeArray>(
			std::nullopt,
			m_current_struct->name+".allocation",
			TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Integer),
			m_current_struct->max_individual_struts_var->to_reference(),
			Token()
		);
		auto allocation_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(allocation_var),
			nullptr,
			Token()
		);
		m_current_struct->allocation_var = static_cast<NodeArray*>(allocation_decl->variable.get());
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(max_structs_decl), Token()));
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(free_idx_decl), Token()));
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(allocation_decl), Token()));
		return node_block;
	}

	inline NodeAST * visit(NodeAccessChain& node) override {
		if(auto lowering = node.get_lowering(m_program)) {
			node.accept(*lowering);
		}
		return node.replace_with(std::move(node.chain.back()));
	}

	inline NodeAST * visit(NodeSingleDeclaration& node) override {
		// turn member into array if it is a member
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		// turn node.value into param list if is member and has been turned into array
		if(node.variable->is_member() and node.value) {
			if(node.variable->get_node_type() != NodeType::Array and node.variable->get_node_type() != NodeType::NDArray) {
				return &node;
			}
			if(node.value->get_node_type() != NodeType::ParamList) {
				auto param_list = std::make_unique<NodeParamList>(node.tok);
				param_list->params.push_back(std::move(node.value));
				param_list->parent = &node;
				node.value = std::move(param_list);
			}
		}
		return &node;
	}

	inline NodeAST * visit(NodeVariable& node) override {
		// if member, turn into array
		if(node.is_member()) {
			auto node_array = node.to_array(m_current_struct->max_individual_struts_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			return safe_replace_datastructure(std::move(node_array), &node);
		}
		return &node;
	}
	inline NodeAST * visit(NodePointer& node) override {
		// if member, turn into array of pointers
		if(node.is_member()) {
			auto node_array = node.to_array(m_current_struct->max_individual_struts_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			return safe_replace_datastructure(std::move(node_array), &node);
		}
		return &node;
	}
	inline NodeAST * visit(NodeArray& node) override {
		// if member, turn into multi-dimensional array
		/*
		 * 	declare velocities[10]: [int]
		 * 	declare struct.velocity[struct.MAX_STRUCTS, 10]
		 */
		if(node.is_member()) {
			auto node_ndarray = node.to_ndarray();
			node_ndarray->sizes->prepend_param(m_current_struct->max_individual_struts_var->to_reference());
			node_ndarray->dimensions = node_ndarray->sizes->params.size();
			node_ndarray->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), node_ndarray->dimensions);
			return safe_replace_datastructure(std::move(node_ndarray), &node);
		}
		return &node;
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		// if member, turn into multi-dimensional array
		if(node.is_member()) {
			node.sizes->prepend_param(m_current_struct->max_individual_struts_var->to_reference());
			node.dimensions = node.sizes->params.size();
			node.ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), node.dimensions);
			node.is_local = false;
		}
		return &node;
	}

	inline NodeAST * visit(NodeVariableRef& node) override {
		// if member reference, turn into array reference with (struct.free_idx as index if in constructor, self as index if not)
		if(node.is_member_ref()) {
			auto node_array_ref = node.to_array_ref(get_index_ref().get());
			return safe_replace_reference(std::move(node_array_ref), &node);
		}
		return &node;
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(node.is_member_ref()) {
			// if array has no indexes -> wildcard
			if(!node.index) {
				node.index = std::make_unique<NodeWildcard>("*", node.tok);
			}
			auto node_ndarray_ref = node.to_ndarray_ref();
			node_ndarray_ref->indexes->prepend_param(get_index_ref());
			node_ndarray_ref->declaration = node.declaration;
			node_ndarray_ref->determine_sizes();
			return safe_replace_reference(std::move(node_ndarray_ref), &node);
		}
		return &node;
	}

	inline NodeAST * visit(NodePointerRef& node) override {
		if(node.is_member_ref()) {
			auto node_array_ref = node.to_array_ref(get_index_ref().get());
			return safe_replace_reference(std::move(node_array_ref), &node);
		}
		return &node;
	}

	inline NodeAST * visit(NodeNDArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(node.is_member_ref()) {
			node.determine_sizes();
			// if array has no indexes -> everything should be copied -> wildcards for every index of size
			if (!node.indexes) {
				node.indexes = std::make_unique<NodeParamList>(node.tok);
				for (int i = 1; i < node.sizes->params.size(); i++) {
					node.indexes->add_param(std::make_unique<NodeWildcard>("*", node.tok));
				}
			}
			node.indexes->prepend_param(get_index_ref());
		}
		return &node;
	}


	inline NodeAST * visit(NodeFunctionDefinition& node) override {
		m_current_func = &node;
		node.header->accept(*this);
		node.body->accept(*this);
		// lower init function
		if(node.header->name == m_current_struct->name+".__init__") {
			lower_init_method(&node);
		} else {
			node.header->args->prepend_param(m_current_struct->node_self->clone());
		}
		return &node;
	}

private:
	/// to be used on references
	inline NodeReference* safe_replace_reference(std::unique_ptr<NodeReference> new_node, NodeReference* old_node) {
		new_node->match_data_structure(old_node->declaration);
		m_def_provider->remove_reference(new_node->declaration, old_node);
		auto new_ref = static_cast<NodeReference*>(old_node->replace_with(std::move(new_node)));
		m_def_provider->add_reference(new_ref->declaration, new_ref);
		return new_ref;
	}
	/// to be used on datastructures
	inline NodeDataStructure* safe_replace_datastructure(std::unique_ptr<NodeDataStructure> new_node, NodeDataStructure* old_node) {
		auto references = m_def_provider->get_references(old_node);
		auto new_data_struct = static_cast<NodeDataStructure*>(old_node->replace_with(std::move(new_node)));
		m_def_provider->set_references(new_data_struct, references);
		for(auto & ref : references) {
			ref->declaration = new_data_struct;
		}
		return new_data_struct;
	}
	/// uses the member table to determine the size max size of struct allocations
	/// declare const Node.MAX_STRUCTS: int := MAX_STRUCTS/_max(10, 11*12)
	/// returns either MAX_STRUCTS or binary_expr with biggest array member size
	inline std::unique_ptr<NodeAST> get_max_individual_structs_size(NodeStruct* object) {
		std::vector<std::unique_ptr<NodeAST>> sizes;
		for(auto & data : object->member_table) {
			if(data.second->get_node_type() == NodeType::Array) {
				auto node_array = static_cast<NodeArray*>(data.second);
				sizes.push_back(node_array->size->clone());
			} else if(data.second->get_node_type() == NodeType::NDArray) {
				auto node_ndarray = static_cast<NodeNDArray*>(data.second);
				auto size = NodeBinaryExpr::create_right_nested_binary_expr(node_ndarray->sizes->params, 0, token::MULT);
				sizes.push_back(std::move(size));
			}
		}
		if(sizes.empty()) {
			return m_max_structs_ref->clone();
		}
		add_max_function_def(m_program);
		return std::make_unique<NodeBinaryExpr>(
			token::DIV,
			m_max_structs_ref->clone(),
			find_max_array_size(sizes),
			Token()
			);
	}

	static std::unique_ptr<NodeFunctionCall> create_max_call(const std::unique_ptr<NodeAST>& left, const std::unique_ptr<NodeAST>& right) {
		std::string func_name = "_max";
		return std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					left->tok, // Hier annehmen, dass 'tok' ein Token-Objekt ist
					left->clone(),
					right->clone()
				),
				left->tok
			),
			left->tok
		);
	}

	/// creates a nested call to max(a, b) from many sizes
	static inline std::unique_ptr<NodeFunctionCall> find_max_array_size(const std::vector<std::unique_ptr<NodeAST>> &sizes) {
		if (sizes.empty()) {
			auto error = CompileError(ErrorType::InternalError, "", "", Token());
			error.m_message = "No sizes given for struct.";
			error.exit();
		}
		std::unique_ptr<NodeAST> max_call = sizes[0]->clone();
		for (size_t i = 1; i < sizes.size(); ++i) {
			max_call = create_max_call(max_call, sizes[i]);
		}
		return std::unique_ptr<NodeFunctionCall>(static_cast<NodeFunctionCall*>(max_call.release()));
	}

	/**
	 * function _max(a: int, b: int) -> result
	 * 	result := (a + b + abs(a - b)) / 2
	 * end function
	 */
	static inline bool add_max_function_def(NodeProgram* program) {
		std::string func_name = "_max";

		// Prüfen, ob die Funktion bereits existiert
		auto it = program->function_lookup.find({func_name, 2});
		if (it != program->function_lookup.end()) {
			return false; // Funktion existiert bereits
		}

		// Erstellung der Parameter a und b
		auto node_a = std::make_unique<NodeVariable>(std::nullopt, "a", TypeRegistry::Integer, DataType::Mutable, Token());
		auto node_b = std::make_unique<NodeVariable>(std::nullopt, "b", TypeRegistry::Integer, DataType::Mutable, Token());

		// Referenzen auf die Parameter
		auto node_a_ref = node_a->to_reference();
		auto node_b_ref = node_b->to_reference();

		// Aufbau der Funktionsdefinition
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					Token(),
					std::move(node_a),
					std::move(node_b)
				),
				Token()
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token()),
			Token()
		);
		node_function_def->body->scope = true;
		node_function_def->num_return_params = 1;
		node_function_def->ty = TypeRegistry::Integer;

		// abs(a - b)
		auto node_abs_func = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"abs",
				std::make_unique<NodeParamList>(
					Token(),
					std::make_unique<NodeBinaryExpr>(token::SUB,node_a_ref->clone(),node_b_ref->clone(),Token())
				),Token()
			),Token());
		node_abs_func->kind = NodeFunctionCall::Kind::Builtin;
		// (a + b + abs(a - b)) // 2
		auto max_expression = std::make_unique<NodeBinaryExpr>(
			token::DIV,
			std::make_unique<NodeBinaryExpr>(
				token::ADD,
				std::make_unique<NodeBinaryExpr>(
					token::ADD,
					node_a_ref->clone(),
					node_b_ref->clone(),
					Token()
				),
				std::move(node_abs_func),
				Token()
			),
			std::make_unique<NodeInt>(2, Token()),
			Token()
		);

		// Fügen Sie den Ausdruck in den Funktionskörper ein
		// result := (a + b + abs(a - b)) // 2
		node_function_def->body->add_stmt(
			std::make_unique<NodeStatement>(
				std::make_unique<NodeReturn>(
					Token(),
					std::move(max_expression)
				),
				Token()
			));

		// Fügen Sie die neue Funktionsdefinition zum Programm hinzu
		program->function_definitions.push_back(std::move(node_function_def));

		// Update function lookup so that the new function can be found
		program->update_function_lookup();

		return true;
	}

	/**
	 * Lower init function by adding:
	 *  struct.free_idx := search(struct.allocation, 0)
	 * 	if struct.free_idx = -1
	 * 		message("Error: No more free space available to allocate objects of type 'note'")
	 * 	end if
	 * 	struct.allocation[struct.free_idx] := 1
	 * 	...
	 * 	return struct.free_idx
	 */
	inline void lower_init_method(NodeFunctionDefinition* init) {
		auto node_block = std::make_unique<NodeBlock>(init->tok);
		auto node_search_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"search",
				std::make_unique<NodeParamList>(
					init->tok,
					m_current_struct->allocation_var->to_reference(),
					std::make_unique<NodeInt>(0, init->tok)
				),
				init->tok
			),
			init->tok
		);
		node_search_call->kind = NodeFunctionCall::Kind::Builtin;
		auto node_assign_search = std::make_unique<NodeSingleAssignment>(
			m_current_struct->free_idx_var->to_reference(),
			std::move(node_search_call),
			init->tok
		);
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign_search), init->tok));

		auto node_error_message = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"message",
				std::make_unique<NodeParamList>(
					init->tok,
					std::make_unique<NodeString>(
						"\"Error: No more free space available to allocate objects of type '"+ m_current_struct->name + "'\"",
						init->tok)
				),
				init->tok
			),
			init->tok
		);
		node_error_message->kind = NodeFunctionCall::Kind::Builtin;
		auto node_if_stmt = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(token::EQUAL,
											 m_current_struct->free_idx_var->to_reference(),
											 std::make_unique<NodeInt>(-1, init->tok),
											 init->tok),
			std::make_unique<NodeBlock>(init->tok,
										std::make_unique<NodeStatement>(std::move(node_error_message), init->tok)),
			std::make_unique<NodeBlock>(init->tok),
			init->tok
		);
		node_if_stmt->if_body->scope = true;
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_if_stmt), init->tok));

		auto node_allocation = m_current_struct->allocation_var->to_reference();
		static_cast<NodeArrayRef *>(node_allocation.get())->index = m_current_struct->free_idx_var->to_reference();
		auto node_assign_allocation = std::make_unique<NodeSingleAssignment>(
			std::move(node_allocation),
			std::make_unique<NodeInt>(1, init->tok),
			init->tok
		);
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign_allocation), init->tok));
		init->body->prepend_body(std::move(node_block));
		init->body->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeReturn>(init->tok, m_current_struct->free_idx_var->to_reference()), init->tok)
		);
	}


};