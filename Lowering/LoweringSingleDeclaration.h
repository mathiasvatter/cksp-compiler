//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Checks declarations of ndarrays and adds array size constants
 *
 */
class LoweringSingleDeclaration : public ASTLowering {
private:
	static std::unique_ptr<NodeAST> get_lowered_size_expr(NodeNDArray& ref) {
		// in case of function param -> sizes are not set
		if(!ref.sizes) {
			return nullptr;
		}
		return NodeBinaryExpr::create_right_nested_binary_expr(ref.sizes->params, 0, token::MULT);
	}
public:
	explicit LoweringSingleDeclaration(NodeProgram* program) : ASTLowering(program) {}

	/**
	 * returns a statement list with the declarations of the size constants of the array
	 * Pre Lowering of NDArray to Array
	 *  declare ndarray[3, 3]: int[][] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 * Post Lowering:
	 * 	declare const ndarray::num_elements[3] := (3*3, 3, 3)
	 * 	declare const ndarray.SIZE := 3 * 3 <- removed
	 *  declare const ndarray.SIZE_D1 := 3 <- removed
	 * 	declare const ndarray.SIZE_D2 := 3 <- removed
	 * 	declare ndarray[3, 3]: int[][] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 */
	NodeAST* visit(NodeSingleDeclaration &node) override {
		if(node.variable->get_node_type() == NodeType::NDArray) {
			auto node_ndarray = static_cast<NodeNDArray*>(node.variable.get());
			if(!node_ndarray->sizes) {
				auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
				error.m_message = "Unable to infer array size. Size of NDArray has to be determined at compile time.";
				error.exit();
			}
			auto node_body = std::make_unique<NodeBlock>(node.tok);
			auto node_num_elements_decl = std::make_unique<NodeSingleDeclaration>(
				std::make_unique<NodeArray>(
					std::optional<Token>(),
					node_ndarray->name + OBJ_DELIMITER+"num_elements",
					TypeRegistry::ArrayOfInt,
					std::make_unique<NodeInt>(node_ndarray->dimensions+1, node.tok),
					node.tok
				),
				node.tok
			);
			node_num_elements_decl->variable->data_type = DataType::Const;
			node_num_elements_decl->variable->is_local = node.variable->is_local;
			// Add to num_elements global list
			m_program->num_element_constants[node.variable.get()] = node_num_elements_decl->variable;
			auto node_init_list = std::make_unique<NodeInitializerList>(
				node.tok,
				get_lowered_size_expr(*node_ndarray)
			);

			for (int i = 0; i < node_ndarray->dimensions; i++) {
				node_init_list->add_element(node_ndarray->sizes->params[i]->clone());
			}
			node_num_elements_decl->set_value(std::move(node_init_list));
			node_body->add_as_stmt(std::move(node_num_elements_decl));
			node_body->add_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok));
			return node.replace_with(std::move(node_body));
		}
		return &node;
	}


};

