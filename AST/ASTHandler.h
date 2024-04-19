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
    virtual std::unique_ptr<NodeAST> perform_lowering(NodeListStatement& node) {return nullptr;};


    virtual ~ASTHandler() = default;

protected:
    inline std::unique_ptr<NodeArray> make_array(const std::string &name, int32_t size, const Token& tok, NodeAST *parent) {
        auto node_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, tok));
        auto node_int = make_int(size, node_sizes.get());
        node_sizes->params.push_back(std::move(node_int));
        auto node_indexes = std::unique_ptr<NodeParamList>(new NodeParamList({}, tok));
        auto node_array = std::make_unique<NodeArray>(std::optional<Token>(), name, DataType::Array, std::move(node_sizes), std::move(node_indexes), tok);
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
            if (!node.is_reference) {
                auto node_expression = create_right_nested_binary_expr(node.sizes->params, 0, "*", node.tok);
                node_expression->parent = node.sizes.get();
                node.indexes->params.clear();
                node.indexes->params.push_back(std::move(node_expression));
            } else {
                // convert indexes of multidimensional array
                if (!node.indexes->params.empty()) {
                    auto node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
                    node.indexes->params.clear();
                    node.indexes->params.push_back(std::move(node_expression));
                    node.indexes->update_parents(&node);
                }
            }
        }
//        if(node.data_type == List) {
//            if(node.indexes->params.size() != 2) {
//                CompileError(ErrorType::SyntaxError,"Got wrong amount of indexes for <list>.", node.tok.line, "2", std::to_string(node.indexes->params.size()), node.tok.file).print();
//                exit(EXIT_FAILURE);
//            }
//            auto node_position_array = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(
//                                                                          get_declared_array(node.name+".pos")->clone().release()));
//            node_position_array->indexes->params.clear();
//            node_position_array->indexes->params.push_back(std::move(node.indexes->params[0]));
//
//            auto node_expression = make_binary_expr(ASTType::Integer, "+", std::move(node_position_array), std::move(node.indexes->params[1]), &node, node.tok);
//            node.indexes->params.clear();
//            node.indexes->params.push_back(std::move(node_expression));
//            node.indexes->update_parents(&node);
//            node.name = "_"+node.name;
//        }
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

class ListHandler : public ASTHandler {
public:
    std::unique_ptr<NodeAST> perform_lowering(NodeListStatement& node) override {
//        auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
//        auto node_main_array = make_array(node.name, node.size, node.tok, node_statement_list.get());
//        // accept first to get rid of array identifier
//        node_main_array->accept(*this);
//        std::string name_wo_ident = node_main_array->name;
////    node_main_array->name = "_"+node_main_array->name;
//        //check dimension -> if only 1 then treat as an array
//        int max_dimension = 0;
//        for(auto & param : node.body) {
//            max_dimension = std::max(max_dimension, (int)param->params.size());
//        }
//        if(max_dimension>1) node_main_array->data_type = List;
//
//        auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(node_main_array->clone(), nullptr, node.tok);
//        auto main_size = (int32_t)node.body.size();
//        auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+".SIZE", DataType::Const, node.tok), make_int(main_size,&node), node.tok);
//        node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_statement_list.get()));
//        node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_statement_list.get()));
//
//
//        if(max_dimension == 1) {
//            // bring all one sized param lists into the first
//            for(int i = 1; i<node.body.size(); i++) {
//                node.body[0]->params.push_back(std::move(node.body[i]->params[0]));
//            }
//            add_vector_to_statement_list(node_statement_list, std::move(array_initialization(node_main_array.get(), node.body[0].get())->statements));
//            node_statement_list->update_parents(node.parent);
//            node_statement_list->accept(*this);
//            node.replace_with(std::move(node_statement_list));
//            return;
//        }
//
//        auto node_sizes_array = make_array(name_wo_ident+".sizes", main_size, node.tok, nullptr);
//        auto node_positions_array = make_array(name_wo_ident+".pos", main_size, node.tok, nullptr);
//        std::vector<int32_t> sizes(node.body.size());
//        std::vector<int32_t> positions(node.body.size());
//        auto node_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
//        auto node_positions = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
//        positions[0] = 0;
//        for(int i = 0; i<node.body.size(); i++) {
//            sizes[i] = static_cast<int32_t>(node.body[i]->params.size());
//            if(i>0) positions[i] = positions[i - 1] + sizes[i - 1];
////        std::cout << sizes[i] << ", " << positions[i] << std::endl;
//            auto node_size = make_int(sizes[i], node_sizes.get());
//            node_sizes->params.push_back(std::move(node_size));
//            auto node_position = make_int(positions[i], node_positions.get());
//            node_positions->params.push_back(std::move(node_position));
//        }
//        auto node_sizes_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_sizes_array), std::move(node_sizes), node.tok);
//        auto node_positions_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_positions_array), std::move(node_positions), node.tok);
//        node_statement_list->statements.push_back(statement_wrapper(std::move(node_sizes_declaration), node_statement_list.get()));
//        node_statement_list->statements.push_back(statement_wrapper(std::move(node_positions_declaration), node_statement_list.get()));
//
//        auto node_iterator_var = std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, node.tok);
//        for(int i = 0; i<node.body.size(); i++) {
//            auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//            auto node_array = make_array(name_wo_ident+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
//            node_array_declaration->to_be_declared = node_array->clone();
//            node_array_declaration->assignee = std::move(node.body[i]);
//            node_statement_list->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_statement_list.get()));
//
//            auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//            auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+std::to_string(i)+".SIZE", DataType::Const, node.tok);
//            node_const_declaration->to_be_declared = std::move(node_variable);
//            node_const_declaration->assignee = make_int(sizes[i], node_const_declaration.get());
//            node_statement_list->statements.push_back(statement_wrapper(std::move(node_const_declaration), node_statement_list.get()));
//
//            auto node_while_body = std::make_unique<NodeStatementList>(node.tok);
//            auto node_expression = make_binary_expr(ASTType::Integer, "+", node_iterator_var->clone(), make_int(positions[i], &node),nullptr, node.tok);
//            node_main_array->indexes->params.clear();
//            node_main_array->indexes->params.push_back(std::move(node_expression));
//
//            node_array->indexes->params.push_back(node_iterator_var->clone());
//            auto node_main_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_main_array->clone().release()));
//            node_main_array_copy->name = "_"+node_main_array_copy->name;
//            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_main_array_copy), std::move(node_array), node.tok);
//            node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
//            auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_statement_list.get());
//            node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_loop), node_statement_list.get()));
//        }
//        node_statement_list->update_parents(node.parent);
//        node_statement_list->accept(*this);
//        node.replace_with(std::move(node_statement_list));
    }
};

