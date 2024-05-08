//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"

ASTCollectLowerings::ASTCollectLowerings(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTCollectLowerings::visit(NodeSingleDeclareStatement &node) {
	if(node.assignee) node.assignee->accept(*this);
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeSingleAssignStatement& node) {
	node.array_variable->accept(*this);
	if(node.assignee) node.assignee->accept(*this);
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeGetControlStatement& node) {
	node.ui_id->accept(*this);
	// go back to the parent if it is a single assign statement
	if(node.parent->get_node_type() == NodeType::SingleAssignStatement) {
		return;
	}
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeFunctionCall& node) {
	node.function->accept(*this);
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeNDArray& node) {
	node.indexes->accept(*this);
	node.sizes->accept(*this);
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeListStructRef& node) {
	node.indexes->accept(*this);
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeListStruct& node) {
	if(auto lowering = node.get_lowering(m_def_provider)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeConstStatement &node) {
    if(auto lowering = node.get_lowering(m_def_provider)) {
        node.accept(*lowering);
    }
    node.replace_with(std::move(node.constants));
}

//void ASTCollectLowerings::visit(NodeFamilyStatement &node) {
//    node.members->accept(*this);
//    if(auto lowering = node.get_lowering()) {
//        node.accept(*lowering);
//    }
//    node.replace_with(std::move(node.members));
//}




