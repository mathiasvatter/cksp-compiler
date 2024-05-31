//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTOptimizations.h"



void ASTOptimizations::visit(NodeBinaryExpr& node) {
	node.accept(constant_folding);
}

void ASTOptimizations::visit(NodeSingleDeclareStatement& node) {
	// remove unused variables -> do not remove UIControls
//	if(!node.to_be_declared->is_used and node.to_be_declared->get_node_type() != NodeType::UIControl) {
//		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//		return;
//	}

	node.to_be_declared->accept(*this);
	if(node.assignee) node.assignee->accept(*this);
}

void ASTOptimizations::visit(NodeBody& node) {
    for(auto& statement : node.statements) {
        statement->accept(*this);
    }
    node.cleanup_body();
}