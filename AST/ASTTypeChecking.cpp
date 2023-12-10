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
    if(node.type == Unknown) {
        CompileError(ErrorType::TypeError,"Could not infer variable type.", node.tok.line, "", node.name, node.tok.file).print();
    }
}

void ASTTypeChecking::visit(NodeArray& node) {
}

void ASTTypeChecking::visit(NodeSingleDeclareStatement &node) {
    node.to_be_declared->accept(*this);


}
