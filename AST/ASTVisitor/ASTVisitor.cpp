//
// Created by Mathias Vatter on 05.11.23.
//

#include "ASTVisitor.h"


CompileError ASTVisitor::get_raw_compile_error(ErrorType err_type, const NodeAST &node) {
	return CompileError(err_type, "", "", node.tok);
}

std::unique_ptr<NodeBlock> ASTVisitor::make_while_loop(NodeReference* var, int32_t from, int32_t to, std::unique_ptr<NodeBlock> body, NodeAST* parent) {
    auto node_body = std::make_unique<NodeBlock>(var->tok);
    auto node_assignment = std::make_unique<NodeSingleAssignment>(
		clone_as<NodeReference>(var),
		std::make_unique<NodeInt>(from, var->tok), var->tok
		);
    node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(node_assignment), var->tok));
    auto node_comparison = std::make_unique<NodeBinaryExpr>(
		token::LESS_THAN,
		var->clone(),
		std::make_unique<NodeInt>(to, var->tok), var->tok
	);

    body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(var)));
    auto node_while = std::make_unique<NodeWhile>(
		std::move(node_comparison),
		std::move(body), var->tok
	);
    node_body->add_as_stmt(std::move(node_while));
    return std::move(node_body);
}

std::unique_ptr<NodeIf> ASTVisitor::make_nil_check(std::unique_ptr<NodeReference> ref) {
	auto tok = ref->tok;
	return std::make_unique<NodeIf>(
		std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			std::move(ref),
			std::make_unique<NodeNil>(tok),
			tok
		),
		std::make_unique<NodeBlock>(tok, true),
		std::make_unique<NodeBlock>(tok, true),
		tok
	);
}

