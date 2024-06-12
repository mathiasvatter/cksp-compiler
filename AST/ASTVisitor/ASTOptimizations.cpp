//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTOptimizations.h"



void ASTOptimizations::visit(NodeBinaryExpr& node) {
	static ConstantFolding constant_folding;
	node.accept(constant_folding);
}

void ASTOptimizations::visit(NodeSingleDeclaration& node) {
	// remove unused variables -> do not remove UIControls
	if(!node.variable->is_used and node.variable->get_node_type() != NodeType::UIControl) {
		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		return;
	}

	node.variable->accept(*this);
	if(node.value) node.value->accept(*this);
}

void ASTOptimizations::visit(NodeBody& node) {
    for(auto& statement : node.statements) {
        statement->accept(*this);
    }
    node.cleanup_body();
}