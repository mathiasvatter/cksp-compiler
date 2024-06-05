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

ASTVisitor* NodeArrayRef::get_lowering(DefinitionProvider* def_provider) const {
    static LoweringArray lowering(def_provider);
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

ASTVisitor* NodeNDArrayRef::get_lowering(DefinitionProvider* def_provider) const {
    static LoweringNDArray lowering(def_provider);
    return &lowering;
}

// ************* NodeListStructRef ***************
void NodeListStructRef::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

NodeListStructRef::NodeListStructRef(const NodeListStructRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)), sizes(other.sizes),
	pos(other.pos) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeListStructRef::clone() const {
	return std::make_unique<NodeListStructRef>(*this);
}

ASTVisitor* NodeListStructRef::get_lowering(DefinitionProvider* def_provider) const {
	static LoweringList lowering(def_provider);
	return &lowering;
}