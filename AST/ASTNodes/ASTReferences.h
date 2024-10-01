//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"

struct NodeVariableRef : NodeReference {
	inline NodeVariableRef(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::VariableRef, std::move(tok)) {
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeVariableRef(const NodeVariableRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
	std::unique_ptr<struct NodeArrayRef> to_array_ref(NodeAST* index) override;
	/// this_list.next.next
	std::unique_ptr<struct NodeAccessChain> to_method_chain() override;
	std::unique_ptr<struct NodePointerRef> to_pointer_ref() override;

	/// checks if variable ref is a ndarray size constant by checking the declaration and
	/// pattern matching the name (....SIZE_D(\d+))
	bool is_ndarray_constant();
	/// checks if variable list or array size constant
	bool is_array_constant();
};

struct NodeArrayRef : NodeReference {
	std::unique_ptr<NodeAST> index;
	inline NodeArrayRef(std::string name, std::unique_ptr<NodeAST> index, Token tok)
		: NodeReference(std::move(name), NodeType::ArrayRef, std::move(tok)), index(std::move(index)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeArrayRef(const NodeArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(index) index ->update_parents(this);
	}
	void set_child_parents() override {
		if(index) index ->parent = this;
	};
	std::string get_string() override {
		return name;
	}
//    ASTLowering* get_lowering(NodeProgram *program) const override;
	std::unique_ptr<struct NodeNDArrayRef> to_ndarray_ref() override;
	/// this_list.next.value[6]
	std::unique_ptr<struct NodeAccessChain> to_method_chain() override;
	std::unique_ptr<NodeFunctionCall> get_size();
	/// check if array ref is <list_ref>.size[] array
	bool is_list_sizes();
};

struct NodeNDArrayRef : NodeReference {
	std::unique_ptr<NodeParamList> indexes = nullptr;
    std::unique_ptr<NodeParamList> sizes = nullptr;
	inline NodeNDArrayRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::NDArrayRef, std::move(tok)), indexes(std::move(indexes)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeNDArrayRef(const NodeNDArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(indexes) indexes ->update_parents(this);
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	};
	std::string get_string() override {
		return name;
	}
    ASTLowering* get_lowering(NodeProgram *program) const override;
	std::unique_ptr<NodeArrayRef> to_array_ref(NodeAST* index) override;
	/// this_list.next.value[6,4]
	std::unique_ptr<struct NodeAccessChain> to_method_chain() override;

	[[nodiscard]] inline int num_wildcards() const {
		int count = 0;
		if(indexes) {
			for(auto & idx: indexes->params) if(idx->get_node_type() == NodeType::Wildcard) count++;
		}
		return count;
	}
	/// clones sizes list from declaration if it is a NDArray
	bool determine_sizes();
	inline CompileError throw_missing_indexes_error() {
		auto compile_error = CompileError(ErrorType::SyntaxError, "","", tok);
		compile_error.m_message = "NDArray reference requires indexes in this context: " + tok.val + ".";
		compile_error.m_expected = "Valid indexes";
		return compile_error;
	}
	inline CompileError throw_missing_sizes_error() {
		auto compile_error = CompileError(ErrorType::InternalError, "","", tok);
		compile_error.m_message = "NDArray reference has unknown sizes: " + tok.val + ".";
		compile_error.m_expected = "Valid sizes";
		return compile_error;
	}
	/// adds wildcard indexes to ndarray reference if there are no indexes present
	/// depends on the size -> size has to be known beforehand
	bool add_wildcards() {
		if(this->indexes) return false;
		if(!this->sizes and !this->ty) return false;
		this->indexes = std::make_unique<NodeParamList>(this->tok);
		this->indexes->parent = this->indexes.get();
		// get dimensions either from type or from sizes
		auto num_dimensions = ty ? ty->get_dimensions() : sizes->params.size();
		for(int i=0; i<num_dimensions; i++) {
			auto wildcard = std::make_unique<NodeWildcard>("*", Token());
			this->indexes->add_param(std::move(wildcard));
		}
		return true;
	}
};

struct NodeFunctionVarRef : NodeReference {
	NodeFunctionDefinition* definition = nullptr;
	std::unique_ptr<class NodeFunctionHeader> header;
	NodeFunctionVarRef(std::string name, std::unique_ptr<NodeFunctionHeader> header, Token tok);
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeFunctionVarRef(const NodeFunctionVarRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override;
	std::string get_string() override {
		return name;
	}
	void set_child_parents() override;;
};

struct NodeListRef : NodeReference {
	NodeParamList* sizes = nullptr; // param list of size of the lists in the list
	NodeParamList* pos = nullptr; // param list of positions in the list
	std::unique_ptr<NodeParamList> indexes;
	NodeListRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::ListRef, std::move(tok)), indexes(std::move(indexes)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeListRef(const NodeListRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(indexes) indexes->update_parents(this);
	}
	std::string get_string() override {
		return name;
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	};
	ASTLowering* get_lowering(NodeProgram *program) const override;

};

struct NodePointerRef : NodeReference {
	std::vector<std::string> ptr_chain;
	inline NodePointerRef(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::PointerRef, std::move(tok)) {
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodePointerRef(const NodePointerRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;

	std::unique_ptr<NodeArrayRef> to_array_ref(NodeAST* index) override;
	std::unique_ptr<NodeVariableRef> to_variable_ref() override;

	ASTLowering* get_lowering(NodeProgram *program) const override;

	bool is_string_repr();
	std::unique_ptr<NodeFunctionCall> get_repr_call();
};

struct NodeNil : NodePointerRef {
	inline explicit NodeNil(Token tok) : NodePointerRef(std::move("nil"), std::move(tok)) {node_type = NodeType::Nil;}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeNil(const NodeNil& other) : NodePointerRef(other) {}
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	[[nodiscard]] ASTLowering *get_lowering(NodeProgram *program) const override;
};

struct NodeAccessChain : NodeReference {
	std::vector<std::unique_ptr<NodeAST>> chain;
	std::vector<Type*> types;
	inline NodeAccessChain(std::vector<std::unique_ptr<NodeAST>> method_chain, Token tok)
		: NodeReference("", NodeType::AccessChain, std::move(tok)), chain(std::move(method_chain)) {
		set_child_parents();
	}
	inline explicit NodeAccessChain(Token tok)
		: NodeReference("", NodeType::AccessChain, std::move(tok)) {}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeAccessChain(const NodeAccessChain& other);
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(auto & c: chain) c->update_parents(this);
	}
	void set_child_parents() override {
		for(auto & c: chain) c->parent = this;
	};
	void add_method(std::unique_ptr<NodeAST> m) {
		m->parent = this;
		chain.push_back(std::move(m));
	}
	void update_types() {
		for(const auto& c: chain) {
			types.push_back(c->ty);
		}
	}
	std::string get_string() override {
		std::string str = "";
		for(auto & c: chain) {
			str += c->get_string() + ".";
		}
		return str.erase(str.size() - 1);
	}
	void flatten() {
		std::vector<std::unique_ptr<NodeAST>> flat_list;
		// Rekursive Funktion, um die Parameterliste abzuflachen
		std::function<void(std::vector<std::unique_ptr<NodeAST>>)> flatten = [&](std::vector<std::unique_ptr<NodeAST>> current_node) {
		  for (auto& ptr : current_node) {
			  if (ptr->get_node_type() == NodeType::AccessChain) {
				  flatten(std::move(static_cast<NodeAccessChain*>(ptr.get())->chain));
			  } else {
				  // Wenn es kein NodeParamList ist, fügen wir es direkt zur Liste hinzu
				  ptr->parent = this;
				  flat_list.push_back(std::move(ptr));
			  }
		  }
		};
		flatten(std::move(chain));
		chain = std::move(flat_list);
	}

	/// returns variable ref if access chain is incorrectly detected array/list/ndarray size constant
	std::unique_ptr<NodeVariableRef> is_size_constant();

	ASTLowering* get_lowering(NodeProgram *program) const override;

};

