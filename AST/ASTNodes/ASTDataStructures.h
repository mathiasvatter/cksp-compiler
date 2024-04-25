//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include "AST.h"

struct NodeVariable: NodeDataStructure {
	inline NodeVariable(std::optional<Token> is_persistent, std::string name, DataType type, Token tok)
		: NodeDataStructure(name, tok, NodeType::Variable) {
		persistence = std::move(is_persistent);
		data_type = type;
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeVariable(const NodeVariable& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
};

struct NodeArray : NodeDataStructure {
	int dimensions = 1;
	bool show_brackets = true;
	std::unique_ptr<NodeParamList> sizes = nullptr;
	std::unique_ptr<NodeParamList> indexes = nullptr;
	inline explicit NodeArray(std::string name, Token tok) : NodeDataStructure(name, tok, NodeType::Array) {}
	inline NodeArray(std::optional<Token> is_persistent, std::string name, DataType var_type, std::unique_ptr<NodeParamList> sizes,
					 std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeDataStructure(std::move(name), tok, NodeType::Array),
		  sizes(std::move(sizes)), indexes(std::move(indexes)) {
		persistence = std::move(is_persistent);
		this->data_type = var_type;
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeArray(const NodeArray& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if (sizes) sizes->update_parents(this);
		if (indexes) indexes->update_parents(this);
	}
	void set_child_parents() override {
		if (sizes) sizes->parent = this;
		if (indexes) indexes->parent = this;
	};
	void update_token_data(const Token& token) override {
		if(sizes) sizes -> update_token_data(token);
		if(indexes) indexes ->update_token_data(token);
	}
};

struct NodeNDArray : NodeDataStructure {
	int dimensions = 1;
	bool show_brackets = true;
	std::unique_ptr<NodeParamList> sizes = nullptr;
	std::unique_ptr<NodeParamList> indexes = nullptr;
	inline explicit NodeNDArray(std::string name, Token tok) : NodeDataStructure(name, tok, NodeType::NDArray) {}
	inline NodeNDArray(std::optional<Token> is_persistent, std::string name, DataType var_type, std::unique_ptr<NodeParamList> sizes,
					   std::unique_ptr<NodeParamList> indexes, Token tok)
		: NodeDataStructure(std::move(name), tok, NodeType::NDArray),
		  sizes(std::move(sizes)), indexes(std::move(indexes)) {
		persistence = std::move(is_persistent);
		this->data_type = var_type;
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeNDArray(const NodeNDArray& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if (sizes) sizes->update_parents(this);
		if (indexes) indexes->update_parents(this);
	}
	void set_child_parents() override {
		if (sizes) sizes->parent = this;
		if (indexes) indexes->parent = this;
	};
	std::string get_string() override {
		return name;
	}
	void update_token_data(const Token& token) override {
		if(sizes) sizes -> update_token_data(token);
		if(indexes) indexes ->update_token_data(token);
	}
	ASTVisitor* get_lowering() const override;
};

struct NodeUIControl : NodeDataStructure {
	std::string ui_control_type;
	std::unique_ptr<NodeDataStructure> control_var; //Array or Variable
	std::unique_ptr<NodeParamList> params;
	std::unique_ptr<NodeParamList> sizes; // if it is ui_control array
	std::vector<ASTType> arg_ast_types;
	std::vector<DataType> arg_var_types;
	inline explicit NodeUIControl(Token tok) : NodeDataStructure("", tok, NodeType::UIControl) {}
	inline NodeUIControl(std::string uiControlType, std::unique_ptr<NodeDataStructure> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
		: NodeDataStructure("", tok, NodeType::UIControl), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeUIControl(const NodeUIControl& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		control_var->update_parents(this);
		if (params) params->update_parents(this);
	}
	void set_child_parents() override {
		control_var->parent = this;
		if (params) params->parent = this;
	};
	std::string get_string() override {
		return control_var->get_string();
	}
	void update_token_data(const Token& token) override {
		control_var -> update_token_data(token);
		params -> update_token_data(token);
	}
	ASTVisitor* get_lowering() const override;

};

struct NodeListStruct : NodeDataStructure {
	int32_t size = 0;
	std::vector<std::unique_ptr<NodeParamList>> body;
	inline explicit NodeListStruct(Token tok) : NodeDataStructure("", tok, NodeType::ListStruct) {}
	inline NodeListStruct(std::string name, int32_t size, std::vector<std::unique_ptr<NodeParamList>> body, Token tok)
		: NodeDataStructure(std::move(name), tok, NodeType::ListStruct), size(size), body(std::move(body)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeListStruct(const NodeListStruct& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		for (auto & b : body) {
			b->update_parents(this);
		}
	}
	void set_child_parents() override {
		for (auto & b : body) {
			if(b) b->parent = this;
		}
	};
	std::string get_string() override { return ""; }
	void update_token_data(const Token& token) override {
		for(auto &b : body) {
			b->update_token_data(token);
		}
	}
	ASTVisitor* get_lowering() const override;

};
