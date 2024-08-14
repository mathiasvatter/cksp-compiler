//
// Created by Mathias Vatter on 05.11.23.
//

#include "ASTVisitor.h"


CompileError ASTVisitor::get_raw_compile_error(ErrorType err_type, const NodeAST &node) {
	return CompileError(err_type, "", "", node.tok);
}

std::unique_ptr<NodeBlock> ASTVisitor::make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeBlock> body, NodeAST* parent) {
    auto node_body = std::make_unique<NodeBlock>(var->tok);
    auto node_assignment = std::make_unique<NodeSingleAssignment>(
		var->clone(),
		std::make_unique<NodeInt>(from, var->tok), var->tok
		);
    node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assignment), var->tok));
    auto node_comparison = std::make_unique<NodeBinaryExpr>(
		token::LESS_THAN,
		var->clone(),
		std::make_unique<NodeInt>(to, var->tok), var->tok
	);
	auto node_increment = std::make_unique<NodeStatement>(
		std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"inc",
				std::make_unique<NodeParamList>(var->tok,var->clone()),
				var->tok
			),
			var->tok
		),
		var->tok
	);
    body->statements.push_back(std::move(node_increment));
    auto node_while = std::make_unique<NodeWhile>(
		std::move(node_comparison),
		std::move(body), var->tok
	);
    node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_while), var->tok));
    return std::move(node_body);
}

