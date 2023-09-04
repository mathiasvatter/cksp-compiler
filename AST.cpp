//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTVisitor.h"

void NodeAST::accept(ASTVisitor &visitor) {
}

void NodeInt::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeReal::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeString::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeArray::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeBinaryExpr::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeAssignStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeCallback::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeProgram::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void NodeFunctionHeader::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeFunctionDefinition::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeUnaryExpr::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

