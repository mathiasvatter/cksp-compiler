//
// Created by Mathias Vatter on 30.03.24.
//

#pragma once

#include "../AST/AST.h"

/// Lowering of data structures to simpler data structures
/// e.g. Lists to arrays, multidimensional arrays to arrays
class ASTHandler {
public:
    ASTHandler() = default;
    ~ASTHandler() = default;

    virtual std::unique_ptr<NodeAST> perform_lowering(NodeNDArray& node) {return nullptr;};
	virtual std::unique_ptr<NodeAST> perform_lowering(NodeUIControl& node) {return nullptr;};
    virtual std::unique_ptr<NodeBody> add_members_pre_lowering(DataStructure& node) {return nullptr;};
	virtual std::unique_ptr<NodeBody> add_members_post_lowering(DataStructure& node) {return nullptr;};

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

    static inline std::unique_ptr<NodeInt> make_int(int32_t value, NodeAST* parent) {
        auto node_int = std::make_unique<NodeInt>(value, parent->tok);
        node_int->parent = parent;
        return node_int;
    }
	template<typename T>std::unique_ptr<NodeStatement> statement_wrapper(std::unique_ptr<T> node, NodeAST* parent) {
		auto node_statement = std::make_unique<NodeStatement>(std::move(node), node->tok);
		node_statement->statement->parent = node_statement.get();
		node_statement->parent = parent;
		return node_statement;
	}
	static inline std::unique_ptr<NodeFunctionCall> make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok) {
		auto func_args = std::unique_ptr<NodeParamList>(new NodeParamList(std::move(args), tok));
		// make function header
		auto func = std::make_unique<NodeFunctionHeader>(name, std::move(func_args), tok);
		// make function call out of header
		auto func_call = std::make_unique<NodeFunctionCall>(false, std::move(func), tok);
		func_call->function->parent = func_call.get();
		func_call->update_parents(parent);
		return func_call;
	}
	inline static std::unique_ptr<NodeBinaryExpr> make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok) {
		auto binary_expression = std::make_unique<NodeBinaryExpr>(op, std::move(lhs), std::move(rhs), tok);
		binary_expression->type = type;
		binary_expression->parent = parent;
		binary_expression->left->parent = binary_expression.get();
		binary_expression->right->parent = binary_expression.get();
		return binary_expression;
	}

	inline std::unique_ptr<NodeBody> make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeBody> body, NodeAST* parent) {
		auto node_body = std::make_unique<NodeBody>(var->tok);
		auto node_assignment = std::make_unique<NodeSingleAssignStatement>(var->clone(), make_int(from, var), var->tok);
		node_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_body.get()));
		auto node_comparison = make_binary_expr(ASTType::Comparison, "<", var->clone(), make_int(to, var), nullptr, var->tok);
		std::vector<std::unique_ptr<NodeAST>> func_args;
		func_args.push_back(var->clone());
		auto node_increment = make_function_call("inc", std::move(func_args), nullptr, var->tok);
		node_body->statements.push_back(statement_wrapper(std::move(node_increment), node_body.get()));
		auto node_while = std::make_unique<NodeWhileStatement>(std::move(node_comparison), std::move(body), var->tok);
		node_body->statements.push_back(statement_wrapper(std::move(node_while), node_body.get()));
		node_body->update_parents(parent);
		return std::move(node_body);
	}


};





//class ListHandler : public ASTHandler {
//public:
//    std::unique_ptr<NodeAST> perform_lowering(NodeListStatement& node) override {
//        auto node_body = std::make_unique<NodeBody>(node.tok);
//        auto node_main_array = make_array(node.name, node.size, node.tok, node_body.get());
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
//        node_body->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_body.get()));
//        node_body->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_body.get()));
//
//
//        if(max_dimension == 1) {
//            // bring all one sized param lists into the first
//            for(int i = 1; i<node.body.size(); i++) {
//                node.body[0]->params.push_back(std::move(node.body[i]->params[0]));
//            }
//            add_vector_to_statement_list(node_body, std::move(array_initialization(node_main_array.get(), node.body[0].get())->statements));
//            node_body->update_parents(node.parent);
//            node_body->accept(*this);
//            node.replace_with(std::move(node_body));
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
//        node_body->statements.push_back(statement_wrapper(std::move(node_sizes_declaration), node_body.get()));
//        node_body->statements.push_back(statement_wrapper(std::move(node_positions_declaration), node_body.get()));
//
//        auto node_iterator_var = std::make_unique<NodeVariable>(std::optional<Token>(), "_iterator", DataType::Mutable, node.tok);
//        for(int i = 0; i<node.body.size(); i++) {
//            auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//            auto node_array = make_array(name_wo_ident+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
//            node_array_declaration->to_be_declared = node_array->clone();
//            node_array_declaration->assignee = std::move(node.body[i]);
//            node_body->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_body.get()));
//
//            auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//            auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), name_wo_ident+std::to_string(i)+".SIZE", DataType::Const, node.tok);
//            node_const_declaration->to_be_declared = std::move(node_variable);
//            node_const_declaration->assignee = make_int(sizes[i], node_const_declaration.get());
//            node_body->statements.push_back(statement_wrapper(std::move(node_const_declaration), node_body.get()));
//
//            auto node_while_body = std::make_unique<NodeBody>(node.tok);
//            auto node_expression = make_binary_expr(ASTType::Integer, "+", node_iterator_var->clone(), make_int(positions[i], &node),nullptr, node.tok);
//            node_main_array->indexes->params.clear();
//            node_main_array->indexes->params.push_back(std::move(node_expression));
//
//            node_array->indexes->params.push_back(node_iterator_var->clone());
//            auto node_main_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_main_array->clone().release()));
//            node_main_array_copy->name = "_"+node_main_array_copy->name;
//            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_main_array_copy), std::move(node_array), node.tok);
//            node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
//            auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_body.get());
//            node_body->statements.push_back(statement_wrapper(std::move(node_while_loop), node_body.get()));
//        }
//        node_body->update_parents(node.parent);
//        node_body->accept(*this);
//        node.replace_with(std::move(node_body));
//    }
//};

