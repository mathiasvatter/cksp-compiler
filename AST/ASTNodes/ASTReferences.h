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
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeVariableRef(const NodeVariableRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
};

struct NodeArrayRef : NodeReference {
	std::unique_ptr<NodeAST> index;
	inline NodeArrayRef(std::string name, std::unique_ptr<NodeAST> index, Token tok)
		: NodeReference(std::move(name), NodeType::ArrayRef, std::move(tok)), index(std::move(index)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
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
    ASTVisitor* get_lowering(NodeProgram *program) const override;
};

struct NodeNDArrayRef : NodeReference {
	std::unique_ptr<NodeParamList> indexes = nullptr;
    std::unique_ptr<NodeParamList> sizes = nullptr;
	inline NodeNDArrayRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::NDArrayRef, std::move(tok)), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
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
    ASTVisitor* get_lowering(NodeProgram *program) const override;

};

struct NodeListRef : NodeReference {
	NodeParamList* sizes = nullptr; // param list of size of the lists in the list
	NodeParamList* pos = nullptr; // param list of positions in the list
	std::unique_ptr<NodeParamList> indexes;
	NodeListRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::ListRef, std::move(tok)), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
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
	ASTVisitor* get_lowering(NodeProgram *program) const override;

};

struct NodePointerRef : NodeReference {
	std::vector<std::string> ptr_chain;
	inline NodePointerRef(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::PointerRef, std::move(tok)) {
		update_ptr_chain();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodePointerRef(const NodePointerRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
	void update_ptr_chain() {
		ptr_chain.clear();
		std::istringstream iss(name);
		std::string ns;
		while (std::getline(iss, ns, '.')) {
			ptr_chain.push_back(ns);
		}
	}
	std::string get_object_name() {
		if(ptr_chain.empty())
			return "";
		return ptr_chain.at(0);
	}
};

