//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"
#include "../Desugaring/ASTDesugaring.h"

/// entry points: NodeSingleDeclaration
class LoweringNDArray : public ASTLowering {
private:
	static std::unique_ptr<NodeAST> get_lowered_size_expr(NodeNDArray& ref) {
		// in case of function param -> sizes are not set
		if(!ref.sizes) {
			return nullptr;
		}
		return NodeBinaryExpr::create_right_nested_binary_expr(ref.sizes->params, 0, token::MULT);
	}
public:
	explicit LoweringNDArray(NodeProgram* program) : ASTLowering(program) {}

	NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value->accept(*this);
		return &node;
	}

	/**
	 * returns a statement list with the declarations of the size constants of the array
	 * Pre Lowering of NDArray to Array
	 *  declare ndarray[3, 3]: int[][] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 * Post Lowering:
	 * 	declare const ndarray::num_elements[3] := (3*3, 3, 3)
	 * 	declare const ndarray.SIZE := 3 * 3
	 *  declare const ndarray.SIZE_D1 := 3
	 * 	declare const ndarray.SIZE_D2 := 3
	 * 	declare _ndarray[3 * 3]: int[] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 */
	NodeAST* visit(NodeSingleDeclaration &node) override {
        if(node.variable->get_node_type() == NodeType::NDArray) {
			auto node_ndarray = static_cast<NodeNDArray*>(node.variable.get());
			if(!node_ndarray->sizes) {
				auto error = CompileError(ErrorType::Variable, "", "", node.tok);
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
			auto node_init_list = std::make_unique<NodeInitializerList>(
				node.tok,
				get_lowered_size_expr(*node_ndarray)
			);
			auto node_size_decl = std::make_unique<NodeSingleDeclaration>(
					std::make_unique<NodeVariable>(
							std::optional<Token>(),
							node_ndarray->name + ".SIZE",
							TypeRegistry::Integer,
							DataType::Const, node.tok),
					get_lowered_size_expr(*node_ndarray), node.tok);
			node_body->add_as_stmt(std::move(node_size_decl));
			for (int i = 0; i < node_ndarray->dimensions; i++) {
				auto node_var = std::make_unique<NodeVariable>(
						std::optional<Token>(),
						node_ndarray->name + ".SIZE_D" + std::to_string(i + 1),
						TypeRegistry::Integer,
						DataType::Const, node.tok);
				auto node_declaration = std::make_unique<NodeSingleDeclaration>(
						std::move(node_var),
						node_ndarray->sizes->params[i]->clone(), node.tok);
				node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_declaration), node.tok));
				node_init_list->add_element(node_ndarray->sizes->params[i]->clone());
			}
			node_num_elements_decl->set_value(std::move(node_init_list));
			node_body->add_as_stmt(std::move(node_num_elements_decl));
			node.variable->accept(*this);
			node_body->add_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok));
			node_body->flatten();
			return node.replace_with(std::move(node_body));
        }
		return &node;
	}
	/// Lowering of multidimensional arrays to arrays -> declaration
	NodeAST* visit(NodeNDArray& node) override {
        auto node_lowered_array = std::make_unique<NodeArray>(
                node.persistence,
                node.name,
                TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
				get_lowered_size_expr(node), node.tok);
		node_lowered_array->name = "_" + node_lowered_array->name;
		node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		node_lowered_array->parent = node.parent;
		node_lowered_array->is_local = node.is_local;
		node_lowered_array->data_type = node.data_type;
		// call array lowering
		node_lowered_array->lower(m_program);
		return node.replace_with(std::move(node_lowered_array));
	}

    /// Lowering of multidimensional arrays to arrays when reference
	NodeAST* visit(NodeNDArrayRef& node) override {
        auto error = CompileError(ErrorType::Variable, "", "", node.tok);
        if(node.indexes and node.sizes and node.indexes->size() != node.sizes->size()) {
            error.m_message = "Number of indices does not match number of dimensions: " + node.name;
            error.exit();
        }
        // convert index of multidimensional array
        std::unique_ptr<NodeAST> node_expression = nullptr;
		if(node.indexes) {
			// if no sizes -> ndarray is func param
			if(!node.sizes) {
				error.m_message = "Unable to infer array size. Size of <NDArrayRef> has to be determined at compile time.";
				error.exit();
			}
			node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
		}
        auto node_lowered_array = std::make_unique<NodeArrayRef>(
                node.name,
                std::move(node_expression), node.tok);
        node_lowered_array->name = "_" + node_lowered_array->name;
//        node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
        node_lowered_array->ty = node.ty->get_element_type();
		// if no sizes -> ndarray is func param and needs to have another type dimension
		if(!node.indexes) {
			node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		}
		node_lowered_array->data_type = node.data_type;
        node_lowered_array->parent = node.parent;
        node_lowered_array->declaration = node.declaration;
		node_lowered_array->update_parents(node.parent);
        return node.replace_with(std::move(node_lowered_array));
    }

//	NodeAST* visit(NodeFor& node) override {
//		node.body->accept(*this);
//		return node.desugar(m_program)->accept(*this);
//	}

	static std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {
		// Basisfall: letztes Element in der Berechnung
		if (dimension == indices.size() - 1) {
			return indices[dimension]->clone();
		}
		// Produkt der Größen der nachfolgenden Dimensionen
		std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
		for (size_t i = dimension + 2; i < sizes.size(); ++i) {
			size_product = std::make_unique<NodeBinaryExpr>(token::MULT, std::move(size_product), sizes[i]->clone(), tok);
		}
		// Berechnung des aktuellen Teils der Formel
		std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
			token::MULT, indices[dimension]->clone(), std::move(size_product), tok);

		// Rekursiver Aufruf für den nächsten Teil der Formel
		std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

		// Kombinieren des aktuellen Teils mit dem nächsten Teil
		return std::make_unique<NodeBinaryExpr>(token::ADD, std::move(current_part), std::move(next_part), tok);
	}

};

