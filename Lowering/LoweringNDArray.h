//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"

/// entry points: NodeSingleDeclareStatement
class LoweringNDArray : public ASTLowering {
public:
	explicit LoweringNDArray(DefinitionProvider* def_provider) : ASTLowering(def_provider) {}

	/// returns a statement list with the declarations of the size constants of the array
	void visit(NodeSingleDeclareStatement &node) override {
        if(node.to_be_declared->get_node_type() == NodeType::NDArray) {
            if (auto node_ndarray = cast_node<NodeNDArray>(node.to_be_declared.get())) {
                auto node_body = std::make_unique<NodeBody>(node.tok);
                for (int i = 0; i < node_ndarray->dimensions; i++) {
                    auto node_var = std::make_unique<NodeVariable>(
                            std::optional<Token>(),
                            node_ndarray->name + ".SIZE_D" + std::to_string(i + 1),
                            TypeRegistry::Integer,
                            DataType::Const, node.tok);
                    auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(
                            std::move(node_var),
                            node_ndarray->sizes->params[i]->clone(), node.tok);
                    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), node.tok);
                    node_statement->update_parents(node_body.get());
                    node_body->statements.push_back(std::move(node_statement));
                }
                node.to_be_declared->accept(*this);
                node_body->statements.push_back(std::make_unique<NodeStatement>(node.clone(), node.tok));
                node.replace_with(std::move(node_body));
            }
        }
	}
	/// Lowering of multidimensional arrays to arrays -> declaration
	void visit(NodeNDArray& node) override {
		auto node_expression = create_right_nested_binary_expr(node.sizes->params, 0, token::MULT, node.tok);
        auto node_lowered_array = std::make_unique<NodeArray>(
                node.persistence,
                node.name,
                TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
                std::move(node_expression), node.tok);
		node_lowered_array->name = "_" + node_lowered_array->name;
		node_lowered_array->type = node.type;
		node_lowered_array->parent = node.parent;
		node_lowered_array->is_local = node.is_local;
		node_lowered_array->data_type = node.data_type;
		if(auto lowering = node_lowered_array->get_lowering(m_def_provider)) {
			node_lowered_array->accept(*lowering);
		}
		node.replace_with(std::move(node_lowered_array));
	}

    /// Lowering of multidimensional arrays to arrays when reference
    void visit(NodeNDArrayRef& node) override {
        auto error = CompileError(ErrorType::Variable, "", "", node.tok);
        if(node.indexes->params.size() != node.sizes->params.size()) {
            error.m_message = "Number of indices does not match number of dimensions: " + node.name;
            error.exit();
        }
        // convert index of multidimensional array
        auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
        auto node_lowered_array = std::make_unique<NodeArrayRef>(
                node.name,
                std::move(node_expression), node.tok);
        node_lowered_array->name = "_" + node_lowered_array->name;
        node_lowered_array->type = node.type;
        node_lowered_array->parent = node.parent;
        node_lowered_array->declaration = node.declaration;
		node_lowered_array->update_parents(node.parent);
        node.replace_with(std::move(node_lowered_array));
    }

private:
	std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, token op, const Token& tok) {
		// Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
		if (index >= nodes.size() - 1) {
			return nodes[index]->clone();
		}
		// Erstelle die rechte Seite der Expression rekursiv.
		auto right = create_right_nested_binary_expr(nodes, index + 1, op, tok);
		// Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
		return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), tok);
	}
	std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {
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

