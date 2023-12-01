//
// Created by Mathias Vatter on 05.11.23.
//

#include "ASTVisitor.h"


std::unique_ptr<NodeInt> ASTVisitor::make_int(int32_t value, NodeAST* parent) {
    auto node_int = std::make_unique<NodeInt>(value, parent->tok);
    node_int->parent = parent;
    return node_int;
}

std::unique_ptr<NodeParamList> ASTVisitor::make_init_array_list(const std::vector<int32_t>& values, NodeAST* parent) {
    auto node_array_init_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    node_array_init_list -> parent = parent;
    for(auto & i : values) {
        node_array_init_list->params.push_back(make_int(i, node_array_init_list.get()));
    }
    return node_array_init_list;
}

std::unique_ptr<NodeStatement> ASTVisitor::make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok) {
    auto func_args = std::unique_ptr<NodeParamList>(new NodeParamList(std::move(args), tok));
    for(auto & arg : func_args->params) {
        arg->parent = func_args.get();
    }
    // make function header
    auto func = std::make_unique<NodeFunctionHeader>(name, std::move(func_args), tok);
    func->args->parent = func.get();

    // make function call out of header
    auto func_call = std::make_unique<NodeFunctionCall>(false, std::move(func), tok);
    func_call->function->parent = func_call.get();

    // make statement out of function call
    auto function_call = std::make_unique<NodeStatement>(std::move(func_call), tok);
    function_call -> parent = parent;
    return function_call;
}

std::unique_ptr<NodeBinaryExpr>ASTVisitor::make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok) {
    // make comparison expression
    auto comparison = std::make_unique<NodeBinaryExpr>(op, std::move(lhs), std::move(rhs), tok);
    comparison->type = type;
    comparison->parent = parent;
    comparison->left->parent = comparison.get();
    comparison->right->parent = comparison.get();
    return comparison;
}

std::unique_ptr<NodeStatement> ASTVisitor::make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent) {
    auto node_variable = std::make_unique<NodeVariable>(false, name, type, parent->tok);
    node_variable->type = Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), make_int(value, parent), parent->tok);
    node_declare_statement->assignee->parent = node_declare_statement.get();
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeStatement> ASTVisitor::make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent) {
    auto sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    sizes->params.push_back(make_int(size, parent));
    auto indexes  = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    auto node_array = std::make_unique<NodeArray>(false, name, Array, std::move(sizes), std::move(indexes), parent->tok);
    node_array->sizes->parent = node_array.get();
    node_array->indexes->parent = node_array.get();
    node_array->type = Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), make_init_array_list(values, parent), parent->tok);
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    node_declare_statement->assignee->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeStatementList> ASTVisitor::array_initialization(NodeArray* array, NodeParamList* list) {
    auto node_statement_list = std::make_unique<NodeStatementList>(array->tok);
    auto node_array = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(array->clone().release()));
    for(int i = 0; i<list->params.size(); i++) {
        auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(list->params[i]->tok);
        node_array->indexes->params.clear();
        node_array->indexes->params.push_back(make_int((int32_t)i, node_array->indexes.get()));
        node_assign_statement->array_variable = node_array->clone();
        node_assign_statement->assignee = std::move(list->params[i]);
        node_assign_statement->update_parents(node_statement_list.get());
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_statement_list.get()));
    }
    return std::move(node_statement_list);
}

//
//template<typename T>std::unique_ptr<NodeStatement> ASTVisitor::statement_wrapper(std::unique_ptr<T> node, NodeAST* parent) {
//    auto node_statement = std::make_unique<NodeStatement>(std::move(node), node->tok);
//    node_statement->statement->parent = node_statement.get();
//    node_statement->parent = parent;
//    return node_statement;
//}