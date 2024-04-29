//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * This class is responsible for desugaring for loops into while loops
 * and takes one for loop at a time to rewrite it as a
 * while loop. Nested for loops need to be handled in the
 * parent ASTVisitor class.
 * The transformation is as follows:
 *
 * Pre-desugaring:
 * for o := 0 downto 4-1
 *     message(4+5)
 * end for
 *
 * Post-desugaring:
 * $o := 0
 * while($o >= 3)
 *     message(9)
 *     dec($o)
 * end while
 */
class DesugarForStatement : public ASTDesugaring {
public:
    void inline visit(NodeForStatement& node) override {
        // function arg
        std::unique_ptr<NodeAST> iterator_var = node.iterator->array_variable->clone();
        std::unique_ptr<NodeAST> assign_var = iterator_var->clone();
        std::unique_ptr<NodeAST> function_var = iterator_var->clone();

        // while statement
        auto node_while_statement = std::make_unique<NodeWhileStatement>(node.tok);
        if(!node.step) {
            // function call
            std::string function_name = "inc";
            if (node.to.type == token::DOWNTO) function_name = "dec";
            std::vector<std::unique_ptr<NodeAST>> args;
            args.push_back(std::move(function_var));
            auto inc_statement = make_function_call(function_name, std::move(args), node_while_statement.get(), node.tok);
            inc_statement->update_parents(&node);
            node.statements->statements.push_back(std::move(inc_statement));
        } else {
            auto assign_var2 = function_var->clone();
            auto inc_expression = make_binary_expr(ASTType::Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
            auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
            auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
            node_statement->update_parents(&node);
            node.statements->statements.push_back(std::move(node_statement));
        }

        // handle while condition
        std::string comparison_op = "<=";
        if(node.to.type == token::DOWNTO) comparison_op = ">=";
        // make comparison expression
        auto comparison = make_binary_expr(ASTType::Comparison, comparison_op, std::move(iterator_var), std::move(node.iterator_end), node_while_statement.get(), node.tok);

        node_while_statement->condition = std::move(comparison);
        node_while_statement->statements = std::move(node.statements);

        auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(node.tok);
        node_assign_statement->array_variable = std::move(assign_var);
        node_assign_statement->assignee = std::move(node.iterator->assignee);

        auto node_body = std::make_unique<NodeBody>(node.tok);
        node_body->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_body.get()));

        node_body->statements.push_back(statement_wrapper(std::move(node_while_statement), node_body.get()));
        node_body->update_parents(node.parent);
        replacement_node = std::move(node_body);
//        node.replace_with(std::move(node_body));
    }
};