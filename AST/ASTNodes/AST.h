//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <list>
#include <chrono>
#include <functional>
#include <typeindex>

#include "ASTHelper.h"
#include "../Types.h"
#include "../../misc/HashFunctions.h"

class ASTDesugaring;

struct NodeAST {
    Token tok;
	Type* ty = nullptr;
    NodeType node_type;
    NodeAST* parent = nullptr;
	explicit NodeAST(Token tok=Token(), NodeType node_type=NodeType::DeadCode);
	virtual ~NodeAST() = default;
    NodeAST(const NodeAST& other);
    [[nodiscard]] virtual std::unique_ptr<NodeAST> clone() const = 0;
	virtual NodeAST* accept(class ASTVisitor &visitor);
	virtual NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
		throw std::runtime_error("replace_child not implemented for this node type");
		return nullptr;
	};
	NodeAST* replace_with(std::unique_ptr<NodeAST> newNode);
    // Hinzugefügte Methode zum Aktualisieren der Parent-Pointer
    virtual void update_parents(NodeAST* new_parent) {
        parent = new_parent;
    }
	virtual void set_child_parents() {};
    virtual std::string get_string() = 0;
    virtual void update_token_data(const Token& token) {
        tok.line = token.line; tok.file = token.file;
    }
    [[nodiscard]] virtual ASTDesugaring *get_desugaring(struct NodeProgram *program) const {
        return nullptr;
    }
	[[nodiscard]] virtual class ASTLowering * get_lowering(NodeProgram *program) const {
		return nullptr;
	}
	[[nodiscard]] virtual class ASTLowering * get_post_lowering(NodeProgram *program) const {
		return nullptr;
	}
	[[nodiscard]] virtual class ASTLowering * get_data_lowering(NodeProgram *program) const {
		return nullptr;
	}
	virtual NodeAST* desugar(NodeProgram* program);
	virtual NodeAST* lower(NodeProgram* program);
	virtual NodeAST* post_lower(NodeProgram* program);
	virtual NodeAST* data_lower(NodeProgram* program);
    [[nodiscard]] NodeType get_node_type() const { return node_type; }
	/// attempts to set the element type of this node to element_type if node has Composite Type
	/// and elemen_type is Basic Type
	Type* set_element_type(Type *element_type);
	void debug_print();
	virtual std::unique_ptr<struct NodeAccessChain> to_method_chain() {return nullptr;}
	bool is_constant();
	int get_bison_tokens();
	bool is_nil();
	/// removes node from AST and all references from data_structs
	NodeAST *remove_node();
	/// performs ASTVariableChecking class on provided node to add refs to data_structs
	void collect_references();
	/// performs ASTVariableChecking class on provided node and removes vars and refs from datastrucs
	void remove_references();
	// Template-Methode für den Cast
	template <typename TargetType>
	TargetType* cast() {
		// Überprüfen, ob der Typ des Objekts mit `TargetType` übereinstimmt
		if (std::type_index(typeid(*this)) == std::type_index(typeid(TargetType))) {
			return static_cast<TargetType*>(this);
		}
		return nullptr;
	}
	/// returns the last most NodeBlock
	[[nodiscard]] struct NodeBlock* get_parent_block() const;
	/// returns the last most NodeStatement
	[[nodiscard]] struct NodeStatement* get_parent_statement() const;
	[[nodiscard]] NodeBlock* get_outmost_block() const;
	[[nodiscard]] struct NodeCallback* get_current_callback() const;
	[[nodiscard]] struct NodeFunctionDefinition* get_current_function() const;
	void do_constant_folding();
};

template<typename T>
std::unique_ptr<T> clone_as(NodeAST* node) {
	auto cloned_ptr = node->clone(); // Nutzt die clone()-Methode der NodeAST Klasse
	return std::unique_ptr<T>(static_cast<T*>(cloned_ptr.release()));
}

template <typename T>
T* get_parent_of_type(const NodeAST& node) {
	NodeAST* current = node.parent;
	while (current) {
		if (auto desired = current->cast<T>()) {
			return desired;
		}
		current = current->parent;
	}
	return nullptr;
}

struct NodeDeadCode : NodeAST {
    explicit NodeDeadCode(const Token tok) : NodeAST(tok, NodeType::DeadCode) {};
    NodeAST *accept(struct ASTVisitor &visitor) override;
    NodeDeadCode(const NodeDeadCode& other) : NodeAST(other) {}
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {return "";}
};

struct NodeReference : NodeAST {
    std::string name;
    std::weak_ptr<class NodeDataStructure> declaration;
    bool is_engine = false;
    bool is_local = false;
	enum Kind{Builtin, Compiler, User, Throwaway};
	Kind kind = User;
	DataType data_type = DataType::Mutable;
    inline explicit NodeReference(Token tok) : NodeAST(std::move(tok), NodeType::DeadCode) {}
    inline NodeReference(std::string name, NodeType node_type, Token tok)
            : NodeAST(tok, node_type), name(std::move(name)) {}
	~NodeReference() override;
    NodeAST* accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeReference(const NodeReference& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return name;
    }
	virtual std::unique_ptr<struct NodeArrayRef> to_array_ref(std::unique_ptr<NodeAST> index) {return nullptr;}
	virtual std::unique_ptr<struct NodeVariableRef> to_variable_ref() {return nullptr;}
	virtual std::unique_ptr<struct NodePointerRef> to_pointer_ref() {return nullptr;}
	virtual std::unique_ptr<struct NodeNDArrayRef> to_ndarray_ref() {return nullptr;}
	std::unique_ptr<NodeAccessChain> to_method_chain() override {return nullptr;}
	[[nodiscard]] std::shared_ptr<NodeDataStructure> get_declaration() const;
	/// Completes the data structure of reference by copying missing parameters of declaration
	void match_data_structure(const std::shared_ptr<NodeDataStructure>& data_structure);
    /// Determines if current reference is function argument
    bool is_func_arg();
	std::unique_ptr<struct NodeFunctionCall> wrap_in_get_ui_id();
	bool needs_get_ui_id();
	/// determines if reference is reference to struct member
	[[nodiscard]] bool is_member_ref() const;
	/// checks if reference is raw version of multi-dimensional array
	bool is_raw_array() {
		return (name[0] == '_' && name[1] != '_') or name.ends_with(".raw");
	}
	/// when is variable = raw array? if variable has _ in front and is array and was declared without _
	/// returns sanitized name of reference
	[[nodiscard]] std::string sanitize_name() const {
		if (name.empty()) return "";
		std::string_view sanitized_name = name;
		if (sanitized_name[0] == '_' && (sanitized_name.size() == 1 || sanitized_name[1] != '_')) {
			sanitized_name.remove_prefix(1); // Effizienter als erase
		} else if (sanitized_name.size() >= 4 && sanitized_name.substr(sanitized_name.size() - 4) == ".raw") {
			sanitized_name.remove_suffix(4); // Effizienter als replace
		}
		return std::string(sanitized_name); // Erzeugt das Resultat nur einmal
	}

	[[nodiscard]] std::vector<std::string> get_ptr_chain() const {
		std::vector<std::string> ptr_chain;
		std::istringstream iss(name);
		std::string ns;
		while (std::getline(iss, ns, '.')) {
			ptr_chain.push_back(std::move(ns));
		}
		return ptr_chain;
	}

	static struct NodeStruct* get_object_ptr(NodeProgram* program, const std::string& obj);
	/// lower type from object to int if applicable
	NodeReference* lower_type();
	/// checks if reference is l_value in an assignment
	bool is_l_value();
	/// checks if reference is somewhere in the r_value expresssion
	bool is_r_value();
	/// checks if reference is in a string representation (printing or string assignment)
	[[nodiscard]] bool is_string_env();
	virtual std::unique_ptr<NodeReference> inflate_dimension(std::unique_ptr<NodeAST> new_index) {
		return nullptr;
	}
	void remove_obj_prefix() {
		size_t pos = name.find(OBJ_DELIMITER);
		if (pos != std::string::npos)
			name = name.substr(pos + OBJ_DELIMITER.size());
	}
	/// to be used on references
	NodeReference *replace_reference(std::unique_ptr<NodeReference> new_node);
};

struct NodeDataStructure : NodeAST, public std::enable_shared_from_this<NodeDataStructure> {
	bool is_used = false;
	bool is_engine = false;
	std::optional<Token> persistence;
	bool is_local = false;
	bool is_global = false;
	bool has_obj_assigned = false;
	DataType data_type;
	std::string name;
	std::unordered_set<NodeReference*> references;
	inline NodeDataStructure(std::string name, Type* ty, Token tok, NodeType node_type) : NodeAST(std::move(tok), node_type), name(std::move(name)) {
        this->ty = ty;
    }
	NodeAST* accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeDataStructure(const NodeDataStructure& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
    virtual std::unique_ptr<NodeReference> to_reference();
	/// determines if current data structure is local variable and sets is_local flag
	bool determine_locality(class NodeProgram* program, struct NodeBlock* current_block);
	/// determines if current data structure is a parameter in a function definition
	bool is_function_param();
	/// determines if current data structure is member of a struct, if yes returns pointer to struct
	NodeStruct* is_member();
	/// tries to infer the type by specializing given type from Number to Integer
	virtual Type* cast_type();
	/// returns fitting reference node type for the data structures
	virtual NodeType get_ref_node_type() {return NodeType::DeadCode;}
	void match_metadata(const std::shared_ptr<NodeDataStructure>& data_structure);
	/// methods to change node type. Everything possible is copied over, even the type;
	virtual std::unique_ptr<class NodeVariable> to_variable() {return nullptr;}
	virtual std::unique_ptr<class NodePointer> to_pointer() {return nullptr;}
	virtual std::unique_ptr<class NodeArray> to_array(std::unique_ptr<NodeAST> size) {return nullptr;}
	virtual std::unique_ptr<class NodeNDArray> to_ndarray() {return nullptr;}
	virtual std::unique_ptr<class NodeList> to_list() {return nullptr;}
	/// lower type from object to int if applicable
	NodeDataStructure* lower_type();
	virtual std::unique_ptr<NodeDataStructure> inflate_dimension(std::unique_ptr<NodeAST> new_index) {
		return nullptr;
	}
	/// to be used on datastructures
	NodeDataStructure *replace_datastruct(std::unique_ptr<NodeDataStructure> new_node);
	bool is_num_elements_constant() const {
		if(data_type != DataType::Const) return false;
		size_t pos = name.find(OBJ_DELIMITER+"num_elements");
		if(pos == std::string::npos) return false;
		return true;
	}
	std::shared_ptr<NodeDataStructure> get_shared() {
		return shared_from_this();
	}
	bool is_shared() {
		return !weak_from_this().expired();
	}
	void add_reference(NodeReference* ref) {
		references.insert(ref);
	}
	void remove_reference(NodeReference* ref) {
		references.erase(ref);
	}
	void clear_references();
	void to_global() {
		is_global = true;
		is_local = false;
	}
};

struct NodeInstruction : NodeAST {
    inline explicit NodeInstruction(NodeType node_type, Token tok) : NodeAST(std::move(tok), node_type) {};
    ~NodeInstruction() override = default;
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeInstruction(const NodeInstruction& other) : NodeAST(other) {};
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return "";
    }
};

struct NodeExpression : NodeAST {
    inline explicit NodeExpression(NodeType node_type, Token tok) : NodeAST(std::move(tok), node_type) {};
    ~NodeExpression() override = default;
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeExpression(const NodeExpression& other) : NodeAST(other) {};
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return "";
    }
};

struct NodeWildcard : NodeAST {
	std::string value;
	inline explicit NodeWildcard(std::string v, Token tok) : NodeAST(std::move(tok), NodeType::Wildcard), value(v) {}
	NodeAST* accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeWildcard(const NodeWildcard& other) : NodeAST(other), value(other.value) {}
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return value;
	}
	bool check_semantic();
};

struct NodeInt : NodeAST {
	int32_t value;
	inline explicit NodeInt(int32_t v, Token tok) : NodeAST(std::move(tok), NodeType::Int), value(v) {}
	NodeAST* accept(struct ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeInt(const NodeInt& other) : NodeAST(other), value(other.value) {}
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return std::to_string(value);
	}
};

struct NodeReal : NodeAST {
    double value;
    inline explicit NodeReal(double value, Token tok) : NodeAST(std::move(tok), NodeType::Real), value(value) {}
    NodeAST* accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeReal(const NodeReal& other) : NodeAST(other), value(other.value) {}
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return std::to_string(value);
    }
};

struct NodeString : NodeAST {
    std::string value;
    inline explicit NodeString(std::string value, Token tok) : NodeAST(std::move(tok), NodeType::String), value(std::move(value)) {}
    NodeAST* accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeString(const NodeString& other) : NodeAST(other), value(other.value) {}
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return value;
    }
};

struct NodeReferenceList: NodeAST {
	std::vector<std::unique_ptr<NodeReference>> references;
	inline explicit NodeReferenceList(Token tok) : NodeAST(std::move(tok), NodeType::ReferenceList) {
		set_child_parents();
	}
	inline explicit NodeReferenceList(std::vector<std::unique_ptr<NodeReference>> references, Token tok) : NodeAST(std::move(tok), NodeType::ReferenceList), references(std::move(references)) {
		set_child_parents();
	}
	// Variadischer Template-Konstruktor
	template<typename... References>
	explicit NodeReferenceList(Token tok, References&&... references) : NodeAST(std::move(tok), NodeType::ReferenceList) {
		(add_reference(std::move(references)), ...);
	}

	NodeAST* accept(struct ASTVisitor &visitor) override;
	NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Kopierkonstruktor
	NodeReferenceList(const NodeReferenceList& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(auto& ref : references) ref->update_parents(this);
	}
	void set_child_parents() override {
		for(auto& ref : references) {
			if(ref) ref->parent = this;
		}
	};
	std::string get_string() override {
		std::string str;
		if(references.empty()) return str;
		for(auto & ref : references) str += ref->get_string() + ", ";
		return str.erase(str.size() - 2);
	}
	void update_token_data(const Token& token) override {
		for(auto &p : references) p->update_token_data(token);
	}
//	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	/**
	 * @brief Returns the index of the given node in the parameter list.
	 * @param node Pointer to the node to find.
	 * @return Index of the node if found, otherwise -1.
	 */
	int get_idx(NodeAST* node);
	std::unique_ptr<NodeReference>& param(int idx) {
		return references.at(idx);
	}
	[[nodiscard]] size_t size() const {
		return references.size();
	}
	[[nodiscard]] bool empty() const {
		return references.empty();
	}
	void add_reference(std::unique_ptr<NodeReference> ref);
	void prepend_reference(std::unique_ptr<NodeReference> ref);


};

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params{};
    inline explicit NodeParamList(Token tok) : NodeAST(std::move(tok), NodeType::ParamList) {
        set_child_parents();
    }
    inline explicit NodeParamList(std::vector<std::unique_ptr<NodeAST>> params, Token tok) : NodeAST(std::move(tok), NodeType::ParamList), params(std::move(params)) {
		set_child_parents();
	}
	// Variadischer Template-Konstruktor
	template<typename... Args>
	explicit NodeParamList(Token tok, Args&&... args) : NodeAST(std::move(tok), NodeType::ParamList) {
		(params.push_back(std::move(args)), ...);
		set_child_parents();
	}

	NodeAST* accept(struct ASTVisitor &visitor) override;
	NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeParamList(const NodeParamList& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(auto& param : params) param->update_parents(this);
    }
	void set_child_parents() override {
		for(auto& param : params) {
			if(param) param->parent = this;
		}
	};
    std::string get_string() override {
        std::string str;
		if(params.empty()) return str;
        for(auto & p : params) {
            str += p->get_string() + ", ";
        }
        return str.erase(str.size() - 2);
    }
    void update_token_data(const Token& token) override {
        for(auto &p : params) p->update_token_data(token);
    }
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	/**
	 * @brief Returns the index of the given node in the parameter list.
	 * @param node Pointer to the node to find.
	 * @return Index of the node if found, otherwise -1.
	 */
	int get_idx(NodeAST* node);
	std::unique_ptr<NodeAST>& param(int idx) {
		return params.at(idx);
	}
	[[nodiscard]] size_t size() const {
		return params.size();
	}
	[[nodiscard]] bool empty() const {
		return params.empty();
	}
	/**
	 * @brief Adds a new parameter to the parameter list.
	 * @param param Unique pointer to the new parameter node.
	 */
	void add_param(std::unique_ptr<NodeAST> param);
	/**
	 * @brief Prepends a new parameter to the beginning of the parameter list.
	 *
	 * @param param Unique pointer to the new parameter node.
	 */
	void prepend_param(std::unique_ptr<NodeAST> param);
	/**
	 * @brief Flattens the parameter list by removing nested parameter lists.
	 */
	void flatten();

	/**
	 * @brief Converts the parameter list to an initializer list.
	 *
	 * @return Unique pointer to the created NodeInitializerList.
	 */
	std::unique_ptr<struct NodeInitializerList> to_initializer_list();
};

struct NodeInitializerList: NodeAST {
	std::vector<std::unique_ptr<NodeAST>> elements;
	inline explicit NodeInitializerList(Token tok) : NodeAST(std::move(tok), NodeType::InitializerList) {
		set_child_parents();
	}
	inline explicit NodeInitializerList(std::vector<std::unique_ptr<NodeAST>> elements, Token tok)
	: NodeAST(std::move(tok), NodeType::InitializerList), elements(std::move(elements)) {
		set_child_parents();
	}
	template<typename... Args>
	explicit NodeInitializerList(Token tok, Args &&... args) : NodeAST(std::move(tok), NodeType::InitializerList) {
		(elements.push_back(std::move(args)), ...);
		set_child_parents();
	}

	NodeAST *accept(struct ASTVisitor &visitor) override;
	NodeAST *replace_child(NodeAST *oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Kopierkonstruktor
	NodeInitializerList(const NodeInitializerList &other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;

	void update_parents(NodeAST *new_parent) override {
		parent = new_parent;
		for (auto &el : elements) el->update_parents(this);
	}

	void set_child_parents() override {
		for (auto &el : elements) el->parent = this;
	};

	std::string get_string() override {
		std::string str;
		if (elements.empty()) return str;
		for (auto &p : elements) {
			str += p->get_string() + ", ";
		}
		return str.erase(str.size() - 2);
	}

	void update_token_data(const Token &token) override {
		for (auto &el : elements) el->update_token_data(token);
	}

	void add_element(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		elements.push_back(std::move(param));
	}
	void prepend_element(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		elements.insert(elements.begin(), std::move(param));
	}
	std::unique_ptr<NodeAST>& elem(int idx) {
		return elements.at(idx);
	}
	[[nodiscard]] size_t size() const {
		return elements.size();
	}
	[[nodiscard]] bool empty() const {
		return elements.empty();
	}
	/// recursively flattens the initializer list
	void flatten();

	/**
	 * @brief Calculates the dimensions of a multi- or one-dimensional initializer list.
	 *
	 * This method calculates the dimensions of a multi-dimensional parameter list by recursively
	 * traversing the list and checking the sizes of nested lists. If the sizes of nested lists are
	 * inconsistent, an exception is thrown.
	 *
	 * @return A vector containing the sizes of each dimension.
	 * @throws CompileError if the sizes of nested lists are inconsistent.
	 */
	[[nodiscard]] std::vector<int> get_dimensions() const;
};

struct NodeUnaryExpr : NodeAST {
    token op;
    std::unique_ptr<NodeAST> operand;
    inline explicit NodeUnaryExpr(Token tok) : NodeAST(std::move(tok), NodeType::UnaryExpr) {}
    inline NodeUnaryExpr(token op, std::unique_ptr<NodeAST> operand, Token tok) : NodeAST(std::move(tok), NodeType::UnaryExpr), operand(std::move(operand)), op(std::move(op)) {
		set_child_parents();
	}
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeUnaryExpr(const NodeUnaryExpr& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        operand->update_parents(this);
    }
	void set_child_parents() override {
		if(operand) operand->parent = this;
	};
    std::string get_string() override {
        return tokenStrings[(int)op] + operand->get_string();
    }
    void update_token_data(const Token& token) override {
        operand -> update_token_data(token);
    }
};

struct NodeBinaryExpr: NodeAST {
	std::unique_ptr<NodeAST> left, right;
	token op;
    bool has_forced_parenth = false;
    inline explicit NodeBinaryExpr(Token tok) : NodeAST(std::move(tok), NodeType::BinaryExpr) {}
    inline NodeBinaryExpr(token op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right, Token tok)
    	: NodeAST(std::move(tok), NodeType::BinaryExpr), op(op), left(std::move(left)), right(std::move(right)) {
		set_child_parents();
	}
	NodeAST* accept(struct ASTVisitor &visitor) override;
	NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeBinaryExpr(const NodeBinaryExpr& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        left->update_parents(this);
        right->update_parents(this);
    }
	void set_child_parents() override {
		if(left) left->parent = this;
		if(right) right->parent = this;
	};
    std::string get_string() override {
        return left->get_string() + tokenStrings[(int)op] + right->get_string();
    }
    void update_token_data(const Token& token) override {
        left -> update_token_data(token);
        right -> update_token_data(token);
    }
	/// declare ndarray3[10,10,10,10] -> declare _ndarray3[10 * (10 * (10 * 10))]
	static std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, token op);
	/// ndarray3[4,5, 6, 7] -> _ndarray3[(4 * ((10 * 10) * 10)) + ((5 * (10 * 10)) + ((6 * 10) + 7))]
	static std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeCallback: NodeAST {
	bool is_thread_safe = true;
    std::string begin_callback;
    std::unique_ptr<NodeAST> callback_id = nullptr;
    std::unique_ptr<NodeBlock> statements;
    std::string end_callback;
    explicit NodeCallback(Token tok);
	NodeCallback(std::string begin_callback, std::unique_ptr<NodeBlock> statements, std::string end_callback, Token tok);
    ~NodeCallback();
	NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeCallback(const NodeCallback& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
	void set_child_parents() override;;
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override;
};

struct NodeImport : NodeAST {
    std::string filepath;
    std::string alias;
    inline explicit NodeImport(std::string filepath, std::string alias, Token tok)
        : NodeAST(std::move(tok), NodeType::Import), filepath(std::move(filepath)), alias(std::move(alias)) {}
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeImport(const NodeImport& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {}
};

struct NodeFunctionDefinition: NodeAST, public std::enable_shared_from_this<NodeFunctionDefinition> {
	/// is tagged when restricted builtin functions are used within this function (save_array, load_array, etc)
	bool is_restricted = false;
	/// is tagged when non thread-safe builtin functions are used within this function (wait, wait_asnyc, etc)
	bool is_thread_safe = true;
    bool is_used = false;
    bool is_compiled = false;
	bool visited = false;
	int num_return_params = 0;
	int num_return_stmts = 0;
	std::vector<struct NodeReturn*> return_stmts;
    std::unordered_set<class NodeFunctionCall*> call_sites = {};
    std::shared_ptr<struct NodeFunctionHeader> header;
    std::optional<std::shared_ptr<NodeDataStructure>> return_variable;
    bool override = false;
    std::unique_ptr<NodeBlock> body;
    explicit NodeFunctionDefinition(Token tok);
    NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
						   std::optional<std::unique_ptr<NodeDataStructure>> returnVariable, bool override,
						   std::unique_ptr<NodeBlock> body, Token tok);
    ~NodeFunctionDefinition() override;
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeFunctionDefinition(const NodeFunctionDefinition& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
	void set_child_parents() override;;
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override;
	[[nodiscard]] ASTLowering *get_lowering(NodeProgram *program) const override;
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	bool is_method();
	void update_param_data_type() const;
	std::shared_ptr<NodeDataStructure>& get_param(int i);
	[[nodiscard]] size_t get_num_params() const;
	[[nodiscard]] bool has_no_params() const;
	bool is_expression_function();
	std::shared_ptr<NodeFunctionDefinition> get_shared() {
		return shared_from_this();
	}
	void do_register_reuse(NodeProgram* program);
	void do_return_param_promotion();
};

struct NodeProgram : NodeAST {
	class DefinitionProvider* def_provider = nullptr;
	class ReferenceManager* ref_manager = nullptr;
	NodeCallback* init_callback = nullptr;
	NodeCallback* current_callback = nullptr;
	/// holds the current function definition that is being processed
	std::stack<std::weak_ptr<NodeFunctionDefinition>> function_call_stack{};
	std::shared_ptr<NodeFunctionDefinition> get_curr_function() {
		if(function_call_stack.empty()) return nullptr;
		return function_call_stack.top().lock();
	}
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::shared_ptr<NodeFunctionDefinition>> function_definitions;
	std::vector<std::shared_ptr<NodeFunctionDefinition>> additional_function_definitions;
	std::unordered_map<StringIntKey, std::weak_ptr<NodeFunctionDefinition>, StringIntKeyHash> function_lookup;
	std::vector<std::unique_ptr<struct NodeStruct>> struct_definitions;
	std::unordered_map<std::string, NodeStruct*> struct_lookup;
	std::unique_ptr<NodeBlock> global_declarations;
	explicit NodeProgram(Token tok);
	NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::shared_ptr<NodeFunctionDefinition>> functionDefinitions, Token tok);
	~NodeProgram() override;
    NodeAST* accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeProgram(const NodeProgram& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
	void set_child_parents() override;;
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override {}
	/// update function lookup table
	void update_function_lookup();
	void add_function_definition(const std::shared_ptr<NodeFunctionDefinition>& def);
	void update_struct_lookup();
	/// Checks for uniqueness of all callbacks except "on ui_control"
	bool check_unique_callbacks();
	/// Checks for existence and uniqueness of "on init" callback
	/// If found, returns pointer to the callback node
	NodeCallback* move_on_init_callback();
	/// merges vector of additional function definitions into the main function definitions vector
	void merge_function_definitions();
	static std::unique_ptr<NodeBlock> declare_compiler_variables();
	void inline_global_variables();
	void inline_structs();
	void reset_function_visited_flag();
	void reset_function_used_flag();
	bool is_init_callback(NodeCallback* curr_callback) const {
		return curr_callback == init_callback;
	}
	void remove_unused_functions();
	void order_function_definitions();
};




