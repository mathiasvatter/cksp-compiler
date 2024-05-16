//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTOptimizations.h"

ASTOptimizations::ASTOptimizations(std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines) : m_function_inlines(std::move(m_function_inlines)) {}

void ASTOptimizations::visit(NodeStatement& node) {
    if(!node.function_inlines.empty()) {
        auto node_body = std::make_unique<NodeBody>(node.function_inlines[0]->tok);
        node_body->parent = &node;
        for(auto & func : node.function_inlines) {
            auto it = m_function_inlines.find(func);
            node_body->statements.push_back(std::move(it->second));
        }
        node_body->statements.push_back(statement_wrapper(std::move(node.statement), &node));
        node_body->accept(*this);
        node.statement = std::move(node_body);
    } else {
        node.statement->accept(*this);
    }
}

void ASTOptimizations::visit(NodeBinaryExpr& node) {
	node.accept(constant_folding);
}

void ASTOptimizations::visit(NodeSingleDeclareStatement& node) {
	// remove unused variables -> do not remove UIControls
	if(!node.to_be_declared->is_used and node.to_be_declared->get_node_type() != NodeType::UIControl) {
		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		return;
	}

	node.to_be_declared->accept(*this);
	if(node.assignee) node.assignee->accept(*this);
}