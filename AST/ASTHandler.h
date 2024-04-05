//
// Created by Mathias Vatter on 30.03.24.
//

#pragma once

#include "AST.h"

/// Lowering of data structures to simpler data structures
/// e.g. Lists to arrays, multidimensional arrays to arrays
class ASTHandler {
public:
    virtual void check_syntax(NodeAST& node) {};
    virtual void perform_desugaring(NodeAST& node) {};
    virtual std::unique_ptr<NodeAST> perform_lowering(NodeAST& node) {return nullptr;};

    virtual std::unique_ptr<NodeAST> perform_lowering(NodeSingleDeclareStatement& node) {return nullptr;};
    virtual std::unique_ptr<NodeAST> perform_lowering(NodeArray& node) {return nullptr;};


    virtual ~ASTHandler() = default;

protected:
    inline std::unique_ptr<NodeArray> make_array(const std::string &name, int32_t size, const Token& tok, NodeAST *parent) {
        auto node_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, tok));
        auto node_int = make_int(size, node_sizes.get());
        node_sizes->params.push_back(std::move(node_int));
        auto node_indexes = std::unique_ptr<NodeParamList>(new NodeParamList({}, tok));
        auto node_array = std::make_unique<NodeArray>(std::optional<Token>(), name, VarType::Array, std::move(node_sizes), std::move(node_indexes), tok);
        node_array->indexes->parent = node_array.get();
        node_array->sizes->parent = node_array.get();
        node_array->parent = parent;
        return std::move(node_array);
    }

    inline std::unique_ptr<NodeInt> make_int(int32_t value, NodeAST* parent) {
        auto node_int = std::make_unique<NodeInt>(value, parent->tok);
        node_int->parent = parent;
        return node_int;
    }
};

class SingleDeclareStatementHandler : public ASTHandler {
public:
    std::unique_ptr<NodeAST> perform_lowering(NodeSingleDeclareStatement& node) override {
        auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
        if(auto* node_array = dynamic_cast<NodeArray*>(node.to_be_declared.get())) {
            if(node_array->dimensions > 1) {
                for (int i = 0; i < node_array->sizes->params.size(); i++) {
                    auto node_var = std::make_unique<NodeVariable>(std::optional<Token>(),node_array->name + ".SIZE_D" + std::to_string(i + 1),Const, node.tok);
                    auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_var),node_array->sizes->params[i]->clone(),node.tok);
                    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), node.tok);
                    node_statement_list->statements.push_back(std::move(node_statement));
                }
                auto node_arr = node_array->get_handler()->perform_lowering(*node_array);
                node.to_be_declared = std::move(node_arr);
                node_statement_list->statements.push_back(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
                return node_statement_list;
            }
        }
        return std::move(node.clone());
    }
};

class ArrayHandler : public ASTHandler {
public:
    std::unique_ptr<NodeAST> perform_lowering(NodeArray& node) override {
        // handle multidimensional arrays
        if(node.dimensions > 1) {
            // dynamic cast to check if parent is of type NodeSingleDeclareStatement
            if (dynamic_cast<NodeSingleDeclareStatement *>(node.parent)) {
                auto node_expression = create_right_nested_binary_expr(node.sizes->params, 0, "*", node.tok);
                node_expression->parent = node.sizes.get();
                node.indexes->params.clear();
                node.indexes->params.push_back(std::move(node_expression));
            } else if (dynamic_cast<NodeSingleAssignStatement *>(node.parent)) {
                // convert indexes of multidimensional array
                if (!node.indexes->params.empty()) {
                    auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
                    node.indexes->params.clear();
                    node.indexes->params.push_back(std::move(node_expression));
                    node.indexes->update_parents(&node);
                }
            }
        }
        return std::move(node.clone());
    }
private:
    std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok) {
        // Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
        if (index >= nodes.size() - 1) {
            return nodes[index]->clone();
        }
        // Erstelle die rechte Seite der Expression rekursiv.
        auto right = create_right_nested_binary_expr(nodes, index + 1, op, tok);
        // Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
        return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), tok);
    }
    std::unique_ptr<NodeAST> calculate_index_expression(
            const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {

        // Basisfall: letztes Element in der Berechnung
        if (dimension == indices.size() - 1) {
            return indices[dimension]->clone();
        }

        // Produkt der Größen der nachfolgenden Dimensionen
        std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
        for (size_t i = dimension + 2; i < sizes.size(); ++i) {
            size_product = std::make_unique<NodeBinaryExpr>("*", std::move(size_product), sizes[i]->clone(), tok);
        }

        // Berechnung des aktuellen Teils der Formel
        std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
                "*", indices[dimension]->clone(), std::move(size_product), tok);

        // Rekursiver Aufruf für den nächsten Teil der Formel
        std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

        // Kombinieren des aktuellen Teils mit dem nächsten Teil
        return std::make_unique<NodeBinaryExpr>("+", std::move(current_part), std::move(next_part), tok);
    }
};

