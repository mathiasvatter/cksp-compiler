//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"

struct NodeVariableRef : NodeReference {
	inline NodeVariableRef(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::VariableRef, tok) {}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeVariableRef(const NodeVariableRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {}
	std::string get_string() override {
		return name;
	}
};

struct NodeArrayRef : NodeReference {
	std::unique_ptr<NodeAST> index;
	inline NodeArrayRef(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::ArrayRef, tok) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeArrayRef(const NodeArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		if(index) index ->update_parents(this);
	}
	void set_child_parents() override {
		if(index) index ->parent = this;
	};
	std::string get_string() override {
		return name;
	}
};

struct NodeNDArrayRef : NodeReference {
	std::unique_ptr<NodeParamList> indexes;
	inline NodeNDArrayRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::NDArrayRef, tok), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeNDArrayRef(const NodeNDArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		if(indexes) indexes ->update_parents(this);
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	};
	std::string get_string() override {
		return name;
	}
};

struct NodeListStructRef : NodeReference {
	NodeParamList* sizes = nullptr; // param list of size of the lists in the list
	NodeParamList* pos = nullptr; // param list of positions in the list
	std::unique_ptr<NodeParamList> indexes;
	NodeListStructRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::ListStructRef, tok), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeListStructRef(const NodeListStructRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		if(indexes) indexes->update_parents(this);
	}
	std::string get_string() override {
		return name;
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	};
	ASTVisitor* get_lowering() const override;

};

