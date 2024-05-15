//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugarStructs.h"

void ASTDesugarStructs::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTDesugarStructs::visit(NodeBody& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(cleanup_node_body(&node));
}

void ASTDesugarStructs::visit(NodeDeclareStatement& node) {
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugarStructs::visit(NodeAssignStatement &node) {
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugarStructs::visit(NodeForEachStatement& node) {
    node.statements->accept(*this);
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        // accept again to desugar resulting for loops
        replacement->accept(*this);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugarStructs::visit(NodeForStatement& node) {
    node.statements->accept(*this);
    if(auto desugaring = node.get_desugaring()) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugarStructs::visit(NodeFamilyStatement &node) {
    node.members->accept(*this);
    if(auto lowering = node.get_lowering(nullptr)) {
        node.accept(*lowering);
    }
    node.replace_with(std::move(node.members));
}

void ASTDesugarStructs::visit(NodeParamList &node) {
	for(auto & param : node.params) {
		param->accept(*this);
	}
	// in case it is a double param_list [[0,1,2,3]] move params up to parent
	if(auto node_param_list = cast_node<NodeParamList>(node.parent)) {
		if(node_param_list->params.size() == 1) {
			node_param_list->params.insert(
				node_param_list->params.begin(),
				std::make_move_iterator(node.params.begin()),
				std::make_move_iterator(node.params.end())
			);
			node_param_list->params.pop_back();
			node_param_list->set_child_parents();
		}
	}
}