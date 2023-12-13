//
// Created by Mathias Vatter on 02.12.23.
//

#include "ASTTypeChecking.h"


void ASTTypeChecking::visit(NodeProgram& node) {
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
}

void ASTTypeChecking::visit(NodeVariable& node) {

    // only print error if it is in a declaration
    if(node.type == Unknown) {
        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
            CompileError(ErrorType::TypeError,"Could not infer variable type.", node.tok.line, "", node.name, node.tok.file).print();
		} else {
			node.type = node.declaration->type;
		}
    }
}

void ASTTypeChecking::visit(NodeArray& node) {
    // only print error if it is in a declaration
    if(node.type == Unknown) {
        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
            CompileError(ErrorType::TypeError,"Could not infer array type.", node.tok.line, "", node.name, node.tok.file).print();
        } else {
			node.type = node.declaration->type;
		}
    }
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);

}
