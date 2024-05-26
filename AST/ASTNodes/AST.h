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

#include "ASTHelper.h"
#include "../Types.h"

class ASTDesugaring;

struct NodeAST {
    Token tok;
    ASTType type;
	Type* ty = nullptr;
    NodeType node_type;
    NodeAST* parent = nullptr;
	NodeAST(const Token tok=Token(), NodeType node_type=NodeType::DeadCode);
	virtual ~NodeAST() = default;
    NodeAST(const NodeAST& other);
    [[nodiscard]] virtual std::unique_ptr<NodeAST> clone() const = 0;
	virtual void accept(class ASTVisitor& visitor);
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {};
	void replace_with(std::unique_ptr<NodeAST> newNode);
    // Hinzugefügte Methode zum Aktualisieren der Parent-Pointer
    virtual void update_parents(NodeAST* new_parent) {
        parent = new_parent;
    }
	virtual void set_child_parents() {};
    virtual std::string get_string() = 0;
    virtual void update_token_data(const Token& token) {
        tok.line = token.line; tok.file = token.file;
    }
	[[nodiscard]] virtual ASTVisitor* get_lowering(class DefinitionProvider* def_provider) const {
		return nullptr;
	}
    [[nodiscard]] virtual ASTDesugaring* get_desugaring() const {
        return nullptr;
    }
    [[nodiscard]] NodeType get_node_type() const { return node_type; }
};

template<typename T>
std::unique_ptr<T> clone_as(NodeAST* node) {
	auto cloned_ptr = node->clone(); // Nutzt die clone()-Methode der NodeAST Klasse
	return std::unique_ptr<T>(static_cast<T*>(cloned_ptr.release()));
}

struct NodeDeadCode : NodeAST {
    explicit NodeDeadCode(const Token tok) : NodeAST(tok, NodeType::DeadCode) {};
    void accept(ASTVisitor& visitor) override;
    NodeDeadCode(const NodeDeadCode& other) : NodeAST(other) {}
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {return "";}
};

struct NodeReference : NodeAST {
    std::string name;
    class NodeDataStructure* declaration = nullptr;
    bool is_engine = false;
    bool is_local = false;
    bool is_compiler_return = false;
	DataType data_type = DataType::Mutable;
    inline explicit NodeReference(Token tok) : NodeAST(std::move(tok), NodeType::DeadCode) {}
    inline NodeReference(std::string name, NodeType node_type, Token tok)
            : NodeAST(tok, node_type), name(std::move(name)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeReference(const NodeReference& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return name;
    }
	/// Completes the data structure of reference by copying missing parameters of declaration
	void match_data_structure(NodeDataStructure* data_structure);
};

struct NodeDataStructure : NodeAST {
	bool is_used = false;
	bool is_engine = false;
	std::optional<Token> persistence;
	bool is_local = false;
	bool is_global = false;
	bool is_compiler_return = false;
	DataType data_type;
	std::string name;
	inline explicit NodeDataStructure(std::string name, Token tok, NodeType node_type) : NodeAST(std::move(tok), node_type), name(std::move(name)) {}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeDataStructure(const NodeDataStructure& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
    virtual std::unique_ptr<NodeReference> to_reference();
	/// determines if current data structure is local variable and sets is_local flag
	bool determine_locality(class NodeProgram* program, class NodeBody* current_body, class NodeCallback* current_callback);
};

struct NodeInt : NodeAST {
	int32_t value;
	inline explicit NodeInt(int32_t v, Token tok) : NodeAST(std::move(tok), NodeType::Int), value(v) {type = ASTType::Integer;}
	void accept(ASTVisitor& visitor) override;
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
    inline explicit NodeReal(double value, Token tok) : NodeAST(std::move(tok), NodeType::Real), value(value) {type = ASTType::Real;}
    void accept(ASTVisitor& visitor) override;
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
    inline explicit NodeString(std::string value, Token tok) : NodeAST(std::move(tok), NodeType::String), value(std::move(value)) {type = ASTType::String;}
    void accept(ASTVisitor& visitor) override;
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

	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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
};

struct NodeUnaryExpr : NodeAST {
    token op;
    std::unique_ptr<NodeAST> operand;
    inline explicit NodeUnaryExpr(Token tok) : NodeAST(std::move(tok), NodeType::UnaryExpr) {}
    inline NodeUnaryExpr(token op, std::unique_ptr<NodeAST> operand, Token tok) : NodeAST(std::move(tok), NodeType::UnaryExpr), operand(std::move(operand)), op(std::move(op)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeParamList> array_variable;
    std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeAssignStatement(Token tok) : NodeAST(std::move(tok), NodeType::AssignStatement) {}
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(std::move(tok), NodeType::AssignStatement), array_variable(std::move(array_variable)), assignee(std::move(assignee)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeAssignStatement(const NodeAssignStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        array_variable->update_parents(this);
        assignee->update_parents(this);
    }
	void set_child_parents() override {
		if(array_variable) array_variable->parent = this;
		if(assignee) assignee->parent = this;
	};
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleAssignStatement : NodeAST {
    std::unique_ptr<NodeAST> array_variable;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleAssignStatement(Token tok) : NodeAST(std::move(tok), NodeType::SingleAssignStatement) {}
    NodeSingleAssignStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeAST(std::move(tok), NodeType::SingleAssignStatement), array_variable(std::move(arrayVariable)), assignee(std::move(assignee)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignStatement(const NodeSingleAssignStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        array_variable->update_parents(this);
        assignee->update_parents(this);
    }
	void set_child_parents() override {
		if(array_variable) array_variable->parent = this;
		if(assignee) assignee->parent = this;
	};
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
};

struct NodeDeclareStatement : NodeAST {
	std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared;
	std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeDeclareStatement(Token tok) : NodeAST(std::move(tok), NodeType::DeclareStatement) {}
	inline NodeDeclareStatement(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
    	: NodeAST(std::move(tok), NodeType::DeclareStatement), to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeDeclareStatement(const NodeDeclareStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
	void set_child_parents() override {
		for(auto& decl : to_be_declared) {
			if(decl) decl->parent = this;
		}
		if(assignee) assignee->parent = this;
	};
    std::string get_string() override {
        std::string str = "declare ";
        for(auto & decl : to_be_declared) {
            str += decl->get_string() + ", ";
        }
        return str.erase(str.size() - 2) + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        for(auto const &decl : to_be_declared) {
            decl->update_token_data(token);
        }
        assignee -> update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeSingleDeclareStatement : NodeAST {
    std::unique_ptr<NodeDataStructure> to_be_declared;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleDeclareStatement(Token tok) : NodeAST(std::move(tok), NodeType::SingleDeclareStatement) {}
    NodeSingleDeclareStatement(std::unique_ptr<NodeDataStructure> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
    : NodeAST(std::move(tok), NodeType::SingleDeclareStatement), to_be_declared(std::move(arrayVariable)), assignee(std::move(assignee)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclareStatement(const NodeSingleDeclareStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        to_be_declared->update_parents(this);
        if(assignee) assignee->update_parents(this);
    }
	void set_child_parents() override {
		if(to_be_declared) to_be_declared->parent = this;
		if(assignee) assignee->parent = this;
	};
    std::string get_string() override {
        auto string = to_be_declared->get_string();
        if(assignee)
            string += " := " + assignee->get_string();
        return string;
    }
    void update_token_data(const Token& token) override {
        to_be_declared -> update_token_data(token);
        if(assignee) assignee -> update_token_data(token);
    }
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
	/// returns new assign statement with the declared variable and assignee or neutral element. Can optionally take new
	/// variable to make reference of
	[[nodiscard]] std::unique_ptr<NodeSingleAssignStatement> to_assign_stmt(NodeDataStructure* var=nullptr);
};

struct NodeReturnStatement : NodeAST {
	std::vector<std::unique_ptr<NodeAST>> return_variables;
	inline explicit NodeReturnStatement(Token tok) : NodeAST(std::move(tok), NodeType::ReturnStatement) {}
	NodeReturnStatement(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
		: NodeAST(std::move(tok), NodeType::ReturnStatement), return_variables(std::move(return_variables)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeReturnStatement(const NodeReturnStatement& other);
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
	std::string get_string() override {
		return "";
	}
	void update_token_data(const Token& token) override {
		for(auto &ret : return_variables) {
			ret->update_token_data(token);
		}
	}
};

struct NodeGetControlStatement : NodeAST {
    std::unique_ptr<NodeAST> ui_id; //array or variable
    std::string control_param;
    inline NodeGetControlStatement(std::unique_ptr<NodeAST> ui_id, std::string controlParam, Token tok)
        : NodeAST(std::move(tok), NodeType::GetControlStatement), ui_id(std::move(ui_id)), control_param(std::move(controlParam)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeGetControlStatement(const NodeGetControlStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        ui_id->update_parents(this);
    }
	void set_child_parents() override {
		if(ui_id) ui_id->parent = this;
	};
    std::string get_string() override {
        return ui_id->get_string() + " -> " + control_param;
    }
    void update_token_data(const Token& token) override {
        ui_id -> update_token_data(token);
    }
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;

};

struct NodeBody;

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
    std::vector<NodeAST*> function_inlines = {};
    inline explicit NodeStatement(Token tok) : NodeAST(std::move(tok), NodeType::Statement) {}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeAST(std::move(tok), NodeType::Statement), statement(std::move(statement)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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


struct NodeBody : NodeAST {
	bool scope = false;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeBody(Token tok) : NodeAST(std::move(tok), NodeType::Body) {}
    inline NodeBody(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeAST(std::move(tok), NodeType::Body), statements(std::move(statements)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    NodeBody(const NodeBody& other);
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
	void append_body(std::unique_ptr<NodeBody> new_body);
    void prepend_body(std::unique_ptr<NodeBody> new_body);
	/// adds a node statement to internal vector and sets parent pointer
	void add_stmt(std::unique_ptr<NodeStatement> stmt);
	/// puts nested statement list in current one
	void cleanup_body();
};



struct NodeStructStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> members;
    inline explicit NodeStructStatement(Token tok) : NodeAST(std::move(tok), NodeType::StructStatement) {}
    inline NodeStructStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
    : NodeAST(std::move(tok), NodeType::StructStatement), prefix(std::move(prefix)), members(std::move(members)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeStructStatement(const NodeStructStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & member : members) {
            member->update_parents(this);
        }
    }
	void set_child_parents() override {
		for(auto& m : members) {
			if(m) m->parent = this;
		}
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        for(auto &m : members) {
            m->update_token_data(token);
        }
    }
};

struct NodeIfStatement: NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBody> statements;
	std::unique_ptr<NodeBody> else_statements;
    inline explicit NodeIfStatement(Token tok) : NodeAST(std::move(tok), NodeType::IfStatement) {}
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, std::unique_ptr<NodeBody> elseStatements, Token tok)
    : NodeAST(std::move(tok), NodeType::IfStatement), condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeIfStatement(const NodeIfStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
		statements->update_parents(this);
		else_statements->update_parents(this);
    }
	void set_child_parents() override {
		condition->parent = this;
		statements->parent = this;
		else_statements->parent = this;
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
		statements->update_token_data(token);
		else_statements->update_token_data(token);
    }
};

struct NodeForStatement : NodeAST {
    std::unique_ptr<NodeSingleAssignStatement> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step = nullptr;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForStatement(Token tok) : NodeAST(std::move(tok), NodeType::ForStatement) {}
    inline NodeForStatement(std::unique_ptr<NodeSingleAssignStatement> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBody> statements, Token tok)
    : NodeAST(std::move(tok), NodeType::ForStatement), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForStatement(const NodeForStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        iterator->update_parents(this);
        iterator_end->update_parents(this);
        if (step) step ->update_parents(this);
        statements->update_parents(this);
    }
	void set_child_parents() override {
		iterator->parent = this;
		iterator_end->parent = this;
		if(step) step->parent = this;
		statements->parent = this;
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        iterator -> update_token_data(token);
        to.line = token.line; to.file = token.file;
        iterator_end -> update_token_data(token);
        if(step) step ->update_token_data(token);
        statements->update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeForEachStatement : NodeAST {
    std::unique_ptr<NodeParamList> keys;
    std::unique_ptr<NodeAST> range;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForEachStatement(Token tok) : NodeAST(std::move(tok), NodeType::ForEachStatement) {}
    inline NodeForEachStatement(std::unique_ptr<NodeParamList> keys, std::unique_ptr<NodeAST> range, std::unique_ptr<NodeBody> statements, Token tok)
    : NodeAST(std::move(tok), NodeType::ForEachStatement), keys(std::move(keys)), range(std::move(range)), statements(std::move(statements)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForEachStatement(const NodeForEachStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        keys->update_parents(this);
        range->update_parents(this);
        statements->update_parents(this);
    }
	void set_child_parents() override {
		keys->parent = this;
		range->parent = this;
		statements->parent = this;
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        keys -> update_token_data(token);
        range -> update_token_data(token);
        statements->update_token_data(token);
    }
    ASTDesugaring* get_desugaring() const override;
};

struct NodeWhileStatement : NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeWhileStatement(Token tok) : NodeAST(std::move(tok), NodeType::WhileStatement) {}
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, Token tok)
    	: NodeAST(std::move(tok), NodeType::WhileStatement), condition(std::move(condition)), statements(std::move(statements)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeWhileStatement(const NodeWhileStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        statements->update_parents(this);
    }
	void set_child_parents() override {
		condition->parent = this;
		statements->parent = this;
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        statements->update_token_data(token);
    }
};

struct NodeSelectStatement : NodeAST {
	std::unique_ptr<NodeAST> expression;
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases;
    inline explicit NodeSelectStatement(Token tok) : NodeAST(std::move(tok), NodeType::SelectStatement) {}
	inline NodeSelectStatement(std::unique_ptr<NodeAST> expression, std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases, Token tok)
    : NodeAST(std::move(tok), NodeType::SelectStatement), expression(std::move(expression)), cases(std::move(cases)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
    NodeSelectStatement(const NodeSelectStatement& other);
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
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

struct NodeCallback: NodeAST {
	bool is_thread_safe = true;
    std::string begin_callback;
    std::unique_ptr<NodeAST> callback_id = nullptr;
    std::unique_ptr<NodeBody> statements;
    std::string end_callback;
    inline explicit NodeCallback(Token tok) : NodeAST(std::move(tok), NodeType::Callback) {}
	inline NodeCallback(std::string begin_callback, std::unique_ptr<NodeBody> statements, std::string end_callback, Token tok)
     : NodeAST(std::move(tok), NodeType::Callback), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {
		set_child_parents();
	}
	void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeCallback(const NodeCallback& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        if(callback_id) callback_id->update_parents(this);
        statements->update_parents(this);
    }
	void set_child_parents() override {
		if(callback_id) callback_id->parent = this;
		statements->parent = this;
	};
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        if(callback_id) callback_id -> update_token_data(token);
        statements -> update_token_data(token);
    }
};

struct NodeImport : NodeAST {
    std::string filepath;
    std::string alias;
    inline explicit NodeImport(std::string filepath, std::string alias, Token tok)
        : NodeAST(std::move(tok), NodeType::Import), filepath(std::move(filepath)), alias(std::move(alias)) {}
    void accept(ASTVisitor& visitor) override;
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
    std::vector<ASTType> arg_ast_types;
    std::vector<DataType> arg_var_types;
	std::vector<Type*> arg_types;
    inline explicit NodeFunctionHeader(Token tok) : NodeAST(std::move(tok), NodeType::FunctionHeader) {}
    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
    : NodeAST(std::move(tok), NodeType::FunctionHeader), name(std::move(name)), args(std::move(args)) {
		set_child_parents();
	};
    void accept(ASTVisitor& visitor) override;
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
    std::set<class NodeFunctionCall*> call_sites = {};
    std::set<NodeCallback*> callback_sites = {};
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeParamList>> return_variable;
    bool override = false;
    std::unique_ptr<NodeBody> body;
    inline explicit NodeFunctionDefinition(Token tok) : NodeAST(std::move(tok), NodeType::FunctionDefinition) {}
    inline NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
                                  std::optional<std::unique_ptr<NodeParamList>> returnVariable, bool override,
                                  std::unique_ptr<NodeBody> body, Token tok)
	   : NodeAST(std::move(tok), NodeType::FunctionDefinition), header(std::move(header)), return_variable(std::move(returnVariable)), override(override),body(std::move(body)) {
		set_child_parents();
	};
    void accept(ASTVisitor& visitor) override;
    NodeFunctionDefinition(const NodeFunctionDefinition& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
        if(return_variable.has_value()) return_variable.value()->update_parents(this);
    }
	void set_child_parents() override {
		header->parent = this;
		body->parent = this;
		if(return_variable.has_value()) return_variable.value()->parent = this;
	};
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override {
        header -> update_token_data(token);
        if(return_variable.has_value()) return_variable.value()->update_token_data(token);
    }
};


struct NodeFunctionCall : NodeAST {
    bool is_call = false;
    std::unique_ptr<NodeFunctionHeader> function;
    NodeFunctionDefinition* definition = nullptr;
    inline explicit NodeFunctionCall(Token tok) : NodeAST(std::move(tok), NodeType::FunctionCall) {}
    inline NodeFunctionCall(bool isCall, std::unique_ptr<NodeFunctionHeader> function, Token tok)
            : NodeAST(std::move(tok), NodeType::FunctionCall), is_call(isCall), function(std::move(function)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    NodeFunctionCall(const NodeFunctionCall& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        function->update_parents(this);
    }
	void set_child_parents() override {
		function->parent = this;
	};
    std::string get_string() override {
        return function->get_string();
    }
    void update_token_data(const Token& token) override {
        function -> update_token_data(token);
    }
	ASTVisitor* get_lowering(DefinitionProvider* def_provider) const override;
	/// attempts to get and set the definition pointer of the function call and updates the call sites of the definition
	NodeFunctionDefinition* find_definition(class NodeProgram *program);
	/// attempts to get and match metadata from builtin function to this
    NodeFunctionDefinition* find_builtin_definition(NodeProgram *program);
	/// gets and sets definition ptr or matches builtin func metadata -> throws error if not found
	bool get_definition(NodeProgram* program);
};

struct NodeProgram : NodeAST {
	NodeCallback* init_callback = nullptr;
	DefinitionProvider* def_provider = nullptr;
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> function_lookup;
    inline explicit NodeProgram(Token tok) : NodeAST(std::move(tok), NodeType::Program) {}
    inline NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions, Token tok)
                       : NodeAST(std::move(tok), NodeType::Program), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)) {
		set_child_parents();
	}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeProgram(const NodeProgram& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(auto & c : callbacks) c->update_parents(this);
        for(auto & f : function_definitions) f->update_parents(this);
    }
	void set_child_parents() override {
		for(auto& c : callbacks) c->parent = this;
		for(auto& f : function_definitions) f->parent = this;
	};
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override {}
	/// update function lookup table and falsify visited flag
	void update_function_lookup();
};




