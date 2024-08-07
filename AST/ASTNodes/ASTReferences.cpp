//
// Created by Mathias Vatter on 25.04.24.
//

#include <regex>

#include "ASTReferences.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringList.h"
#include "../../Lowering/LoweringNDArray.h"
#include "../../Lowering/LoweringArray.h"
#include "../../Lowering/LoweringPointer.h"
#include "../../Lowering/LoweringAccessChain.h"
#include "../../Lowering/LoweringNil.h"

// ************* NodeVariableRef ***************
NodeAST *NodeVariableRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeVariableRef::NodeVariableRef(const NodeVariableRef& other)
	: NodeReference(other) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeVariableRef::clone() const {
	return std::make_unique<NodeVariableRef>(*this);
}

std::unique_ptr<NodeArrayRef> NodeVariableRef::to_array_ref(NodeAST* index) {
	return std::make_unique<NodeArrayRef>(name, index ? index->clone() : nullptr, tok);
}

std::unique_ptr<NodePointerRef> NodeVariableRef::to_pointer_ref() {
	return std::make_unique<NodePointerRef>(name, tok);
}


std::unique_ptr<NodeAccessChain> NodeVariableRef::to_method_chain() {
	// split into variable references
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->parent = this->parent;
	return method_chain;
}

bool NodeVariableRef::is_ndarray_constant() {
	if(declaration and declaration->get_node_type() == NodeType::NDArray) {
		auto ndarray= static_cast<NodeNDArray*>(declaration);
		std::string string_pattern = "^" + ndarray->name + R"(.SIZE_D(\d+)$)";
		std::regex pattern(string_pattern);
		std::smatch match;
		// Überprüfen, ob der String dem Muster entspricht
		if (std::regex_match(name, match, pattern)) {
			// Extrahiere die Zahl aus dem Match
			int number = std::stoi(match[1].str());

			// Überprüfe, ob die Zahl innerhalb der Grenzen liegt
			return number >= 1 && number <= ndarray->dimensions;
		}
		return false;
	}
	return false;
}

// ************* NodeArrayRef ***************
NodeAST *NodeArrayRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeArrayRef::NodeArrayRef(const NodeArrayRef& other)
	: NodeReference(other), index(clone_unique(other.index)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeArrayRef::clone() const {
	return std::make_unique<NodeArrayRef>(*this);
}
NodeAST *NodeArrayRef::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (index.get() == oldChild) {
		index = std::move(newChild);
		return index.get();
	}
	return nullptr;
}

ASTLowering* NodeArrayRef::get_lowering(NodeProgram *program) const {
    static LoweringArray lowering(program);
    return &lowering;
}

std::unique_ptr<NodeNDArrayRef> NodeArrayRef::to_ndarray_ref() {
	return std::make_unique<NodeNDArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, index->clone()) : nullptr, tok);
}

std::unique_ptr<NodeAccessChain> NodeArrayRef::to_method_chain() {
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	auto array_ref = clone_as<NodeArrayRef>(this);
	array_ref->name = ptr_strings.back();
	ptr_strings.pop_back();
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->add_method(std::move(array_ref));
	method_chain->parent = this->parent;
	return method_chain;
}

std::unique_ptr<NodeFunctionCall> NodeArrayRef::get_size() {
	return DefinitionProvider::num_elements(clone_as<NodeArrayRef>(this));
}

// ************* NodeNDArrayRef ***************
NodeAST *NodeNDArrayRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeNDArrayRef::NodeNDArrayRef(const NodeNDArrayRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)),
	sizes(clone_unique(other.sizes)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeNDArrayRef::clone() const {
	return std::make_unique<NodeNDArrayRef>(*this);
}

ASTLowering* NodeNDArrayRef::get_lowering(NodeProgram *program) const {
    static LoweringNDArray lowering(program);
    return &lowering;
}

std::unique_ptr<NodeArrayRef> NodeNDArrayRef::to_array_ref(NodeAST* index) {
	return std::make_unique<NodeArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, index->clone()) : nullptr, tok);
}

bool NodeNDArrayRef::determine_sizes() {
	if(!declaration) return false;
	if(declaration->get_node_type() != NodeType::NDArray) return false;
	auto node_ndarray = static_cast<NodeNDArray*>(declaration);
	// has no size if function definition parameter
	if(!node_ndarray->sizes) return false;
	sizes = clone_as<NodeParamList>(node_ndarray->sizes.get());
	sizes->parent = this;
	return true;
}

std::unique_ptr<NodeAccessChain> NodeNDArrayRef::to_method_chain() {
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	auto array_ref = clone_as<NodeNDArrayRef>(this);
	array_ref->name = ptr_strings.back();
	ptr_strings.pop_back();
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->add_method(std::move(array_ref));
	method_chain->parent = this->parent;
	return method_chain;
}

// ************* NodeListRef ***************
NodeAST *NodeListRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeListRef::NodeListRef(const NodeListRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)), sizes(other.sizes),
	pos(other.pos) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeListRef::clone() const {
	return std::make_unique<NodeListRef>(*this);
}

ASTLowering* NodeListRef::get_lowering(NodeProgram *program) const {
	static LoweringList lowering(program);
	return &lowering;
}


// ************* NodePointerRef ***************
NodeAST *NodePointerRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodePointerRef::NodePointerRef(const NodePointerRef& other)
	: NodeReference(other) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodePointerRef::clone() const {
	return std::make_unique<NodePointerRef>(*this);
}

std::unique_ptr<NodeArrayRef> NodePointerRef::to_array_ref(NodeAST* index) {
	return std::make_unique<NodeArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, index->clone()) : nullptr, tok);
}

std::unique_ptr<NodeVariableRef> NodePointerRef::to_variable_ref() {
	return std::make_unique<NodeVariableRef>(name, tok);
}

ASTLowering* NodePointerRef::get_lowering(NodeProgram *program) const {
	static LoweringPointer lowering(program);
	return &lowering;
}

bool NodePointerRef::is_string_repr() {
	bool is_string = false;
	// is within string environment
	is_string |= parent->ty == TypeRegistry::String;
	// is within message call
	is_string |= is_func_arg() and static_cast<NodeFunctionHeader*>(parent->parent)->name == "message";
	// is within return statement
	is_string |= parent->get_node_type() == NodeType::Return and static_cast<NodeReturn*>(parent)->definition->ty == TypeRegistry::String;
	return is_string;
}

std::unique_ptr<NodeFunctionCall> NodePointerRef::get_repr_call() {
	return std::make_unique<NodeFunctionCall>(
		false,
		std::make_unique<NodeFunctionHeader>(
			ty->to_string() + ".__repr__",
			std::make_unique<NodeParamList>(tok, this->to_variable_ref()),
			tok
		),
		tok
	);
}

// ************* NodeNil ***************
NodeAST *NodeNil::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeNil::clone() const {
	return std::make_unique<NodeNil>(*this);
}

ASTLowering* NodeNil::get_lowering(NodeProgram *program) const {
	static LoweringNil lowering(program);
	return &lowering;
}

// ************* NodeAccessChain ***************
NodeAST *NodeAccessChain::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeAccessChain::NodeAccessChain(const NodeAccessChain& other)
	: NodeReference(other), chain(clone_vector(other.chain)), types(other.types) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeAccessChain::clone() const {
	return std::make_unique<NodeAccessChain>(*this);
}

NodeAST *NodeAccessChain::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& c : chain) {
		if (c.get() == oldChild) {
			c = std::move(newChild);
			return c.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeAccessChain::get_lowering(NodeProgram *program) const {
	static LoweringAccessChain lowering(program);
	return &lowering;
}
