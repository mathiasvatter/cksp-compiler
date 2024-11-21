//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class PreLoweringStruct : public ASTLowering {
private:
	NodeFunctionDefinition* m_current_func = nullptr;
	NodeStruct* m_current_struct = nullptr;
	std::unique_ptr<NodeVariableRef> m_max_structs_ref = std::make_unique<NodeVariableRef>("MAX_STRUCTS", Token());

public:
	explicit PreLoweringStruct(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeStruct& node) override {
		m_current_struct = &node;

		// check if all member node types are allowed
		for(auto & m: node.member_table) {
			auto member = m.second.lock();
			if(NodeStruct::allowed_member_node_types.find(member->get_node_type()) == NodeStruct::allowed_member_node_types.end()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", member->tok);
				error.m_message = "Member type not allowed in struct. Only <Variables>, <Arrays>, <NDArrays>, <Pointers> and <Structs> are allowed.";
				error.m_got = member->ty->to_string();
				error.exit();
			}
			node.member_node_types.insert(member->get_node_type());
		}

		// add compiler struct vars
		prepend_compiler_struct_vars(&node);
		node.collect_recursive_structs(m_program);
//		node.generate_ref_count_methods();
		node.rebuild_member_table();
		return &node;
	}

	void prepend_compiler_struct_vars(NodeStruct* strct) {
		auto node_block = std::make_unique<NodeBlock>(Token());
		auto max_structs_var = std::make_shared<NodeVariable>(
			std::nullopt,
			strct->name+OBJ_DELIMITER+"MAX_STRUCTS",
			TypeRegistry::Integer,
			DataType::Const,
			Token()
		);
		max_structs_var->is_engine = true;
		strct->max_individual_struts_var = max_structs_var;
		auto max_structs_decl = std::make_unique<NodeSingleDeclaration>(
			max_structs_var,
			get_max_individual_structs_size(strct),
			Token()
		);
		// add free_idx variable and allocation array to struct
		auto free_idx_var = std::make_shared<NodeVariable>(
			std::nullopt,
			strct->name+OBJ_DELIMITER+"free_idx",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		free_idx_var->is_engine = true;
		strct->free_idx_var = free_idx_var;
		auto free_idx_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(free_idx_var),
			nullptr,
			Token()
		);
		auto allocation_var = std::make_shared<NodeArray>(
			std::nullopt,
			strct->name+OBJ_DELIMITER+"allocation",
			TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Integer),
			strct->max_individual_struts_var->to_reference(),
			Token()
		);
		allocation_var->is_engine = true;
		strct->allocation_var = allocation_var;
		auto allocation_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(allocation_var),
			nullptr,
			Token()
		);
		auto stack_var = std::make_shared<NodeArray>(
			std::nullopt,
			strct->name+OBJ_DELIMITER+"stack",
			TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Integer),
			strct->max_individual_struts_var->to_reference(),
			Token()
		);
		stack_var->is_engine= true;
		strct->stack_var = stack_var;
		auto stack_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(stack_var),
			nullptr,
			Token()
		);
		// add free_idx variable and allocation array to struct
		auto stack_top_var = std::make_shared<NodeVariable>(
			std::nullopt,
			strct->name+OBJ_DELIMITER+"stack_top",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		stack_top_var->is_engine = true;
		strct->stack_top_var = stack_top_var;
		auto stack_top_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(stack_top_var),
			nullptr,
			Token()
		);
		node_block->add_as_stmt(std::move(max_structs_decl));
		node_block->add_as_stmt(std::move(free_idx_decl));
		node_block->add_as_stmt(std::move(allocation_decl));
		node_block->add_as_stmt(std::move(stack_decl));
		node_block->add_as_stmt(std::move(stack_top_decl));
		strct->members->prepend_body(std::move(node_block));
	}


public:

	/// uses the member table to determine the size max size of struct allocations
	/// declare const Node.MAX_STRUCTS: int := MAX_STRUCTS/_max(10, 11*12)
	/// returns either MAX_STRUCTS or binary_expr with biggest array member size
	inline std::unique_ptr<NodeAST> get_max_individual_structs_size(NodeStruct* object) {
		std::vector<std::unique_ptr<NodeAST>> sizes;
		for(auto & data : object->member_table) {
			auto member = data.second.lock();
			if(member->get_node_type() == NodeType::Array) {
				auto node_array = static_pointer_cast<NodeArray>(member);
				sizes.push_back(node_array->size->clone());
			} else if(member->get_node_type() == NodeType::NDArray) {
				auto node_ndarray = static_pointer_cast<NodeNDArray>(member);
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
		std::string func_name = "Struct"+OBJ_DELIMITER+"max";
		return std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
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
		return unique_ptr_cast<NodeFunctionCall>(std::move(max_call));
	}

	/**
	 * function _max(a: int, b: int) -> result
	 * 	result := (a + b + abs(a - b)) / 2
	 * end function
	 */
	static inline bool add_max_function_def(NodeProgram* program) {
		std::string func_name = "Struct"+OBJ_DELIMITER+"max";

		// Prüfen, ob die Funktion bereits existiert
		auto it = program->function_lookup.find({func_name, 2});
		if (it != program->function_lookup.end()) {
			return false; // Funktion existiert bereits
		}

		// Erstellung der Parameter a und b
		auto node_a = std::make_shared<NodeVariable>(std::nullopt, "a", TypeRegistry::Integer, DataType::Param, Token());
		auto node_b = std::make_shared<NodeVariable>(std::nullopt, "b", TypeRegistry::Integer, DataType::Param, Token());

		// Referenzen auf die Parameter
		auto node_a_ref = node_a->to_reference();
		auto node_b_ref = node_b->to_reference();

		// Aufbau der Funktionsdefinition
		auto node_function_header = std::make_unique<NodeFunctionHeader>(func_name, Token());
		node_function_header->add_param(std::move(node_a));
		node_function_header->add_param(std::move(node_b));
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::move(node_function_header),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token()),
			Token()
		);
		node_function_def->body->scope = true;
		node_function_def->num_return_params = 1;
		node_function_def->num_return_stmts = 1;
		node_function_def->ty = TypeRegistry::Integer;

		// abs(a - b)
		auto node_abs_func = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
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
		program->add_function_definition(std::move(node_function_def));
		return true;
	}

};