//
// Created by Mathias Vatter on 07.05.24.
//

#include "ASTOptimizations.h"



NodeAST * ASTOptimizations::visit(NodeBinaryExpr& node) {
	static ConstantFolding constant_folding;
	node.accept(constant_folding);
	return &node;
}

NodeAST * ASTOptimizations::visit(NodeSingleDeclaration& node) {
	// remove unused variables -> do not remove UIControls
	if(!node.variable->is_used and node.variable->get_node_type() != NodeType::UIControl) {
		return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
	}

	node.variable->accept(*this);
	if(node.value) node.value->accept(*this);
	return &node;
}

NodeAST * ASTOptimizations::visit(NodeBlock& node) {
    for(auto& statement : node.statements) {
        statement->accept(*this);
    }
	node.flatten();
	return &node;
}