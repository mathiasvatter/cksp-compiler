//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"
#include "../TypeRegistry.h"

struct NodeVariable: NodeDataStructure {
	inline NodeVariable(std::optional<Token> is_persistent, std::string name, Type* ty, DataType type, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Variable) {
		persistence = std::move(is_persistent);
		data_type = type;
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeVariable(const NodeVariable& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::VariableRef;}
	std::unique_ptr<class NodeArray> to_array() override;
	std::unique_ptr<class NodeNDArray> to_ndarray() override ;
	std::unique_ptr<class NodeListStruct> to_list() override ;
};

struct NodeArray : NodeDataStructure {
	bool show_brackets = true;
	std::unique_ptr<NodeAST> size = nullptr;
	inline NodeArray(std::string name, Token tok) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::Array) {}
	inline NodeArray(std::optional<Token> is_persistent, std::string name, Type* ty, DataType var_type, std::unique_ptr<NodeAST> size, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Array),
		  size(std::move(size)) {
		persistence = std::move(is_persistent);
		this->data_type = var_type;
		set_child_parents();
	}
    inline NodeArray(std::optional<Token> is_persistent, std::string name, Type* ty, std::unique_ptr<NodeAST> size, Token tok)
            : NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Array),
              size(std::move(size)) {
        persistence = std::move(is_persistent);
        this->data_type = DataType::Array;
        set_child_parents();
    }
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeArray(const NodeArray& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if (size) size->update_parents(this);
	}
	void set_child_parents() override {
		if (size) size->parent = this;
	};
	void update_token_data(const Token& token) override {
		if(size) size->update_token_data(token);
	}
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::ArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<class NodeNDArray> to_ndarray() override;
	std::unique_ptr<class NodeListStruct> to_list() override;
};

struct NodeNDArray : NodeDataStructure {
	int dimensions = 1;
	bool show_brackets = true;
	std::unique_ptr<NodeParamList> sizes = nullptr;
	inline explicit NodeNDArray(std::string name, Token tok) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, tok, NodeType::NDArray) {}
	inline NodeNDArray(std::optional<Token> is_persistent, std::string name, Type* ty, DataType var_type, std::unique_ptr<NodeParamList> sizes, Token tok)
		: NodeDataStructure(std::move(name), ty, tok, NodeType::NDArray),
		  sizes(std::move(sizes)) {
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
	}
	void set_child_parents() override {
		if (sizes) sizes->parent = this;
	};
	std::string get_string() override {
		return name;
	}
	void update_token_data(const Token& token) override {
		if(sizes) sizes->update_token_data(token);
	}
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::NDArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<NodeArray> to_array() override;
	std::unique_ptr<NodeListStruct> to_list() override;
};

struct NodeUIControl : NodeDataStructure {
	std::string ui_control_type;
	std::unique_ptr<NodeDataStructure> control_var; //Array or Variable
	std::unique_ptr<NodeParamList> params;
	std::unique_ptr<NodeParamList> sizes; // if it is ui_control array
	std::vector<ASTType> arg_ast_types;
	std::vector<DataType> arg_var_types;
	std::vector<Type*> arg_types;
    NodeUIControl* declaration = nullptr;
	inline explicit NodeUIControl(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl) {}
	inline NodeUIControl(std::string uiControlType, std::unique_ptr<NodeDataStructure> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
		: NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
	Type* cast_type() override {
		control_var->cast_type();
		ty = control_var->ty;
		return ty;
	}
};

struct NodeListStruct : NodeDataStructure {
	int32_t size = 0;
	std::vector<std::unique_ptr<NodeParamList>> body;
	inline explicit NodeListStruct(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::ListStruct) {}
	inline NodeListStruct(std::string name, Type* ty, int32_t size, std::vector<std::unique_ptr<NodeParamList>> body, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::ListStruct), size(size), body(std::move(body)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeListStruct(const NodeListStruct& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
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
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
	NodeType get_ref_node_type() override {return NodeType::ListStructRef;}
	std::unique_ptr<NodeVariable> to_variable() override;
	std::unique_ptr<NodeArray> to_array() override;
	std::unique_ptr<NodeNDArray> to_ndarray() override;
};

struct NodeConstStatement : NodeDataStructure {
//    std::string prefix;
    std::unique_ptr<NodeBody> constants;
    inline explicit NodeConstStatement(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::ConstStatement) {}
    inline NodeConstStatement(std::string name, std::unique_ptr<NodeBody> constants, Token tok)
            : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::ConstStatement), constants(std::move(constants)) {
        set_child_parents();
    }
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeConstStatement(const NodeConstStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        constants ->update_parents(this);
    }
    void set_child_parents() override {
        constants->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        constants->update_token_data(token);
    }
    ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;

};