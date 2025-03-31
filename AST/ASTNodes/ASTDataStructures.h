//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <utility>

#include "AST.h"
#include "ASTInstructions.h"
#include "../TypeRegistry.h"

struct NodeVariable final : NodeDataStructure {
	NodeVariable(std::optional<Token> is_persistent, std::string name, Type* ty, const DataType type, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Variable) {
		persistence = std::move(is_persistent);
		data_type = type;
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeVariable(const NodeVariable& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::VariableRef;}
	std::unique_ptr<NodeArray> to_array(std::unique_ptr<NodeAST> size) override;
	std::unique_ptr<NodePointer> to_pointer() override;
	std::unique_ptr<NodeNDArray> to_ndarray() override ;
	std::unique_ptr<NodeList> to_list() override ;
	std::unique_ptr<NodeDataStructure> inflate_dimension(std::unique_ptr<NodeAST> new_index) override;

};

struct NodePointer final : NodeDataStructure {
	NodePointer(std::optional<Token> is_persistent, std::string name, Type* ty, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::Pointer) {
		persistence = std::move(is_persistent);
		data_type = DataType::Mutable;
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodePointer(const NodePointer& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::PointerRef;}

	ASTLowering* get_lowering(NodeProgram *program) const override;
	std::unique_ptr<NodeArray> to_array(std::unique_ptr<NodeAST> size) override;
	std::unique_ptr<NodeVariable> to_variable() override;
	std::unique_ptr<NodeDataStructure> inflate_dimension(std::unique_ptr<NodeAST> new_index) override;
};

struct NodeComposite : NodeDataStructure {
	bool show_brackets = true;
	std::unique_ptr<NodeParamList> num_elements = nullptr;
	// Konstruktor
	NodeComposite(std::string name, Type* ty, Token tok, const NodeType node_type)
		: NodeDataStructure(std::move(name), ty, std::move(tok), node_type) {}
	// Kopierkonstruktor
	NodeComposite(const NodeComposite& other) : NodeDataStructure(other),
		show_brackets(other.show_brackets), num_elements(clone_unique(other.num_elements)) {}
	// Standardmethoden für gemeinsame Funktionalitäten
	void update_parents(NodeAST* new_parent) override {
		if(num_elements) num_elements->update_parents(new_parent);
		parent = new_parent;
	}
	void set_child_parents() override {
		if(num_elements) num_elements->parent = this;
	}
	void update_token_data(const Token& token) override {
		if(num_elements) num_elements->update_token_data(token);
	}
	void set_num_elements(std::unique_ptr<NodeParamList> num_elem) {
		num_elem->parent = this;
		this->num_elements = std::move(num_elem);
	}
	virtual std::shared_ptr<NodeArray> get_raw() = 0;
	virtual std::unique_ptr<NodeAST> get_size() = 0;
};


struct NodeArray final : NodeComposite {
	std::unique_ptr<NodeAST> size = nullptr;
	NodeArray(std::string name, Token tok) : NodeComposite(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::Array) {}
    NodeArray(std::optional<Token> is_persistent, std::string name, Type* ty, std::unique_ptr<NodeAST> size, Token tok)
            : NodeComposite(std::move(name), ty, std::move(tok), NodeType::Array),
              size(std::move(size)) {
        persistence = std::move(is_persistent);
        this->data_type = DataType::Mutable;
        set_child_parents();
    }
	NodeAST * accept(ASTVisitor &visitor) override;
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
	}
	void update_token_data(const Token& token) override {
		if(size) size->update_token_data(token);
	}
	void set_size(std::unique_ptr<NodeAST> size) {
		size->parent = this;
		this->size = std::move(size);
	}
//	ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering *get_data_lowering(NodeProgram *program) const override;

    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::ArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<NodeNDArray> to_ndarray() override;
	std::unique_ptr<NodeList> to_list() override;
	std::unique_ptr<NodeDataStructure> inflate_dimension(std::unique_ptr<NodeAST> new_index) override;
	std::shared_ptr<NodeArray> get_raw() override {
		return shared_ptr_cast<NodeArray>(get_shared());
	}
	std::unique_ptr<NodeAST> get_size() override {
		return size->clone();
	}
};

struct NodeNDArray final : NodeComposite {
	int inflation_times = 0;
	int dimensions = 1;
	std::unique_ptr<NodeParamList> sizes = nullptr;
	explicit NodeNDArray(std::string name, Token tok) : NodeComposite(std::move(name), TypeRegistry::Unknown, tok, NodeType::NDArray) {}
	NodeNDArray(std::optional<Token> is_persistent, std::string name, Type *ty, std::unique_ptr<NodeParamList> sizes, Token tok)
		: NodeComposite(std::move(name), ty, std::move(tok), NodeType::NDArray),
		  sizes(std::move(sizes)) {
		persistence = std::move(is_persistent);
		this->data_type = DataType::Mutable;
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
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
	}
	std::string get_string() override {
		return name;
	}
	void update_token_data(const Token& token) override {
		if(sizes) sizes->update_token_data(token);
	}
//	ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering *get_data_lowering(NodeProgram *program) const override;

    std::unique_ptr<NodeReference> to_reference() override;
	NodeType get_ref_node_type() override {return NodeType::NDArrayRef;}
	std::unique_ptr<NodeVariable> to_variable() override {
		return std::make_unique<NodeVariable>(persistence, name, ty, DataType::Mutable, tok);
	}
	std::unique_ptr<NodeArray> to_array(std::unique_ptr<NodeAST> size) override;
	std::unique_ptr<NodeList> to_list() override;
	std::unique_ptr<NodeDataStructure> inflate_dimension(std::unique_ptr<NodeAST> new_index) override;
	std::shared_ptr<NodeArray> get_raw() override;
	std::unique_ptr<NodeAST> get_size() override;
};

struct NodeFunctionHeader final : NodeDataStructure {
	bool has_forced_parenth = false;
	std::vector<std::unique_ptr<NodeFunctionParam>> params;
	explicit NodeFunctionHeader(std::string name, Token tok) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::FunctionHeader) {}
	NodeFunctionHeader(std::string name, std::vector<std::unique_ptr<NodeFunctionParam>> params, Token tok)
		: NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::FunctionHeader), params(std::move(params)) {
		set_child_parents();
	}
	NodeFunctionHeader(std::string name, std::unique_ptr<NodeFunctionParam> param, Token tok)
		: NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::FunctionHeader) {
		params.push_back(std::move(param));
		set_child_parents();
	}
	// Variadischer Template-Konstruktor
	template<typename... Params>
	explicit NodeFunctionHeader(std::string name, Token tok, Params&&... params) : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::FunctionHeader) {
		(add_param(std::move(params)), ...);
	}
	~NodeFunctionHeader() override = default;
	NodeAST* accept(ASTVisitor &visitor) override;
	NodeFunctionHeader(const NodeFunctionHeader& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(const auto &param : params) param->update_parents(this);
	}
	void set_child_parents() override {
		for(const auto &param : params) param->parent = this;
	}
	std::string get_string() override {
		std::string output = name + "(";
		for(const auto &param : params) output += param->get_string() + ", ";
		output.erase(output.size() - 2);
		return output + ")";
	}
	void update_token_data(const Token& token) override {
		tok.line = token.line; tok.file = token.file;
		for(const auto &param : params) param->update_token_data(token);
	}
	NodeType get_ref_node_type() override {return NodeType::FunctionHeaderRef;}
	Type* create_function_type(Type* return_type = TypeRegistry::Unknown) {
		auto ret_type = return_type;
		if (const auto type = ty->cast<FunctionType>()) {
			if (return_type == TypeRegistry::Unknown) ret_type = type->m_return_type;
		}
		std::vector<Type*> func_arg_types;
		for(const auto &param : params) func_arg_types.push_back(param->variable->ty);
		ty = TypeRegistry::add_function_type(func_arg_types, ret_type);
		return ty;
	}
	void add_param(std::shared_ptr<NodeDataStructure> param) {
		const auto tok = param->tok;
		auto decl = std::make_unique<NodeFunctionParam>(std::move(param), nullptr, tok);
		decl->parent = this;
		params.push_back(std::move(decl));
	}
	void add_param(std::unique_ptr<NodeFunctionParam> param) {
		param->parent = this;
		params.push_back(std::move(param));
	}
	void prepend_param(std::unique_ptr<NodeDataStructure> param) {
		auto decl = std::make_unique<NodeFunctionParam>(std::move(param));
		decl->parent = this;
		params.insert(params.begin(), std::move(decl));
	}
	std::shared_ptr<NodeDataStructure>& get_param(const int idx) const {
		return params[idx]->variable;
	}
	bool has_union_params() const {
		for (const auto& param : params) {
			if (param->variable->ty->get_element_type()->is_union_type()) {
				return true;
			}
		}
		return false;
	}
};

struct NodeUIControl final : NodeDataStructure {
	std::string ui_control_type;
	std::shared_ptr<NodeDataStructure> control_var; //Array or Variable
	std::unique_ptr<NodeParamList> params;
	std::unique_ptr<NodeParamList> sizes; // if it is ui_control array
    std::weak_ptr<NodeUIControl> declaration;
	explicit NodeUIControl(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl) {}
	NodeUIControl(std::string uiControlType, std::shared_ptr<NodeDataStructure> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
		: NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::UIControl), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
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
	}
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
	std::shared_ptr<NodeUIControl> get_declaration() const {
		return declaration.lock();
	}
};

struct NodeList final : NodeDataStructure {
	int32_t size = 0;
	std::vector<std::unique_ptr<NodeInitializerList>> body;
	explicit NodeList(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::List) {}
	NodeList(std::string name, Type* ty, const int32_t size, std::vector<std::unique_ptr<NodeInitializerList>> body, Token tok)
		: NodeDataStructure(std::move(name), ty, std::move(tok), NodeType::List), size(size), body(std::move(body)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeList(const NodeList& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for (const auto & b : body) {
			b->update_parents(this);
		}
	}
	void set_child_parents() override {
		for (auto & b : body) {
			if(b) b->parent = this;
		}
	}
	std::string get_string() override { return ""; }
	void update_token_data(const Token& token) override {
		for(const auto &b : body) {
			b->update_token_data(token);
		}
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;
	NodeType get_ref_node_type() override {return NodeType::ListRef;}
	std::unique_ptr<NodeVariable> to_variable() override;
	std::unique_ptr<NodeArray> to_array(std::unique_ptr<NodeAST> size) override;
	std::unique_ptr<NodeNDArray> to_ndarray() override;
};

struct NodeConst final : NodeDataStructure {
    std::unique_ptr<NodeBlock> constants;
    explicit NodeConst(Token tok) : NodeDataStructure("", TypeRegistry::Unknown, std::move(tok), NodeType::Const) {}
    NodeConst(std::string name, std::unique_ptr<NodeBlock> constants, Token tok)
            : NodeDataStructure(std::move(name), TypeRegistry::Unknown, std::move(tok), NodeType::Const), constants(std::move(constants)) {
        NodeConst::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
    }
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        constants->update_token_data(token);
    }
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;

};

struct NodeStruct final : NodeDataStructure {
	std::shared_ptr<NodePointer> node_self = std::make_shared<NodePointer>(std::nullopt, "self", TypeRegistry::add_object_type(this->name), this->tok);
	std::unique_ptr<NodeBlock> members;
	std::unordered_map<std::string, std::weak_ptr<NodeDataStructure>> member_table;
	std::set<std::string> member_set;
	std::shared_ptr<NodeFunctionDefinition> constructor = nullptr;
	std::vector<std::shared_ptr<NodeFunctionDefinition>> methods;
	std::set<std::string> method_set;
	std::unordered_map<StringIntKey, std::weak_ptr<NodeFunctionDefinition>, StringIntKeyHash> method_table;
	std::unordered_set<NodeType> member_node_types;
	std::shared_ptr<NodeVariable> max_individual_structs_var = nullptr;
	std::unique_ptr<NodeAST> max_individual_structs_count = nullptr;
	std::shared_ptr<NodeVariable> free_idx_var = nullptr;
	std::shared_ptr<NodeArray> allocation_var = nullptr;
	std::shared_ptr<NodeArray> stack_var = nullptr;
	std::shared_ptr<NodeVariable> stack_top_var = nullptr;
	std::unordered_map<token, std::weak_ptr<NodeFunctionDefinition>> overloaded_operators;
	std::unordered_set<NodeStruct*> recursive_structs;
	inline static std::unordered_set<NodeType> allowed_member_node_types = {NodeType::Variable, NodeType::Pointer, NodeType::NDArray, NodeType::Array};
	explicit NodeStruct(const std::string& name, Token tok) : NodeDataStructure(name, TypeRegistry::add_object_type(name), std::move(tok), NodeType::Struct) {}
	NodeStruct(const std::string& name, std::unique_ptr<NodeBlock> members, std::vector<std::shared_ptr<NodeFunctionDefinition>> methods, Token tok)
		: NodeDataStructure(name, TypeRegistry::add_object_type(name), std::move(tok), NodeType::Struct), members(std::move(members)), methods(std::move(methods)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeStruct(const NodeStruct& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		members->update_parents(this);
		for (const auto & m : methods) {
			m->update_parents(this);
		}
	}
	void set_child_parents() override {
		members->parent = this;
		for(auto& m : methods) {
			if(m) m->parent = this;
		}
	}
	std::string get_string() override { return ""; }
	void update_token_data(const Token& token) override {
		members->update_token_data(token);
		for(auto& m : methods) {
			if(m) m->update_token_data(token);
		}
	}
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	ASTLowering* get_lowering(NodeProgram *program) const override;
	void pre_lower(NodeProgram* program);
	void rebuild_member_table() {
		member_table.clear();
		for(const auto& member : members->statements) {
			if(const auto decl = member->statement->cast<NodeSingleDeclaration>()) {
				member_table[decl->variable->name] = decl->variable;
			} else {
				auto error = CompileError(ErrorType::VariableError, "<Struct> member must be a declaration", "", tok);
				error.exit();
			}
		}
	}
	void replace_member_in_table(const std::shared_ptr<NodeDataStructure>& old_member, const std::shared_ptr<NodeDataStructure>& new_member) {
		if(old_member->name == new_member->name) {
			member_table[old_member->name] = new_member;
		} else {
			member_table.erase(old_member->name);
			member_table[new_member->name] = new_member;
		}
	}
	void rebuild_method_table() {
		method_table.clear();
		for(auto& method : methods) {
			method_table.insert({{method->header->name, (int)method->header->params.size()}, method});
		}
	}
	std::shared_ptr<NodeFunctionDefinition> add_method(const std::shared_ptr<NodeFunctionDefinition>& method) {
		methods.push_back(method);
		method_table.insert({{method->header->name, (int)method->header->params.size()}, method});
		return method;
	}

	void rebuild_lookup_sets() {
		method_set.clear();
		for(const auto& method : methods) {
			method_set.emplace(method->header->name);
		}
		member_set.clear();
		for(auto& member : member_table) {
			method_set.emplace(member.first);
		}
	}

	std::shared_ptr<NodeDataStructure> get_member(const std::string& ref_name) {
		const auto member = member_table.find(ref_name);
		if(member != member_table.end()) {
			return member->second.lock();
		}
		return nullptr;
	}

	static std::unique_ptr<NodeBlock> declare_struct_constants();
	/// generated init method only needs assignment if it has pointer -> nil
	std::shared_ptr<NodeFunctionDefinition> generate_init_method();

	/**
	 * generates a __repr__ method for a struct
	 * @param obj
	 * // standard str function if not overwritten
	 * function __repr__(self)
	 * 	 return "<struct> Object: "& self
	 * end function
	 */
	std::shared_ptr<NodeFunctionDefinition> generate_repr_method();
	void generate_ref_count_methods(NodeProgram* program);
	std::unique_ptr<NodeWhile> generate_ref_count_while(std::shared_ptr<NodeDataStructure> self, std::shared_ptr<NodeDataStructure> num_refs);
	void inline_struct(NodeProgram* program);
	std::shared_ptr<NodeFunctionDefinition> get_overloaded_method(token op);

	/// Funktion zur rekursiven Sammlung von rekursiven NodeStructs
	void collect_recursive_structs(NodeProgram* program);
};