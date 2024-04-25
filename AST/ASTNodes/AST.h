//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <variant>
#include <list>
#include <chrono>


#include "../../Tokenizer/Tokens.h"
#include "../../Tokenizer/Tokenizer.h"

enum class ASTType {
    Integer,
    Real,
    Number,
    Boolean,
    Comparison,
    String,
    Unknown,
    Any,
    Void,
//	StatementList,
//	Statement,
};

inline std::string type_to_string(ASTType type) {
	switch (type) {
        case ASTType::Integer: return "Integer";
		case ASTType::Real: return "Real";
		case ASTType::String: return "String";
		case ASTType::Any: return "Any";
		case ASTType::Unknown: return "Unknown";
		default: return "Invalid";
	}
}

inline ASTType token_to_type(token tok) {
    switch (tok) {
		case token::INT: return ASTType::Integer;
		case token::FLOAT: return ASTType::Real;
		case token::STRING: return ASTType::String;
        default: return ASTType::Unknown;
    }
};

inline token type_to_token(ASTType type) {
	switch (type) {
		case ASTType::Integer: return token::INT;
		case ASTType::Real: return token::FLOAT;
		case ASTType::String: return token::STRING;
		default: return token::INT;
	}
};

enum class DataType {
    Const,
    Polyphonic,
    Array,
    NDArray,
    List,
    Mutable,
	Define,
    UI_Control,
};

inline ASTType infer_type_from_identifier(std::string& var_name) {
    ASTType type = ASTType::Unknown;
    if(contains(VAR_IDENT, var_name[0]) || contains(ARRAY_IDENT, var_name[0])) {
        std::string identifier(1, var_name[0]);
        var_name = var_name.erase(0,1);
        token token_type = *get_token_type(TYPES, identifier);
        type = token_to_type(token_type);
    }
    return type;
}

enum class NodeType {
    Variable,
	VariableReference,
    Array,
	ArrayReference,
    NDArray,
	NDArrayReference,
    UIControl,
    UnaryExpr,
    BinaryExpr,
    AssignStatement,
    SingleAssignStatement,
    DeclareStatement,
    SingleDeclareStatement,
    ReturnStatement,
    GetControlStatement,
    ParamList,
    Int,
    Real,
    String,
    DeadCode,
    Statement,
    Body,
    ConstStatement,
    StructStatement,
    FamilyStatement,
    ListStruct,
	ListStructReference,
    IfStatement,
    ForStatement,
    ForEachStatement,
    WhileStatement,
    SelectStatement,
    Callback,
    Import,
    FunctionHeader,
    FunctionDefinition,
    FunctionCall,
    Program
};

struct NodeAST {
    Token tok;
    ASTType type;
    NodeType node_type;
    NodeAST* parent = nullptr;
    inline explicit NodeAST(const Token tok=Token(), NodeType node_type=NodeType::DeadCode) : tok(tok),
        type(ASTType::Unknown), node_type(node_type) {}
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
    virtual std::string get_string() = 0;
    virtual void update_token_data(const Token& token) {
        tok.line = token.line; tok.file = token.file;
    }
	[[nodiscard]] virtual ASTVisitor* get_lowering() const {
		return nullptr;
	}
    NodeType get_node_type() const { return node_type; }
};

template <typename T>
bool is_instance_of(NodeAST* node) {
    static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
    return dynamic_cast<T*>(node) != nullptr;
}

template <typename T>
T* cast_node(NodeAST* node) {
    static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
    return dynamic_cast<T*>(node);
}

// Hilfsfunktion zum Klonen von unique_ptrs
template <typename T>
std::unique_ptr<T> clone_unique(const std::unique_ptr<T>& source) {
    return source ? std::unique_ptr<T>(static_cast<T*>(source->clone().release())) : nullptr;
}

// Helper function to clone vectors of unique_ptr to NodeAST
template <typename T>
std::vector<std::unique_ptr<T>> clone_vector(const std::vector<std::unique_ptr<T>>& vec) {
    std::vector<std::unique_ptr<T>> new_vec;
    new_vec.reserve(vec.size());
    for (const auto& item : vec) {
        new_vec.push_back(clone_unique(item));
    }
    return new_vec;
}

template<typename T>
std::unique_ptr<T> clone_as(NodeAST* node) {
    auto cloned_ptr = node->clone(); // Nutzt die clone()-Methode der NodeAST Klasse
    return std::unique_ptr<T>(static_cast<T*>(cloned_ptr.release()));
}


struct NodeDeadCode : NodeAST {
    explicit NodeDeadCode(const Token tok) : NodeAST(tok, NodeType::DeadCode) {};
    void accept(ASTVisitor& visitor) override;
    NodeDeadCode(const NodeDeadCode& other) : NodeAST(other.tok) {}
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {return "";}
};

struct NodeInt : NodeAST {
	int32_t value;
	inline explicit NodeInt(int32_t v, const Token tok) : NodeAST(tok, NodeType::Int), value(v) {type = ASTType::Integer;}
	void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeInt(const NodeInt& other) : NodeAST(other.tok), value(other.value) {}
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return std::to_string(value);
    }
};

struct NodeDataStructure : NodeAST {
	bool is_used = false;
	bool is_engine = false;
	std::optional<Token> persistence;
	bool is_reference = true;
	bool is_local = false;
	bool is_global = false;
	bool is_compiler_return = false;
	DataType data_type;
	std::string name;
	NodeDataStructure* declaration = nullptr; // index in declaration array
	inline explicit NodeDataStructure(std::string name, Token tok, NodeType node_type) : NodeAST(tok, node_type), name(std::move(name)) {}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeDataStructure(const NodeDataStructure& other);
	// Clone Methode
	std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
};

struct NodeReference : NodeAST {
	std::string name;
	NodeDataStructure* declaration = nullptr;
	inline explicit NodeReference(Token tok) : NodeAST(tok, NodeType::UnaryExpr) {}
	inline NodeReference(std::string name, NodeType node_type, Token tok)
		: NodeAST(tok, node_type), name(std::move(name)) {}
	void accept(ASTVisitor& visitor) override;
	// Kopierkonstruktor
	NodeReference(const NodeReference& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {}
	std::string get_string() override {
		return name;
	}
};

struct NodeReal : NodeAST {
    double value;
    inline explicit NodeReal(double value, Token tok) : NodeAST(tok, NodeType::Real), value(value) {type = ASTType::Real;}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeReal(const NodeReal& other) : NodeAST(other.tok), value(other.value) {}
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return std::to_string(value);
    }
};

struct NodeString : NodeAST {
    std::string value;
    inline explicit NodeString(std::string value, Token tok) : NodeAST(tok, NodeType::String), value(std::move(value)) {type = ASTType::String;}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeString(const NodeString& other) : NodeAST(other.tok), value(other.value) {}
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return value;
    }
};

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params;
    inline explicit NodeParamList(std::vector<std::unique_ptr<NodeAST>> params, Token tok) : NodeAST(tok, NodeType::ParamList), params(std::move(params)) {}
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
    Token op;
    std::unique_ptr<NodeAST> operand;
    inline explicit NodeUnaryExpr(Token tok) : NodeAST(tok, NodeType::UnaryExpr) {}
    inline NodeUnaryExpr(Token op, std::unique_ptr<NodeAST> operand, Token tok) : NodeAST(tok, NodeType::UnaryExpr), operand(std::move(operand)), op(std::move(op)) {}
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
    std::string get_string() override {
        return op.val + operand->get_string();
    }
    void update_token_data(const Token& token) override {
        operand -> update_token_data(token);
        op.line = token.line; op.file = token.file;
    }
};

struct NodeBinaryExpr: NodeAST {
	std::unique_ptr<NodeAST> left, right;
	std::string op;
    bool has_forced_parenth = false;
    inline explicit NodeBinaryExpr(Token tok) : NodeAST(tok, NodeType::BinaryExpr) {}
    inline NodeBinaryExpr(std::string op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right, Token tok)
    : NodeAST(tok, NodeType::BinaryExpr), op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
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
    std::string get_string() override {
        return left->get_string() + "op.val" + right->get_string();
    }
    void update_token_data(const Token& token) override {
        left -> update_token_data(token);
        right -> update_token_data(token);
    }
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeParamList> array_variable;
    std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeAssignStatement(Token tok) : NodeAST(tok, NodeType::AssignStatement) {}
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok, NodeType::AssignStatement), array_variable(std::move(array_variable)), assignee(std::move(assignee)) {}
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
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }
};

struct NodeSingleAssignStatement : NodeAST {
    std::unique_ptr<NodeAST> array_variable;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleAssignStatement(Token tok) : NodeAST(tok, NodeType::SingleAssignStatement) {}
    NodeSingleAssignStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeAST(tok, NodeType::SingleAssignStatement), array_variable(std::move(arrayVariable)), assignee(std::move(assignee)) {}
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
    std::string get_string() override {
        return array_variable->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        array_variable -> update_token_data(token);
        assignee -> update_token_data(token);
    }
};

struct NodeDeclareStatement : NodeAST {
	std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared;
	std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeDeclareStatement(Token tok) : NodeAST(tok, NodeType::DeclareStatement) {}
	inline NodeDeclareStatement(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok, NodeType::DeclareStatement), to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeDeclareStatement(const NodeDeclareStatement& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;

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
};

struct NodeSingleDeclareStatement : NodeAST {
    std::unique_ptr<NodeAST> to_be_declared;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleDeclareStatement(Token tok) : NodeAST(tok, NodeType::SingleDeclareStatement) {}
    NodeSingleDeclareStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
    : NodeAST(tok, NodeType::SingleDeclareStatement), to_be_declared(std::move(arrayVariable)), assignee(std::move(assignee)) {}
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
	ASTVisitor* get_lowering() const override;

};

struct NodeReturnStatement : NodeAST {
	std::vector<std::unique_ptr<NodeAST>> return_variables;
	inline explicit NodeReturnStatement(Token tok) : NodeAST(tok, NodeType::ReturnStatement) {}
	NodeReturnStatement(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
		: NodeAST(tok, NodeType::ReturnStatement), return_variables(std::move(return_variables)) {}
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
    inline NodeGetControlStatement(std::unique_ptr<NodeAST> uiId, std::string controlParam, Token tok)
        : NodeAST(tok, NodeType::GetControlStatement), ui_id(std::move(uiId)), control_param(std::move(controlParam)) {}
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
    std::string get_string() override {
        return ui_id->get_string() + " -> " + control_param;
    }
    void update_token_data(const Token& token) override {
        ui_id -> update_token_data(token);
    }
};

struct NodeBody;

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
    std::vector<NodeAST*> function_inlines = {};
    inline explicit NodeStatement(Token tok) : NodeAST(tok, NodeType::Statement) {}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeAST(tok, NodeType::Statement), statement(std::move(statement)) {}
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
    inline explicit NodeBody(Token tok) : NodeAST(tok, NodeType::Body) {}
    inline NodeBody(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeAST(tok, NodeType::Body), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
    NodeBody(const NodeBody& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & stmt : statements) {
            stmt->update_parents(this);
        }
    }
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
};

struct NodeConstStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> constants;
    inline explicit NodeConstStatement(Token tok) : NodeAST(tok, NodeType::ConstStatement) {}
    inline NodeConstStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> constants, Token tok)
    : NodeAST(tok, NodeType::ConstStatement), prefix(std::move(prefix)), constants(std::move(constants)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeConstStatement(const NodeConstStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & c : constants) {
            c->update_parents(this);
        }
    }
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        for(auto &c : constants) {
            c->update_token_data(token);
        }
    }
};

struct NodeStructStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> members;
    inline explicit NodeStructStatement(Token tok) : NodeAST(tok, NodeType::StructStatement) {}
    inline NodeStructStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
    : NodeAST(tok, NodeType::StructStatement), prefix(std::move(prefix)), members(std::move(members)) {}
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
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        for(auto &m : members) {
            m->update_token_data(token);
        }
    }
};

struct NodeFamilyStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> members;
    inline explicit NodeFamilyStatement(Token tok) : NodeAST(tok, NodeType::FamilyStatement) {}
    inline NodeFamilyStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
    : NodeAST(tok, NodeType::FamilyStatement), prefix(std::move(prefix)), members(std::move(members)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeFamilyStatement(const NodeFamilyStatement& other);
    // Clone Methode
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (auto & member : members) {
            member->update_parents(this);
        }
    }
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
    inline explicit NodeIfStatement(Token tok) : NodeAST(tok, NodeType::IfStatement) {}
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, std::unique_ptr<NodeBody> elseStatements, Token tok)
    : NodeAST(tok, NodeType::IfStatement), condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {}
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
    std::unique_ptr<NodeAST> step;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForStatement(Token tok) : NodeAST(tok, NodeType::ForStatement) {}
    inline NodeForStatement(std::unique_ptr<NodeSingleAssignStatement> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBody> statements, Token tok)
    : NodeAST(tok, NodeType::ForStatement), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {}
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
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        iterator -> update_token_data(token);
        to.line = token.line; to.file = token.file;
        iterator_end -> update_token_data(token);
        if(step) step ->update_token_data(token);
        statements->update_token_data(token);
    }
};

struct NodeForEachStatement : NodeAST {
    std::unique_ptr<NodeParamList> keys;
    std::unique_ptr<NodeAST> range;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeForEachStatement(Token tok) : NodeAST(tok, NodeType::ForEachStatement) {}
    inline NodeForEachStatement(std::unique_ptr<NodeParamList> keys, std::unique_ptr<NodeAST> range, std::unique_ptr<NodeBody> statements, Token tok)
    : NodeAST(tok, NodeType::ForEachStatement), keys(std::move(keys)), range(std::move(range)), statements(std::move(statements)) {}
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
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        keys -> update_token_data(token);
        range -> update_token_data(token);
        statements->update_token_data(token);
    }
};

struct NodeWhileStatement : NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBody> statements;
    inline explicit NodeWhileStatement(Token tok) : NodeAST(tok, NodeType::WhileStatement) {}
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBody> statements, Token tok)
    : NodeAST(tok, NodeType::WhileStatement), condition(std::move(condition)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeWhileStatement(const NodeWhileStatement& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        statements->update_parents(this);
    }
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        statements->update_token_data(token);
    }
};

struct NodeSelectStatement : NodeAST {
	std::unique_ptr<NodeAST> expression;
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases;
    inline explicit NodeSelectStatement(Token tok) : NodeAST(tok, NodeType::SelectStatement) {}
	inline NodeSelectStatement(std::unique_ptr<NodeAST> expression, std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases, Token tok)
    : NodeAST(tok, NodeType::SelectStatement), expression(std::move(expression)), cases(std::move(cases)) {}
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
    std::string begin_callback;
    std::unique_ptr<NodeAST> callback_id;
    std::unique_ptr<NodeBody> statements;
    std::string end_callback;
    inline explicit NodeCallback(Token tok) : NodeAST(tok, NodeType::Callback) {}
	inline NodeCallback(std::string begin_callback, std::unique_ptr<NodeBody> statements, std::string end_callback, Token tok)
     : NodeAST(tok, NodeType::Callback), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {}
	void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeCallback(const NodeCallback& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        if(callback_id) callback_id->update_parents(this);
        statements->update_parents(this);
    }
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
        : NodeAST(tok, NodeType::Import), filepath(std::move(filepath)), alias(std::move(alias)) {}
    void accept(ASTVisitor& visitor) override;
    NodeImport(const NodeImport& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {}
};

struct NodeFunctionHeader: NodeAST {
    bool is_engine = false;
    bool has_forced_parenth = false;
    std::string name;
    std::unique_ptr<NodeParamList> args;
    std::vector<ASTType> arg_ast_types;
    std::vector<DataType> arg_var_types;
    inline explicit NodeFunctionHeader(Token tok) : NodeAST(tok, NodeType::FunctionHeader) {}
    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
    : NodeAST(tok, NodeType::FunctionHeader), name(std::move(name)), args(std::move(args)) {};
    void accept(ASTVisitor& visitor) override;
    NodeFunctionHeader(const NodeFunctionHeader& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        args->update_parents(this);
    }
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
    std::set<class NodeFunctionCall*> call = {};
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeParamList>> return_variable;
    bool override = false;
    std::unique_ptr<NodeBody> body;
    inline explicit NodeFunctionDefinition(Token tok) : NodeAST(tok, NodeType::FunctionDefinition) {}
    inline NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
                                  std::optional<std::unique_ptr<NodeParamList>> returnVariable, bool override,
                                  std::unique_ptr<NodeBody> body, Token tok)
	   : NodeAST(tok, NodeType::FunctionDefinition), header(std::move(header)), return_variable(std::move(returnVariable)), override(override),
	   body(std::move(body)) {};
    void accept(ASTVisitor& visitor) override;
    NodeFunctionDefinition(const NodeFunctionDefinition& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
        if(return_variable.has_value()) return_variable.value()->update_parents(this);
    }
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override {
        header -> update_token_data(token);
        if(return_variable.has_value()) return_variable.value()->update_token_data(token);
    }
};


struct NodeFunctionCall : NodeAST {
    bool is_call=false;
    std::unique_ptr<NodeFunctionHeader> function;
    NodeFunctionDefinition* definition = nullptr;
    inline explicit NodeFunctionCall(Token tok) : NodeAST(tok, NodeType::FunctionCall) {}
    inline NodeFunctionCall(bool isCall, std::unique_ptr<NodeFunctionHeader> function, Token tok)
            : NodeAST(tok, NodeType::FunctionCall), is_call(isCall), function(std::move(function)) {}
    void accept(ASTVisitor& visitor) override;
    NodeFunctionCall(const NodeFunctionCall& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        function->update_parents(this);
    }
    std::string get_string() override {
        return function->get_string();
    }
    void update_token_data(const Token& token) override {
        function -> update_token_data(token);
    }
};

struct NodeProgram : NodeAST {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    inline explicit NodeProgram(Token tok) : NodeAST(tok, NodeType::Program) {}
    inline NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions, Token tok)
                       : NodeAST(tok, NodeType::Program), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeProgram(const NodeProgram& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(auto & c : callbacks)
            c->update_parents(this);
        for(auto & f : function_definitions)
            f->update_parents(this);
    }
    std::string get_string() override {return "";}
    void update_token_data(const Token& token) override {}
};




