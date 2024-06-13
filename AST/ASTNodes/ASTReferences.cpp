//
// Created by Mathias Vatter on 25.04.24.
//

#include "ASTReferences.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringList.h"
#include "../../Lowering/LoweringNDArray.h"
#include "../../Lowering/LoweringArray.h"

// ************* NodeVariableRef ***************
void NodeVariableRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeVariableRef::NodeVariableRef(const NodeVariableRef& other)
	: NodeReference(other) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeVariableRef::clone() const {
	return std::make_unique<NodeVariableRef>(*this);
}

// ************* NodeArrayRef ***************
void NodeArrayRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeArrayRef::NodeArrayRef(const NodeArrayRef& other)
	: NodeReference(other), index(clone_unique(other.index)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeArrayRef::clone() const {
	return std::make_unique<NodeArrayRef>(*this);
}
NodeAST * NodeArrayRef::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (index.get() == oldChild) {
		index = std::move(newChild);
		return index.get();
	}
	return nullptr;
}

ASTVisitor* NodeArrayRef::get_lowering(NodeProgram *program) const {
    static LoweringArray lowering(program);
    return &lowering;
}

// ************* NodeNDArrayRef ***************
void NodeNDArrayRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeNDArrayRef::NodeNDArrayRef(const NodeNDArrayRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)),
	sizes(clone_unique(other.sizes)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeNDArrayRef::clone() const {
	return std::make_unique<NodeNDArrayRef>(*this);
}

ASTVisitor* NodeNDArrayRef::get_lowering(NodeProgram *program) const {
    static LoweringNDArray lowering(program);
    return &lowering;
}

// ************* NodeListRef ***************
void NodeListRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeListRef::NodeListRef(const NodeListRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)), sizes(other.sizes),
	pos(other.pos) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeListRef::clone() const {
	return std::make_unique<NodeListRef>(*this);
}

ASTVisitor* NodeListRef::get_lowering(NodeProgram *program) const {
	static LoweringList lowering(program);
	return &lowering;
}


// ************* NodeStructRef ***************
void NodeStructRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeStructRef::NodeStructRef(const NodeStructRef& other)
	: NodeReference(other) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeStructRef::clone() const {
	return std::make_unique<NodeStructRef>(*this);
}