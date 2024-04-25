//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"

struct NodeVariableReference : NodeReference {
	inline NodeVariableReference(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::VariableReference, tok) {}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeVariableReference(const NodeVariableReference& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {}
	std::string get_string() override {
		return name;
	}
};

struct NodeArrayReference : NodeReference {
	std::unique_ptr<NodeAST> index;
	inline NodeArrayReference(std::string name, Token tok)
		: NodeReference(std::move(name), NodeType::ArrayReference, tok) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeArrayReference(const NodeArrayReference& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
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

struct NodeNDArrayReference : NodeReference {
	std::unique_ptr<NodeParamList> indexes;
	inline NodeNDArrayReference(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::NDArrayReference, tok), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeNDArrayReference(const NodeNDArrayReference& other);
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

struct NodeListStructReference : NodeReference {
	NodeParamList* sizes = nullptr; // param list of sizes of the lists in the list
	NodeParamList* pos = nullptr; // param list of positions in the list
	std::unique_ptr<NodeParamList> indexes;
	NodeListStructReference(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeReference(std::move(name), NodeType::ListStructReference, tok), indexes(std::move(indexes)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeListStructReference(const NodeListStructReference& other);
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

