//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTReferences.h"
#include "../ASTVisitor/ASTVisitor.h"

// ************* NodeVariableReference ***************
void NodeVariableReference::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeVariableReference::NodeVariableReference(const NodeVariableReference& other)
	: NodeReference(other) {}

std::unique_ptr<NodeAST> NodeVariableReference::clone() const {
	return std::make_unique<NodeVariableReference>(*this);
}

// ************* NodeArrayReference ***************
void NodeArrayReference::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeArrayReference::NodeArrayReference(const NodeArrayReference& other)
	: NodeReference(other), index(clone_unique(other.index)) {}

std::unique_ptr<NodeAST> NodeArrayReference::clone() const {
	return std::make_unique<NodeArrayReference>(*this);
}

// ************* NodeNDArrayReference ***************
void NodeNDArrayReference::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeNDArrayReference::NodeNDArrayReference(const NodeNDArrayReference& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)) {}

std::unique_ptr<NodeAST> NodeNDArrayReference::clone() const {
	return std::make_unique<NodeNDArrayReference>(*this);
}

// ************* NodeListStructReference ***************
void NodeListStructReference::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeListStructReference::NodeListStructReference(const NodeListStructReference& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)) {}

std::unique_ptr<NodeAST> NodeListStructReference::clone() const {
	return std::make_unique<NodeListStructReference>(*this);
}