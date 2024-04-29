//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"

ASTCollectLowerings::ASTCollectLowerings(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTCollectLowerings::visit(NodeSingleDeclareStatement &node) {
	if(auto lowering = node.get_lowering()) {
		node.accept(*lowering);
	}
}


void ASTCollectLowerings::visit(NodeNDArray& node) {
	if(auto lowering = node.get_lowering()) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeListStructReference& node) {
	if(auto lowering = node.get_lowering()) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeListStruct& node) {
	if(auto lowering = node.get_lowering()) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeConstStatement &node) {
    if(auto lowering = node.get_lowering()) {
        node.accept(*lowering);
    }
    node.replace_with(std::move(node.constants));
}

void ASTCollectLowerings::visit(NodeFamilyStatement &node) {
    if(auto lowering = node.get_lowering()) {
        node.accept(*lowering);
    }
    node.members->accept(*this);
    node.replace_with(std::move(node.members));
}




