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

std::unique_ptr<NodeStatement> ASTVisitor::make_declare_variable(const std::string& name, int32_t value, DataType type, NodeAST* parent) {
    auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), name, type, parent->tok);
    node_variable->type = ASTType::Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), make_int(value, parent), parent->tok);
    node_declare_statement->assignee->parent = node_declare_statement.get();
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeStatement> ASTVisitor::make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent) {
    auto node_array = std::make_unique<NodeArray>(std::optional<Token>(), name, DataType::Array, make_int(size, parent), nullptr, parent->tok);
    node_array->type = ASTType::Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), make_init_array_list(values, parent), parent->tok);
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    node_declare_statement->assignee->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeBody> ASTVisitor::array_initialization(NodeArray* array, NodeParamList* list) {
    auto node_body = std::make_unique<NodeBody>(array->tok);
    auto node_array = clone_as<NodeArray>(array);
    for(int i = 0; i<list->params.size(); i++) {
        auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(list->params[i]->tok);
        node_array->index = make_int((int32_t)i, node_array.get());
        node_assign_statement->array_variable = node_array->clone();
        node_assign_statement->assignee = std::move(list->params[i]);
        node_assign_statement->update_parents(node_body.get());
        node_body->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_body.get()));
    }
    return std::move(node_body);
}

std::unique_ptr<NodeArray> ASTVisitor::make_array(const std::string &name, int32_t size, const Token& tok, NodeAST *parent) {
    auto node_array = std::make_unique<NodeArray>(std::optional<Token>(), name, DataType::Array, nullptr, nullptr, tok);
    node_array->size = make_int(size, node_array.get());
    node_array->update_parents(parent);
    return std::move(node_array);
}

std::unique_ptr<NodeBody> ASTVisitor::make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeBody> body, NodeAST* parent) {
    auto node_body = std::make_unique<NodeBody>(var->tok);

    auto node_assignment = std::make_unique<NodeSingleAssignStatement>(var->clone(), make_int(from, var), var->tok);
    node_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_body.get()));
    auto node_comparison = make_binary_expr(ASTType::Comparison, "<", var->clone(), make_int(to, var), nullptr, var->tok);
    std::vector<std::unique_ptr<NodeAST>> func_args;
    func_args.push_back(var->clone());
    auto node_increment = make_function_call("inc", std::move(func_args), nullptr, var->tok);
    body->statements.push_back(std::move(node_increment));
    auto node_while = std::make_unique<NodeWhileStatement>(std::move(node_comparison), std::move(body), var->tok);
    node_body->statements.push_back(statement_wrapper(std::move(node_while), node_body.get()));
    node_body->update_parents(parent);
    return std::move(node_body);
}

void ASTVisitor::add_vector_to_statement_list(std::unique_ptr<NodeBody> &list, std::vector<std::unique_ptr<NodeStatement>> stmts) {
    list->statements.insert(list->statements.end(),std::make_move_iterator(stmts.begin()),std::make_move_iterator(stmts.end()));
}

std::vector<std::unique_ptr<NodeStatement>> ASTVisitor::cleanup_node_body(NodeBody* node) {
    std::vector<std::unique_ptr<NodeStatement>> temp;
    for(int i = 0; i < node->statements.size(); ++i) {
        if(auto node_body = cast_node<NodeBody>(node->statements[i]->statement.get())) {
            // Übertragen Sie die function_inlines vom aktuellen NodeBody-Element
            // auf das erste Element der inneren NodeBody
            auto& inner_statements = node_body->statements;
            if (!inner_statements.empty()) {
                inner_statements[0]->function_inlines.insert(
                        inner_statements[0]->function_inlines.end(),
                        std::make_move_iterator(node->statements[i]->function_inlines.begin()),
                        std::make_move_iterator(node->statements[i]->function_inlines.end())
                );
            }
            // Aktualisieren Sie das parent-Attribut für jedes innere Statement
            for (auto& stmt : inner_statements) {
                stmt->parent = node;
            }
            // Fügen Sie die inneren Statements zum temporären Vector hinzu
            temp.insert(
                    temp.end(),
                    std::make_move_iterator(inner_statements.begin()),
                    std::make_move_iterator(inner_statements.end())
            );
            // Überspringen Sie das Hinzufügen des aktuellen NodeBody-Elements zu `temp`
            continue;
        }
        // Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht speziell behandelt wird
        temp.push_back(std::move(node->statements[i]));
    }
    // Ersetzen Sie die alte Liste durch die neue
    return std::move(temp);
}

bool ASTVisitor::is_to_be_declared(NodeAST *node) {
	// check if parent is declare statement and if yes then node is on the left side
	auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node->parent);
	return node_declare_statement and node == node_declare_statement->to_be_declared.get();
}