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



