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
            CompileError(ErrorType::TypeError,"Could not infer variable type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
			node.type = Integer;
		} else {
			node.type = node.declaration->type;
		}
    }

	if(node.type == Unknown or node.type == Number or node.type == Any) {
		CompileError(ErrorType::TypeError,"Could not infer variable type. Variable is Unknown/Number/Any", node.tok.line, "valid type", node.name, node.tok.file).print();
	}
}

void ASTTypeChecking::visit(NodeArray& node) {
    // only print error if it is in a declaration
    if(node.type == Unknown) {
        auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
        auto node_ui_control = cast_node<NodeUIControl>(node.parent);
        if(node_declaration or node_ui_control) {
			CompileError(ErrorType::TypeError,"Could not infer array type. Automatically casted as <Integer>. Consider using a variable identifier.", node.tok.line, "", node.name, node.tok.file).print();
			node.type = Integer;
        } else {
			node.type = node.declaration->type;
		}
    }
	if(node.type == Unknown or node.type == Number or node.type == Any) {
		CompileError(ErrorType::TypeError,"Could not infer array type. Variable is Unknown/Number/Any", node.tok.line, "valid type", node.name, node.tok.file).print();
	}
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);
    if(node.assignee)
        node.assignee->accept(*this);

}
