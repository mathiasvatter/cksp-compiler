//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringStruct : public ASTLowering {
private:
	NodeStruct* m_current_struct = nullptr;
	NodeVariable* m_free_idx_var = nullptr;
	NodeArray* m_allocation_var = nullptr;
public:
	explicit LoweringStruct(NodeProgram *program) : ASTLowering(program) {}

	void visit(NodeStruct& node) override {
		m_current_struct = &node;

		// check if all member node types are allowed
		for(auto & m: node.member_table) {
			if(NodeStruct::allowed_member_node_types.find(m.second->get_node_type()) == NodeStruct::allowed_member_node_types.end()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", m.second->tok);
				error.m_message = "Member type not allowed in struct. Only <Variables>, <Arrays>, <NDArrays>, <Pointers> and <Structs> are allowed.";
				error.m_got = m.first;
				error.exit();
			}
			node.member_node_types.insert(m.second->get_node_type());
		}

		node.members->accept(*this);
		node.members->prepend_body(add_compiler_struct_vars());
		for(auto & m: node.methods) {
			m->accept(*this);
		}
	}

	std::unique_ptr<NodeBlock> add_compiler_struct_vars() {
		auto node_block = std::make_unique<NodeBlock>(Token());
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
		m_free_idx_var = static_cast<NodeVariable*>(free_idx_decl->variable.get());
		auto allocation_var = std::make_unique<NodeArray>(
			std::nullopt,
			m_current_struct->name+".allocation",
			TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Integer),
			NodeStruct::max_structs_var->to_reference(),
			Token()
		);
		auto allocation_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(allocation_var),
			nullptr,
			Token()
		);
		m_allocation_var = static_cast<NodeArray*>(allocation_decl->variable.get());
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(free_idx_decl), Token()));
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(allocation_decl), Token()));
		return node_block;
	}

	inline void visit(NodeSingleDeclaration& node) override {
		// turn member into array if it is a member
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		// turn node.value into param list if is member and has been turned into array
		if(node.variable->is_member() and node.value) {
			if(node.variable->get_node_type() != NodeType::Array and node.variable->get_node_type() != NodeType::NDArray) {
				return;
			}
			if(node.value->get_node_type() != NodeType::ParamList) {
				auto param_list = std::make_unique<NodeParamList>(node.tok);
				param_list->params.push_back(std::move(node.value));
				param_list->parent = &node;
				node.value = std::move(param_list);
			}
		}
	}

	inline void visit(NodeVariable& node) override {
		// if member, turn into array
		if(node.is_member()) {
			auto node_array = node.to_array(NodeStruct::max_structs_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			node.replace_with(std::move(node_array));
		}
	}
	inline void visit(NodePointer& node) override {
		// if member, turn into array of pointers
		if(node.is_member()) {
			auto node_array = node.to_array(NodeStruct::max_structs_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			node.replace_with(std::move(node_array));
		}
	}
	inline void visit(NodeArray& node) override {
		// if member, turn into multi-dimensional array
		/*
		 * 	declare velocities[10]: [int]
		 * 	declare struct.velocity[MAX_STRUCTS/10, 10]
		 */

	}

private:
	static std::unique_ptr<NodeFunctionCall> create_max_call(NodeAST* left, NodeAST* right) {
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
	inline std::unique_ptr<NodeFunctionCall> find_max_array_size(std::vector<NodeAST*> &sizes) {
		if (sizes.empty()) {
			auto error = CompileError(ErrorType::InternalError, "", "", Token());
			error.m_message = "No sizes given for struct.";
			error.exit();
		}
		std::unique_ptr<NodeAST> max_call = sizes[0]->clone();
		for (size_t i = 1; i < sizes.size(); ++i) {
			max_call = create_max_call(max_call.get(), sizes[i]);
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
		// return variable
		auto result = std::make_unique<NodeVariable>(std::nullopt, "result", TypeRegistry::Integer, DataType::Mutable, Token());

		// Referenzen auf die Parameter
		auto node_a_ref = node_a->to_reference();
		auto node_b_ref = node_b->to_reference();
		auto node_result_ref = result->to_reference();

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
			std::optional(std::move(result)),
			false,
			std::make_unique<NodeBlock>(Token()),
			Token()
		);
		node_function_def->body->scope = true;

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
				std::make_unique<NodeSingleAssignment>(
					node_result_ref->clone(),
					std::move(max_expression),
					Token()
				),
				Token()
			));

		// Fügen Sie die neue Funktionsdefinition zum Programm hinzu
		program->function_definitions.push_back(std::move(node_function_def));

		// Update function lookup so that the new function can be found
		program->update_function_lookup();

		return true;
	}


};