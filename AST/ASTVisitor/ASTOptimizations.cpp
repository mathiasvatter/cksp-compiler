//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTOptimizations.h"
#include "../../Optimization/ConstantFolding.h"

void ASTOptimizations::visit(NodeBinaryExpr& node) {
//	node.left->accept(*this);
//	node.right->accept(*this);

	ConstantFolding constant_folding;
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