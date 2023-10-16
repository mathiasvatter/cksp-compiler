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

void NodeParamList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeDeclareStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

//void NodeDefineStatement::accept(ASTVisitor &visitor) {
//	visitor.visit(*this);
//}

void NodeIfStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeWhileStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeFunctionCall::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeSelectStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeConstStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeStructStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeFamilyStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeUIControl::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeMacroDefinition::accept(ASTVisitor &visitor) {
//	visitor.visit(*this);
//}
//
//void NodeMacroHeader::accept(ASTVisitor &visitor) {
//    visitor.visit(*this);
//}

void NodeGetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeSetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
