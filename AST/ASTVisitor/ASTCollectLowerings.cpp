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

	/*
	 * checks if the parent of the current node is a single assignment statement.
	 * If the array_variable of the assignment is the get_control statement, then it is a set_control statement.
	 * E.g. if it would be
	 * 	int_buffer := mnu_output[i+1]->x, this block of code would need to handle it.
	 * However, in the case of:
	 * 	mnu_output[i+1]->x := int_buffer, the NodeSingleAssignStatement lowering would handle it.
	 */
	if(node.parent->get_node_type() == NodeType::SingleAssignStatement) {
		if(auto node_assign_stmt = cast_node<NodeSingleAssignStatement>(node.parent)) {
			if(node_assign_stmt->array_variable.get() == &node) return;
		}
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

void ASTCollectLowerings::visit(NodeArray& node) {
    node.size->accept(*this);
    if(auto lowering = node.get_lowering(m_def_provider)) {
        node.accept(*lowering);
    }
}

void ASTCollectLowerings::visit(NodeNDArrayRef& node) {
	node.indexes->accept(*this);
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




