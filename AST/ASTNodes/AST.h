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

#include "ASTHelper.h"
#include "../Types.h"
#include "../TypeRegistry.h"

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
	virtual NodeAST* replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {return nullptr;};
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
	[[nodiscard]] virtual class ASTLowering * get_lowering(struct NodeProgram *program) const {
		return nullptr;
	}
    [[nodiscard]] virtual ASTDesugaring *get_desugaring(NodeProgram *program) const {
        return nullptr;
    }
	virtual NodeAST* desugar(NodeProgram* program);
	virtual NodeAST* lower(NodeProgram* program);
    [[nodiscard]] NodeType get_node_type() const { return node_type; }
	/// attempts to set the element type of this node to element_type if node has Composite Type
	/// and elemen_type is Basic Type
	Type* set_element_type(Type *element_type);
	void debug_print();
	virtual std::unique_ptr<struct NodeAccessChain> to_method_chain() {return nullptr;}

};

template<typename T>
std::unique_ptr<T> clone_as(NodeAST* node) {
	auto cloned_ptr = node->clone(); // Nutzt die clone()-Methode der NodeAST Klasse
	return std::unique_ptr<T>(static_cast<T*>(cloned_ptr.release()));
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
    class NodeDataStructure* declaration = nullptr;
    bool is_engine = false;
    bool is_local = false;
    bool is_compiler_return = false;
	enum Kind{Builtin, Compiler, User, Throwaway};
	Kind kind = User;
	DataType data_type = DataType::Mutable;
    inline explicit NodeReference(Token tok) : NodeAST(std::move(tok), NodeType::DeadCode) {}
    inline NodeReference(std::string name, NodeType node_type, Token tok)
            : NodeAST(tok, node_type), name(std::move(name)) {}
    NodeAST* accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeReference(const NodeReference& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return name;
    }
	virtual std::unique_ptr<struct NodeArrayRef> to_array_ref(NodeAST* index) {return nullptr;}
	virtual std::unique_ptr<struct NodeVariableRef> to_variable_ref() {return nullptr;}
	virtual std::unique_ptr<struct NodePointerRef> to_pointer_ref() {return nullptr;}
	virtual std::unique_ptr<struct NodeNDArrayRef> to_ndarray_ref() {return nullptr;}
	std::unique_ptr<NodeAccessChain> to_method_chain() override {return nullptr;}

	/// Completes the data structure of reference by copying missing parameters of declaration
	void match_data_structure(NodeDataStructure* data_structure);
    /// Determines if current reference is function argument
    bool is_func_arg() {
        if(!this->parent) return false;
        if(!this->parent->parent) return false;
        bool func_arg = this->parent->get_node_type() == NodeType::ParamList and
                          this->parent->parent->get_node_type() == NodeType::FunctionHeader;
        return func_arg;
    }
	std::unique_ptr<struct NodeFunctionCall> wrap_in_get_ui_id();
	/// determines if reference is reference to struct member
	[[nodiscard]] bool is_member_ref() const;
	/// checks if reference is raw version of multi-dimensional array
	bool is_raw_array() {
		return (name[0] == '_' && name[1] != '_') or name.ends_with(".raw");
	}
	/// when is variable = raw array? if variable has _ in front and is array and was declared without _
	/// returns sanitized name of reference
	[[nodiscard]] std::string sanitize_name() {
		std::string var_without_identifier = name;
		if (name[0] == '_' && name[1] != '_') {
			var_without_identifier = var_without_identifier.erase(0,1);
		} else if (name.ends_with(".raw")) {
			var_without_identifier = var_without_identifier.replace(var_without_identifier.size()-4, 4, "");
		}
		return var_without_identifier;
	}

	std::vector<std::string> get_ptr_chain() {
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

	/// determines if given reference is ui_control and belongs to ui array
//	bool is_ui_array_ref(DefinitionProvider* def_provider) {
//
//	}

};

struct NodeDataStructure : NodeAST {
	bool is_used = false;
	bool is_engine = false;
	std::optional<Token> persistence;
	bool is_local = false;
	bool is_global = false;
	bool is_compiler_return = false;
	bool has_obj_assigned = false;
	DataType data_type;
	std::string name;
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

	/// methods to change node type. Everything possible is copied over, even the type;
	virtual std::unique_ptr<class NodeVariable> to_variable() {return nullptr;}
	virtual std::unique_ptr<class NodePointer> to_pointer() {return nullptr;}
	virtual std::unique_ptr<class NodeArray> to_array(NodeAST* size) {return nullptr;}
	virtual std::unique_ptr<class NodeNDArray> to_ndarray() {return nullptr;}
	virtual std::unique_ptr<class NodeList> to_list() {return nullptr;}
	/// lower type from object to int if applicable
	NodeDataStructure* lower_type();
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

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params;
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
        for(auto& param : params) {
            param->update_parents(this);
        }
    }
	void set_child_parents() override {
		for(auto& param : params) {
			if(param) param->parent = this;
		}
	};
    std::string get_string() override {
        std::string str;
        for(auto & p : params) {
            str += p->get_string() + ", ";
        }
        return str.erase(str.size() - 2);
    }
    void update_token_data(const Token& token) override {
        for(auto &p : params) {
            p->update_token_data(token);
        }
    }
	void add_param(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		params.push_back(std::move(param));
	}
	void prepend_param(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		params.insert(params.begin(), std::move(param));
	}
	// Funktion zum Abflachen der Parameterliste
	void flatten() {
		std::vector<std::unique_ptr<NodeAST>> flat_list;
		// Rekursive Funktion, um die Parameterliste abzuflachen
		std::function<void(std::vector<std::unique_ptr<NodeAST>>)> flatten = [&](std::vector<std::unique_ptr<NodeAST>> current_node) {
		  for (auto& param : current_node) {
			  if (param->get_node_type() == NodeType::ParamList) {
				  flatten(std::move(static_cast<NodeParamList*>(param.get())->params));
			  } else {
				  // Wenn es kein NodeParamList ist, fügen wir es direkt zur Liste hinzu
				  param->parent = this;
				  flat_list.push_back(std::move(param));
			  }
		  }
		};
		flatten(std::move(params));
		params = std::move(flat_list);
	}
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
	static std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, token op) {
		// Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
		if (index >= nodes.size() - 1) {
			return nodes[index]->clone();
		}
		// Erstelle die rechte Seite der Expression rekursiv.
		auto right = create_right_nested_binary_expr(nodes, index + 1, op);
		// Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
		return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), Token());
	}
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

struct NodeFunctionHeader: NodeAST {
	bool is_thread_safe = true;
    bool is_builtin = false;
    bool has_forced_parenth = false;
    std::string name;
    std::unique_ptr<NodeParamList> args;
    inline explicit NodeFunctionHeader(Token tok) : NodeAST(std::move(tok), NodeType::FunctionHeader) {}
    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
    : NodeAST(std::move(tok), NodeType::FunctionHeader), name(std::move(name)), args(std::move(args)) {
		set_child_parents();
	};
    NodeAST* accept(struct ASTVisitor &visitor) override;
    NodeFunctionHeader(const NodeFunctionHeader& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        args->update_parents(this);
    }
	void set_child_parents() override {
		args->parent = this;
	};
    std::string get_string() override {
        return name + "(" + args->get_string() + ")";
    }
    void update_token_data(const Token& token) override {
        tok.line = token.line; tok.file = token.file;
        args -> update_token_data(token);
    }

};

struct NodeFunctionDefinition: NodeAST {
    bool is_used = false;
    bool is_compiled = false;
	bool visited = false;
	int num_return_params = 0;
	NodeAST* return_param = nullptr;
    std::unordered_set<class NodeFunctionCall*> call_sites = {};
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeDataStructure>> return_variable;
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
    void update_token_data(const Token& token) override {
        header -> update_token_data(token);
        if(return_variable.has_value()) return_variable.value()->update_token_data(token);
    }
	[[nodiscard]] ASTLowering *get_lowering(NodeProgram *program) const override;
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	bool is_method() {
		bool has_params = header->args->params.size() > 0 and header->args->params[0]->get_string() == "self";
		bool within_struct = parent and parent->get_node_type() == NodeType::Struct;
		return has_params and within_struct;
	}
};

struct NodeProgram : NodeAST {
	NodeCallback* init_callback = nullptr;
	NodeCallback* current_callback = nullptr;
	/// holds the current function definition that is being processed
	std::stack<NodeFunctionDefinition*> function_call_stack{};
	class DefinitionProvider* def_provider = nullptr;
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
	std::vector<std::unique_ptr<NodeFunctionDefinition>> additional_function_definitions;
	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> function_lookup;
	std::vector<std::unique_ptr<struct NodeStruct>> struct_definitions;
	std::unordered_map<std::string, NodeStruct*> struct_lookup;
	std::unique_ptr<NodeBlock> global_declarations;
	explicit NodeProgram(Token tok);
	NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions, Token tok);
	~NodeProgram() =default;
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
	void update_struct_lookup();
	/// Checks for uniqueness of all callbacks except "on ui_control"
	bool check_unique_callbacks();
	/// Checks for existence and uniqueness of "on init" callback
	/// If found, returns pointer to the callback node
	NodeCallback* move_on_init_callback();
	/// merges vector of additional function definitions into the main function definitions vector
	inline void merge_function_definitions() {
		function_definitions.insert(function_definitions.end(), std::make_move_iterator(additional_function_definitions.begin()),
									std::make_move_iterator(additional_function_definitions.end()));
		additional_function_definitions.clear();
		update_function_lookup();
	}
	static std::unique_ptr<NodeBlock> declare_compiler_variables();
	void inline_global_variables();
	void inline_structs();
	void reset_function_visited_flag();
	void reset_function_used_flag();
};




