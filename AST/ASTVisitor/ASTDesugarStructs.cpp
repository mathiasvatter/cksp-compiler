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

std::unique_ptr<NodeAST> ASTDesugarStructs::get_key_value_substitute(const std::string& name) {
	for (auto rit = m_key_value_scope_stack.rbegin(); rit != m_key_value_scope_stack.rend(); ++rit) {
		auto it = rit->find(name);
		if(it != rit->end()) {
			return it->second->clone();
		}
	}
	return nullptr;
}