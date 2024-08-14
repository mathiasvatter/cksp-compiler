//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"
#include "ASTInstructions.h"
#include "../TypeRegistry.h"

struct NodeVariable: NodeDataStructure {
	inline NodeVariable(std::optional<Token> is_persistent, std::string name, Type* ty, DataType type, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Variable) {
		persistence = std::move(is_persistent);
		data_type = type;
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeVariable(const NodeVariable& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::VariableRef;}
	std::unique_ptr<class NodeArray> to_array(NodeAST* size) override;
	std::unique_ptr<class NodePointer> to_pointer() override;
	std::unique_ptr<class NodeNDArray> to_ndarray() override ;
	std::unique_ptr<class NodeList> to_list() override ;
};

struct NodePointer: NodeDataStructure {
	inline NodePointer(std::optional<Token> is_persistent, std::string name, Type* ty, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Pointer) {
		persistence = std::move(is_persistent);
		data_type = DataType::Mutable;
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodePointer(const NodePointer& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::PointerRef;}

	ASTLowering* get_lowering(NodeProgram *program) const override;
	std::unique_ptr<NodeArray> to_array(NodeAST* size) override;
	std::unique_ptr<NodeVariable> to_variable() override;
};

struct NodeArray : NodeDataStructure {
	bool show_brackets = true;
	std::unique_ptr<NodeAST> size = nullptr;
	inline NodeArray(std::string name, Token tok) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::Array) {}
    inline NodeArray(std::optional<Token> is_persistent, std::string name, Type* ty, std::unique_ptr<NodeAST> size, Token tok)
            : NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Array),
              size(std::move(size)) {
        persistence = std::move(is_persistent);
        this->data_type = DataType::Mutable;
        set_child_parents();
    }
	NodeAST * accept(struct ASTVisitor &visitor) override;
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
	ASTLowering* get_lowering(NodeProgram *program) const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::ArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<class NodeNDArray> to_ndarray() override;
	std::unique_ptr<class NodeList> to_list() override;
};

struct NodeNDArray : NodeDataStructure {
	int dimensions = 1;
	bool show_brackets = true;
	std::unique_ptr<NodeParamList> sizes = nullptr;
	inline explicit NodeNDArray(std::string name, Token tok) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, tok, NodeType::NDArray) {}
	inline NodeNDArray(std::optional<Token> is_persistent, std::string name, Type *ty, std::unique_ptr<NodeParamList> sizes, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::NDArray),
		  sizes(std::move(sizes)) {
		persistence = std::move(is_persistent);
		this->data_type = DataType::Mutable;
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
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
	ASTLowering* get_lowering(NodeProgram *program) const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::NDArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<NodeArray> to_array(NodeAST* size) override;
	std::unique_ptr<NodeList> to_list() override;
};

struct NodeUIControl : NodeDataStructure {
	std::string ui_control_type;
	std::unique_ptr<NodeDataStructure> control_var; //Array or Variable
	std::unique_ptr<NodeParamList> params;
	std::unique_ptr<NodeParamList> sizes; // if it is ui_control array
	std::vector<Type*> arg_types;
    NodeUIControl* declaration = nullptr;
	inline explicit NodeUIControl(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl) {}
	inline NodeUIControl(std::string uiControlType, std::unique_ptr<NodeDataStructure> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
		: NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
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
	ASTLowering* get_lowering(NodeProgram *program) const override;
	Type* cast_type() override {
		control_var->cast_type();
		ty = control_var->ty;
		return ty;
	}
	bool is_ui_control_array() const;
};

struct NodeList : NodeDataStructure {
	int32_t size = 0;
	std::vector<std::unique_ptr<NodeParamList>> body;
	inline explicit NodeList(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::List) {}
	inline NodeList(std::string name, Type* ty, int32_t size, std::vector<std::unique_ptr<NodeParamList>> body, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::List), size(size), body(std::move(body)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeList(const NodeList& other);
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
	ASTLowering* get_lowering(NodeProgram *program) const override;
	NodeType get_ref_node_type() override {return NodeType::ListRef;}
	std::unique_ptr<NodeVariable> to_variable() override;
	std::unique_ptr<NodeArray> to_array(NodeAST* size) override;
	std::unique_ptr<NodeNDArray> to_ndarray() override;
};

struct NodeConst : NodeDataStructure {
    std::unique_ptr<NodeBlock> constants;
    inline explicit NodeConst(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::Const) {}
    inline NodeConst(std::string name, std::unique_ptr<NodeBlock> constants, Token tok)
            : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::Const), constants(std::move(constants)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeConst(const NodeConst& other);
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
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;

};

struct NodeStruct : NodeDataStructure {
	std::unique_ptr<NodeVariable> node_self = std::make_unique<NodeVariable>(std::nullopt, "self", TypeRegistry::add_object_type(this->name), DataType::Mutable, this->tok);
	std::unique_ptr<NodeBlock> members;
	std::map<std::string, NodeDataStructure*> member_table;
	std::set<std::string> member_set;
	NodeFunctionDefinition* constructor = nullptr;
	std::vector<std::unique_ptr<NodeFunctionDefinition>> methods;
	std::set<std::string> method_set;
	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> method_table;
	std::unordered_set<NodeType> member_node_types;
	NodeVariable* max_individual_struts_var = nullptr;
	NodeVariable* free_idx_var = nullptr;
	NodeArray* allocation_var = nullptr;
	static std::unordered_set<NodeType> allowed_member_node_types;
	inline explicit NodeStruct(const std::string& name, Token tok) : NodeDataStructure(name, TypeRegistry::add_object_type(name), std::move(tok), NodeType::Struct) {}
	inline NodeStruct(const std::string& name, std::unique_ptr<NodeBlock> members, std::vector<std::unique_ptr<NodeFunctionDefinition>> methods, Token tok)
		: NodeDataStructure(name, TypeRegistry::add_object_type(name), std::move(tok), NodeType::Struct), members(std::move(members)), methods(std::move(methods)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeStruct(const NodeStruct& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		members->update_parents(this);
		for (auto & m : methods) {
			m->update_parents(this);
		}
	}
	void set_child_parents() override {
		members->parent = this;
		for(auto& m : methods) {
			if(m) m->parent = this;
		}
	};
	std::string get_string() override { return ""; }
	void update_token_data(const Token& token) override {
		members->update_token_data(token);
		for(auto& m : methods) {
			if(m) m->update_token_data(token);
		}
	}
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	ASTLowering* get_lowering(NodeProgram *program) const override;

	void update_member_table() {
		member_table.clear();
		for(auto& member : members->statements) {
			if(member->statement->get_node_type() == NodeType::Declaration) {
				auto declaration = static_cast<NodeDeclaration*>(member->statement.get());
				for(auto & var : declaration->variable) {
					member_table[var->name] = var.get();
				}
			} else if(member->statement->get_node_type() == NodeType::SingleDeclaration) {
				auto declaration = static_cast<NodeSingleDeclaration*>(member->statement.get());
				member_table[declaration->variable->name] = declaration->variable.get();
			} else {
				auto error = CompileError(ErrorType::Variable, "<Struct> member must be a declaration", "", tok);
				error.exit();
			}
		}
	}
	void update_method_table() {
		method_table.clear();
		for(auto& method : methods) {
			method_table.insert({{method->header->name, (int)method->header->args->params.size()}, method.get()});
		}
	}

	void update_lookup_sets() {
		method_set.clear();
		for(auto& method : methods) {
			method_set.emplace(method->header->name);
		}
		member_set.clear();
		for(auto& member : member_table) {
			method_set.emplace(member.first);
		}
	}

	NodeDataStructure* get_member(const std::string& ref_name) {
		auto member = member_table.find(ref_name);
		if(member != member_table.end()) {
			return member->second;
		}
		return nullptr;
	}

	static std::unique_ptr<NodeBlock> declare_struct_constants();
	/// generated init method only needs assignment if it has pointer -> nil
	NodeFunctionDefinition* generate_init_method();

	/**
	 * generates a __repr__ method for a struct
	 * @param obj
	 * // standard str function if not overwritten
	 * function __repr__(self)
	 * 	 return "<struct> Object: "& self
	 * end function
	 */
	NodeFunctionDefinition* generate_repr_method();
	void inline_struct(NodeProgram* program);
};