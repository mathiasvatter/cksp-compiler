//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTVisitor.h"

void NodeInt::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeBinaryExpr::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeVariableAssign::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeAssignStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeStatements::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeCallback::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeAST::accept(ASTVisitor &visitor) {

}
