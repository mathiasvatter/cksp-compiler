//
// Created by Mathias Vatter on 08.06.24.
//

#pragma once

#include "AST.h"
#include "../TypeRegistry.h"

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeInstruction {
    std::unique_ptr<NodeAST> statement;
    inline explicit NodeStatement(Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)) {}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)), statement(std::move(statement)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeStatement(const NodeStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        statement->update_parents(this);
    }
    void set_child_parents() override {
        if(statement) statement->parent = this;
    };
    std::string get_string() override {
        return statement->get_string();
    }
    void update_token_data(const Token& token) override {
        statement -> update_token_data(token);
    }
};

struct NodeFunctionCall : NodeInstruction {
    enum Kind{Property, Builtin, UserDefined, Undefined, Method, Constructor, Operator};
    Kind kind = Undefined;
    bool is_call = false;
	bool is_new = false;
    std::unique_ptr<class NodeFunctionHeaderRef> function;
    NodeFunctionDefinition* definition = nullptr;
	explicit NodeFunctionCall(Token tok);
	NodeFunctionCall(bool is_call, std::unique_ptr<NodeFunctionHeaderRef> function, Token tok);
	~NodeFunctionCall() override;
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeFunctionCall(const NodeFunctionCall& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
    void set_child_parents() override;
    std::string get_string() override;
    void update_token_data(const Token& token) override;
    ASTLowering* get_lowering(struct NodeProgram *program) const override;
    /// attempts to get and set the definition pointer of the function call and updates the call sites of the definition
    NodeFunctionDefinition* find_definition(class NodeProgram *program);
    /// attempts to get and match metadata from builtin function to this
    NodeFunctionDefinition* find_builtin_definition(NodeProgram *program);
    /// attempts to get property function that and set definition pointer + error handling
    NodeFunctionDefinition* find_property_definition(NodeProgram *program);
	NodeFunctionDefinition* find_constructor_definition(NodeProgram* program);
    /// gets and sets definition ptr or matches builtin func metadata -> throws error if not found when fail set to true
    bool get_definition(NodeProgram* program, bool fail=false);
	std::unique_ptr<struct NodeAccessChain> to_method_chain() override;

	/// returns true if function call is of kind: Undefined, Builtin or Property
	[[nodiscard]] bool is_builtin_kind() const;
	[[nodiscard]] std::string get_object_name() const;
	[[nodiscard]] std::string get_method_name() const;
	[[nodiscard]] bool is_string_env();

	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeNumElements : NodeInstruction {
	std::unique_ptr<NodeReference> array;
	std::unique_ptr<NodeAST> dimension;
	std::unique_ptr<NodeAST> size;
	inline explicit NodeNumElements(Token tok) : NodeInstruction(NodeType::NumElements, std::move(tok)) {}
	inline NodeNumElements(std::unique_ptr<NodeReference> array, std::unique_ptr<NodeAST> dimension, Token tok)
		: NodeInstruction(NodeType::NumElements, std::move(tok)), array(std::move(array)), dimension(std::move(dimension)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeNumElements(const NodeNumElements& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		array->update_parents(this);
		if(dimension) dimension->update_parents(this);
		if(size) size->update_parents(this);
	}
	void set_child_parents() override {
		array->parent = this;
		if(dimension) dimension->parent = this;
		if(size) size->parent = this;
	};
	std::string get_string() override {
		std::string num_elements = "num_elements[" + array->get_string();
		if(dimension) {
			num_elements += ", " + dimension->get_string() + "]";
		}
		return num_elements;
	}
	void update_token_data(const Token& token) override {
		array->update_token_data(token);
		if(dimension) dimension->update_token_data(token);
		if(size) size->update_token_data(token);
	}
	ASTLowering* get_lowering(struct NodeProgram *program) const override;
	ASTLowering* get_post_lowering(struct NodeProgram *program) const override;
	void set_dimension(std::unique_ptr<NodeAST> new_dimension) {
		dimension = std::move(new_dimension);
		dimension->parent = this;
	}
};

struct NodeDelete : NodeInstruction {
	std::vector<std::unique_ptr<NodeReference>> ptrs;
	inline explicit NodeDelete(Token tok) : NodeInstruction(NodeType::Delete, std::move(tok)) {}
	NodeDelete(std::vector<std::unique_ptr<NodeReference>> delete_pointer, Token tok)
		: NodeInstruction(NodeType::Delete, std::move(tok)), ptrs(std::move(delete_pointer)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeDelete(const NodeDelete& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(auto &del : ptrs) {
			del->update_parents(this);
		}
	}
	void set_child_parents() override {
		for(auto& del : ptrs) {
			if(del) del->parent = this;
		}
	};
	std::string get_string() override {
		std::string del = "delete ";
		for(auto &d : ptrs) {
			del += d->get_string() + ", ";
		}
		return del;
	}
	void update_token_data(const Token& token) override {
		for(auto &del : ptrs) {
			del->update_token_data(token);
		}
	}
	void add_pointer(std::unique_ptr<NodeReference> ptr) {
		ptr->parent = this;
		ptrs.push_back(std::move(ptr));
	}
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;

};

struct NodeSingleDelete : NodeInstruction {
	std::unique_ptr<NodeAST> ptr;
	std::unique_ptr<NodeAST> num;
	inline explicit NodeSingleDelete(Token tok) : NodeInstruction(NodeType::SingleDelete, std::move(tok)) {}
	NodeSingleDelete(std::unique_ptr<NodeAST> ptr, std::unique_ptr<NodeAST> num, Token tok)
		: NodeInstruction(NodeType::SingleDelete, std::move(tok)), ptr(std::move(ptr)), num(std::move(num)) {
		set_child_parents();
		ty = this->ptr->ty;
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Copy Constructor
	NodeSingleDelete(const NodeSingleDelete& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		ptr->update_parents(this);
	}
	void set_child_parents() override {
		ptr->parent = this;
	};
	std::string get_string() override {
		return "delete " + ptr->get_string();
	}
	void update_token_data(const Token& token) override {
		ptr->update_token_data(token);
	}
	ASTLowering* get_lowering(struct NodeProgram *program) const override;

};

/// Node to retain a single pointer
/// only used internally in AST
struct NodeSingleRetain : NodeInstruction {
	std::unique_ptr<NodeReference> ptr;
	std::unique_ptr<NodeAST> num; // number of times to retain
	inline explicit NodeSingleRetain(Token tok) : NodeInstruction(NodeType::SingleRetain, std::move(tok)) {}
	NodeSingleRetain(std::unique_ptr<NodeReference> ptr, std::unique_ptr<NodeAST> num, Token tok)
		: NodeInstruction(NodeType::SingleRetain, std::move(tok)), ptr(std::move(ptr)),
		num(std::move(num)) {
		set_child_parents();
		ty = this->ptr->ty;
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Copy Constructor
	NodeSingleRetain(const NodeSingleRetain& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		ptr->update_parents(this);
		num->update_parents(this);
	}
	void set_child_parents() override {
		ptr->parent = this;
		num->parent = this;
	};
	std::string get_string() override {
		return "retain " + ptr->get_string() + ", " + num->get_string();
	}
	void update_token_data(const Token& token) override {
		ptr->update_token_data(token);
		num->update_token_data(token);
	}
	void set_num(std::unique_ptr<NodeAST> new_num) {
		num = std::move(new_num);
		num->parent = this;
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;

};

/// Node to retain multiple pointers
/// only used internally in AST
struct NodeRetain : NodeInstruction {
	std::vector<std::unique_ptr<NodeSingleRetain>> ptrs;
	inline explicit NodeRetain(Token tok) : NodeInstruction(NodeType::Retain, std::move(tok)) {}
	NodeRetain(std::vector<std::unique_ptr<NodeSingleRetain>> ptrs, Token tok)
		: NodeInstruction(NodeType::Retain, std::move(tok)), ptrs(std::move(ptrs)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Copy Constructor
	NodeRetain(const NodeRetain& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(auto & ptr: ptrs) ptr->update_parents(this);
	}
	void set_child_parents() override {
		for(auto & ptr: ptrs) ptr->parent = this;
	};
	std::string get_string() override {
		std::string output = "delete ";
		for(auto & ptr: ptrs) output += ptr->get_string() + ", ";
		return output;
	}
	void update_token_data(const Token& token) override {
		for(auto & ptr: ptrs) ptr->update_token_data(token);
	}
	void add_single_retain(std::unique_ptr<NodeReference> ref, std::unique_ptr<NodeAST> num) {
		auto retain = std::make_unique<NodeSingleRetain>(std::move(ref), std::move(num), tok);
		retain->parent = this;
		ptrs.push_back(std::move(retain));
	}
};

struct NodeAssignment: NodeInstruction {
    std::vector<std::unique_ptr<NodeReference>> l_values;
    std::unique_ptr<NodeParamList> r_values;
    inline explicit NodeAssignment(Token tok) : NodeInstruction(NodeType::Assignment, std::move(tok)) {}
    inline NodeAssignment(std::vector<std::unique_ptr<NodeReference>> array_variables, std::unique_ptr<NodeParamList> assignees, Token tok)
            : NodeInstruction(NodeType::Assignment, std::move(tok)), l_values(std::move(array_variables)), r_values(std::move(assignees)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    // Copy Constructor
    NodeAssignment(const NodeAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
		for(auto& l_val : l_values) l_val->update_parents(this);
		r_values->update_parents(this);
    }
    void set_child_parents() override {
		for(auto& l_val : l_values) l_val->parent = this;
		if(r_values) r_values->parent = this;
    };
    std::string get_string() override {
		std::string output;
		for(auto& l_val : l_values) output += l_val->get_string();
        return output + " := " + r_values->get_string();
    }
    void update_token_data(const Token& token) override {
		for(auto& l_val : l_values) l_val->update_token_data(token);
		r_values -> update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeSingleAssignment : NodeInstruction {
    std::unique_ptr<NodeReference> l_value;
    std::unique_ptr<NodeAST> r_value;
	std::unique_ptr<NodeSingleDelete> delete_stmt = nullptr;
	std::unique_ptr<NodeSingleRetain> retain_stmt = nullptr;
	bool has_object = false;
    inline explicit NodeSingleAssignment(Token tok) : NodeInstruction(NodeType::SingleAssignment, std::move(tok)) {}
    NodeSingleAssignment(std::unique_ptr<NodeReference> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleAssignment, std::move(tok)), l_value(std::move(arrayVariable)), r_value(std::move(assignee)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignment(const NodeSingleAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
		if(delete_stmt) delete_stmt->update_parents(this);
        l_value->update_parents(this);
        r_value->update_parents(this);
		if(retain_stmt) retain_stmt->update_parents(this);
    }
    void set_child_parents() override {
		if(delete_stmt) delete_stmt->parent = this;
        if(l_value) l_value->parent = this;
        if(r_value) r_value->parent = this;
		if(retain_stmt) retain_stmt->parent = this;
    };
    std::string get_string() override {
        return l_value->get_string() + " := " + r_value->get_string();
    }
    void update_token_data(const Token& token) override {
		if(delete_stmt) delete_stmt->update_token_data(token);
        l_value -> update_token_data(token);
        r_value -> update_token_data(token);
		if(retain_stmt) retain_stmt ->update_token_data(token);
    }
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
    ASTLowering* get_lowering(NodeProgram *program) const override;

};

struct NodeDeclaration : NodeInstruction {
    std::vector<std::unique_ptr<NodeDataStructure>> variable;
    std::unique_ptr<NodeParamList> value;
    inline explicit NodeDeclaration(Token tok) : NodeInstruction(NodeType::Declaration, std::move(tok)) {}
    inline NodeDeclaration(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::Declaration, std::move(tok)), variable(std::move(to_be_declared)), value(std::move(assignee)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    // Copy Constructor
    NodeDeclaration(const NodeDeclaration& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
    void set_child_parents() override {
        for(auto& decl : variable) {
            if(decl) decl->parent = this;
        }
        if(value) value->parent = this;
    };
    std::string get_string() override {
        std::string str = "declare ";
        for(auto & decl : variable) {
            str += decl->get_string() + ", ";
        }
        return str.erase(str.size() - 2) + value->get_string();
    }
    void update_token_data(const Token& token) override {
        for(auto const &decl : variable) {
            decl->update_token_data(token);
        }
        value -> update_token_data(token);
    }
    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeSingleDeclaration : NodeInstruction {
    std::shared_ptr<NodeDataStructure> variable;
    std::unique_ptr<NodeAST> value = nullptr;
	std::unique_ptr<NodeRetain> retain_stmt = nullptr;
	bool has_object = false;
	bool is_promoted = false;
    inline explicit NodeSingleDeclaration(Token tok) : NodeInstruction(NodeType::SingleDeclaration, std::move(tok)) {}
//    NodeSingleDeclaration(std::unique_ptr<NodeDataStructure> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
//            : NodeInstruction(NodeType::SingleDeclaration, std::move(tok)),
//			variable(std::shared_ptr<NodeDataStructure>(std::move(arrayVariable))),
//			value(std::move(assignee)) {
//        set_child_parents();
//    }
	NodeSingleDeclaration(std::shared_ptr<NodeDataStructure> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
		: NodeInstruction(NodeType::SingleDeclaration, std::move(tok)),
		  variable(std::move(arrayVariable)),
		  value(std::move(assignee)) {
		set_child_parents();
	}
	NodeSingleDeclaration(std::unique_ptr<NodeDataStructure> arrayVariable, Token tok)
		: NodeInstruction(NodeType::SingleDeclaration, std::move(tok)),
		  variable(std::move(arrayVariable)) {
		set_child_parents();
	}
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclaration(const NodeSingleDeclaration& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        variable->update_parents(this);
        if(value) value->update_parents(this);
		if(retain_stmt) retain_stmt->update_parents(this);
    }
    void set_child_parents() override {
        variable->parent = this;
        if(value) value->parent = this;
		if(retain_stmt) retain_stmt->parent = this;
    };
    std::string get_string() override {
        auto string = variable->get_string();
        if(value)
            string += " := " + value->get_string();
        return string;
    }
    void update_token_data(const Token& token) override {
        variable -> update_token_data(token);
        if(value) value -> update_token_data(token);
		if(retain_stmt) retain_stmt -> update_token_data(token);
    }
    ASTLowering* get_lowering(struct NodeProgram *program) const override;
    /// returns new assign statement with the declared variable and r_value or neutral element. Can optionally take new
    /// variable to make reference of
    [[nodiscard]] std::unique_ptr<NodeSingleAssignment> to_assign_stmt(NodeDataStructure* var=nullptr);
	void set_retain(std::unique_ptr<NodeRetain> retain) {
		retain->parent = this;
		retain_stmt = std::move(retain);
	}
	void set_value(std::unique_ptr<NodeAST> new_value) {
		value = std::move(new_value);
		value->parent = this;
	}
};

struct NodeFunctionParam : NodeInstruction {
	std::shared_ptr<NodeDataStructure> variable;
	std::unique_ptr<NodeAST> value = nullptr;
	inline explicit NodeFunctionParam(Token tok) : NodeInstruction(NodeType::FunctionParam, std::move(tok)) {}
	NodeFunctionParam(std::shared_ptr<NodeDataStructure> variable, std::unique_ptr<NodeAST> assignee, Token tok)
		: NodeInstruction(NodeType::FunctionParam, std::move(tok)),
		variable(std::move(variable)),
		value(std::move(assignee)) {
		set_child_parents();
	}
	explicit NodeFunctionParam(std::unique_ptr<NodeDataStructure> variable)
		: NodeInstruction(NodeType::FunctionParam, std::move(variable->tok)), variable(std::move(variable)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeFunctionParam(const NodeFunctionParam& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		variable->update_parents(this);
		if(value) value->update_parents(this);
	}
	void set_child_parents() override {
		variable->parent = this;
		if(value) value->parent = this;
	};
	std::string get_string() override {
		auto string = variable->get_string();
		if(value)
			string += " := " + value->get_string();
		return string;
	}
	void update_token_data(const Token& token) override {
		variable -> update_token_data(token);
		if(value) value -> update_token_data(token);
	}
	ASTLowering* get_lowering(struct NodeProgram *program) const override;
};

struct NodeReturn : NodeInstruction {
    std::vector<std::unique_ptr<NodeAST>> return_variables;
	NodeFunctionDefinition* definition = nullptr;
    inline explicit NodeReturn(Token tok) : NodeInstruction(NodeType::Return, std::move(tok)) {}
    NodeReturn(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
            : NodeInstruction(NodeType::Return, std::move(tok)), return_variables(std::move(return_variables)) {
        set_child_parents();
    }
	// Variadischer Template-Konstruktor
	template<typename... Params>
	explicit NodeReturn(Token tok, Params&&... params) : NodeInstruction(NodeType::Return, std::move(tok)) {
		(return_variables.push_back(std::move(params)), ...);
		set_child_parents();
	}
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeReturn(const NodeReturn& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(auto &ret : return_variables) {
            ret->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& ret : return_variables) {
            if(ret) ret->parent = this;
        }
    };
    void update_token_data(const Token& token) override {
        for(auto &ret : return_variables) {
            ret->update_token_data(token);
        }
    }
	void add_return_param(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		return_variables.push_back(std::move(param));
	}
};

struct NodeSingleReturn : NodeInstruction {
	std::unique_ptr<NodeAST> return_variable;
	NodeFunctionDefinition* definition = nullptr;
	inline explicit NodeSingleReturn(Token tok) : NodeInstruction(NodeType::SingleReturn, std::move(tok)) {}
	NodeSingleReturn(std::unique_ptr<NodeAST> return_variable, Token tok)
		: NodeInstruction(NodeType::SingleReturn, std::move(tok)), return_variable(std::move(return_variable)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeSingleReturn(const NodeSingleReturn& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		return_variable->update_parents(this);
	}
	void set_child_parents() override {
		return_variable->parent = this;
	};
	void update_token_data(const Token& token) override {
		return_variable->update_token_data(token);
	}
};

struct NodeBlock : NodeInstruction {
    bool scope = false;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeBlock(Token tok, bool scope=false) : NodeInstruction(NodeType::Block, std::move(tok)), scope(scope) {}
    inline NodeBlock(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeInstruction(NodeType::Block, std::move(tok)), statements(std::move(statements)) {
        set_child_parents();
    }
	// Variadischer Template-Konstruktor
	template<typename... Stmts>
	inline explicit NodeBlock(Token tok, Stmts&&... stmts) : NodeInstruction(NodeType::Block, std::move(tok)) {
		(statements.push_back(std::forward<Stmts>(stmts)), ...);
		set_child_parents();
	}
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeBlock(const NodeBlock& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & stmt : statements) {
            stmt->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& stmt : statements) {
            if(stmt) stmt->parent = this;
        }
    };
    std::string get_string() override {
        std::string str;
        for(auto & stmt : statements) {
            str += stmt->get_string();
        }
        return str;
    }
    void update_token_data(const Token& token) override {
        for(auto &stmt : statements) {
            stmt->update_token_data(token);
        }
    }
    void append_body(std::unique_ptr<NodeBlock> new_body);
    void prepend_body(std::unique_ptr<NodeBlock> new_body);
    /// adds a node statement to internal vector and sets parent pointer, returns pointer to moved object
	NodeStatement* add_stmt(std::unique_ptr<NodeStatement> stmt);
	/// prepends a node statement to internal vector and sets parent pointer
	void prepend_stmt(std::unique_ptr<NodeStatement> stmt) {
		stmt->parent = this;
		statements.insert(statements.begin(), std::move(stmt));
	}
	void prepend_as_stmt(std::unique_ptr<NodeAST> node) {
		auto node_stmt = std::make_unique<NodeStatement>(std::move(node), tok);
		prepend_stmt(std::move(node_stmt));
	}
	NodeStatement* add_as_stmt(std::unique_ptr<NodeAST> node) {
		auto node_stmt = std::make_unique<NodeStatement>(std::move(node), tok);
		add_stmt(std::move(node_stmt));
		return statements.back().get();
	}
	NodeStatement* add_as_single_retain(std::unique_ptr<NodeReference> ref, std::unique_ptr<NodeAST> num) {
		auto retain = std::make_unique<NodeSingleRetain>(std::move(ref), std::move(num), tok);
		add_as_stmt(std::move(retain));
		return statements.back().get();
	}
    /// puts nested statement list in current one
    void flatten();
	/// returns true if the block is a scope block and sets node.scope
	inline bool determine_scope() {
		scope = false;
		if(parent->get_node_type() != NodeType::Statement and !is_instance_of<NodeDataStructure>(parent)) {
			scope = true;
			return scope;
		}
		return false;
	}
	void wrap_in_loop_nest(std::vector<std::shared_ptr<NodeDataStructure>> iterators,
						   std::vector<std::unique_ptr<NodeAST>> lower_bounds,
						   std::vector<std::unique_ptr<NodeAST>> upper_bounds);
	void wrap_in_loop(std::shared_ptr<NodeDataStructure> iterator, std::unique_ptr<NodeAST> lower_bound, std::unique_ptr<NodeAST> upper_bound);
	std::unique_ptr<NodeAST>& get_statement(size_t index) {
		return statements[index]->statement;
	}
	std::unique_ptr<NodeAST>& get_last_statement() {
		return statements.back()->statement;
	}
};

struct NodeFamily : NodeInstruction {
    std::string prefix;
    std::unique_ptr<NodeBlock> members;
    inline explicit NodeFamily(Token tok) : NodeInstruction(NodeType::Family, std::move(tok)) {}
    inline NodeFamily(std::string prefix, std::unique_ptr<NodeBlock> members, Token tok)
            : NodeInstruction(NodeType::Family, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    // Kopierkonstruktor
    NodeFamily(const NodeFamily& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        members->update_parents(this);
    }
    void set_child_parents() override {
        members->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        members->update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeIf: NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBlock> if_body;
    std::unique_ptr<NodeBlock> else_body;
    inline explicit NodeIf(Token tok) : NodeInstruction(NodeType::If, std::move(tok)) {}
    inline NodeIf(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBlock> statements, std::unique_ptr<NodeBlock> elseStatements, Token tok)
            : NodeInstruction(NodeType::If, std::move(tok)), condition(std::move(condition)), if_body(std::move(statements)), else_body(std::move(elseStatements)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeIf(const NodeIf& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        if_body->update_parents(this);
        else_body->update_parents(this);
    }
    void set_child_parents() override {
        condition->parent = this;
        if_body->parent = this;
        else_body->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        if_body->update_token_data(token);
        else_body->update_token_data(token);
    }

};

struct NodeFor : NodeInstruction {
    std::unique_ptr<NodeSingleAssignment> iterator;
    token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step = nullptr;
    std::unique_ptr<NodeBlock> body;
    inline explicit NodeFor(Token tok) : NodeInstruction(NodeType::For, std::move(tok)) {}
    inline NodeFor(std::unique_ptr<NodeSingleAssignment> iterator, token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeInstruction(NodeType::For, std::move(tok)), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), body(std::move(statements)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeFor(const NodeFor& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        iterator->update_parents(this);
        iterator_end->update_parents(this);
        if (step) step ->update_parents(this);
        body->update_parents(this);
    }
    void set_child_parents() override {
        iterator->parent = this;
        iterator_end->parent = this;
        if(step) step->parent = this;
        body->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        iterator -> update_token_data(token);
        iterator_end -> update_token_data(token);
        if(step) step ->update_token_data(token);
        body->update_token_data(token);
    }
    ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeForEach : NodeInstruction {
    std::vector<std::unique_ptr<NodeReference>> keys;
    std::unique_ptr<NodeAST> range;
    std::unique_ptr<NodeBlock> body;
    inline explicit NodeForEach(Token tok) : NodeInstruction(NodeType::ForEach, std::move(tok)) {}
    inline NodeForEach(std::vector<std::unique_ptr<NodeReference>> keys, std::unique_ptr<NodeAST> range, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeInstruction(NodeType::ForEach, std::move(tok)), keys(std::move(keys)), range(std::move(range)), body(std::move(statements)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForEach(const NodeForEach& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
		for(auto &key : keys) key->update_parents(this);
        range->update_parents(this);
        body->update_parents(this);
    }
    void set_child_parents() override {
		for(auto &key : keys) key->parent = this;
        range->parent = this;
        body->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
		for(auto &key : keys) key->update_token_data(token);
        range -> update_token_data(token);
        body->update_token_data(token);
    }
    ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeWhile : NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBlock> body;
    inline explicit NodeWhile(Token tok) : NodeInstruction(NodeType::While, std::move(tok)) {}
    inline NodeWhile(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeInstruction(NodeType::While, std::move(tok)), condition(std::move(condition)), body(std::move(statements)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeWhile(const NodeWhile& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        body->update_parents(this);
    }
    void set_child_parents() override {
        condition->parent = this;
        body->parent = this;
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        body->update_token_data(token);
    }
	ASTLowering* get_lowering(struct NodeProgram *program) const override;
	void set_condition(std::unique_ptr<NodeBinaryExpr> condition) {
		condition->parent = this;
		this->condition = std::move(condition);
	}
};

struct NodeSelect : NodeInstruction {
    std::unique_ptr<NodeAST> expression;
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cases;
    inline explicit NodeSelect(Token tok) : NodeInstruction(NodeType::Select, std::move(tok)) {}
    inline NodeSelect(std::unique_ptr<NodeAST> expression, std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cases, Token tok)
            : NodeInstruction(NodeType::Select, std::move(tok)), expression(std::move(expression)), cases(std::move(cases)) {
        set_child_parents();
    }
    NodeAST * accept(struct ASTVisitor &visitor) override;
    NodeSelect(const NodeSelect& other);
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        expression->update_parents(this);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_parents(this);
            }
            pair.second->update_parents(this);
        }
    }
    void set_child_parents() override {
        if(expression) expression->parent = this;
        for(auto& pair : cases) {
            for(auto& stmt : pair.first) {
                if(stmt) stmt->parent = this;
            }
            pair.second->parent = this;
        }
    };
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        expression -> update_token_data(token);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_token_data(token);
            }
            pair.second->update_token_data(token);
        }
    }
};

struct NodeBreak : NodeInstruction {
	inline explicit NodeBreak(Token tok) : NodeInstruction(NodeType::SingleDeclaration, std::move(tok)) {}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeBreak(const NodeBreak& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
	}
	std::string get_string() override {
		return "break";
	}
	NodeWhile* get_nearest_loop() {
		NodeAST* loop = parent;
		while(loop and loop->get_node_type() != NodeType::While) {
			loop = loop->parent;
		}
		if(!loop) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
			error.m_message = "<Break> statement outside of loop. The <break> keyword can only be used inside a for- or while-loop.";
			error.exit();
		}
		return static_cast<NodeWhile*>(loop);
	}
};