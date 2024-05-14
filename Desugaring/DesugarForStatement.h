//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * @brief This class is responsible for desugaring for loops into while loops
 * It takes one for loop at a time to rewrite it as a
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

        if(!node.step) {
            // function call
            std::string function_name = "inc";
            if (node.to.type == token::DOWNTO) function_name = "dec";

            auto node_inc = std::make_unique<NodeFunctionCall>(
                    false,
                    std::make_unique<NodeFunctionHeader>(
                            function_name,
							std::make_unique<NodeParamList>(node.tok, std::move(function_var)),
							node.tok
						),
                    node.tok
            );
            node.statements->statements.push_back(std::make_unique<NodeStatement>(std::move(node_inc), node.tok));
        } else {
            // i := i + step
            auto inc_expression = std::make_unique<NodeBinaryExpr>(
                    token::ADD,
                    function_var->clone(),
                    std::move(node.step), node.tok
                    );
            inc_expression->type = ASTType::Integer;
            auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(
                    std::move(function_var),
                    std::move(inc_expression), node.tok);
            node.statements->statements.push_back(std::make_unique<NodeStatement>(std::move(node_inc_statement), node.tok));
        }

        // handle while condition
        token comparison_op = token::LESS_EQUAL;
        if(node.to.type == token::DOWNTO) comparison_op = token::GREATER_EQUAL;
        // make comparison expression
        auto comparison = std::make_unique<NodeBinaryExpr>(
                comparison_op,
                std::move(iterator_var),
                std::move(node.iterator_end), node.tok
                );
        comparison->type = ASTType::Comparison;

        auto node_while_statement = std::make_unique<NodeWhileStatement>(
                std::move(comparison),
                std::move(node.statements), node.tok
                );

        auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(
                std::move(assign_var),
                std::move(node.iterator->assignee),
                node.tok
                );

        auto node_body = std::make_unique<NodeBody>(node.tok);
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assign_statement), node.tok));
        node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_while_statement), node.tok));
        replacement_node = std::move(node_body);
    }
};