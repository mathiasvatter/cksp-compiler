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


#include "../Tokenizer/Tokens.h"
#include "../Tokenizer/Tokenizer.h"

enum ASTType {
    Integer,
    Real,
    Number,
    Boolean,
    Comparison,
    String,
    Unknown,
    Any,
	ParamList,
    Void,
	StatementList,
	Statement,
    Compiler

};

inline std::string type_to_string(ASTType type) {
	switch (type) {
		case Integer: return "Integer";
		case Real: return "Real";
		case String: return "String";
		case Any: return "Any";
		case Unknown: return "Unknown";
		default: return "Invalid";
	}
}

inline ASTType token_to_type(token tok) {
    switch (tok) {
        case INT: return Integer;
        case FLOAT: return Real;
        case STRING: return String;
        default: return Integer;
    }
};

inline token type_to_token(ASTType type) {
	switch (type) {
		case Integer: return INT;
		case Real: return FLOAT;
		case String: return STRING;
		default: return INT;
	}
};

enum VarType {
    Const,
    Polyphonic,
    Array,
    List,
    Mutable,
	Define,
    UI_Control,
};

struct NodeAST {
    Token tok;
    ASTType type;
    NodeAST* parent = nullptr;
    inline explicit NodeAST(const Token tok=Token()) : tok(tok), type(ASTType::Unknown) {}
	virtual ~NodeAST() = default;
    // Virtuelle clone()-Methode für tiefe Kopien
    virtual std::unique_ptr<NodeAST> clone() const = 0;
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

struct NodeDeadEnd : NodeAST {
    NodeDeadEnd(const Token tok) : NodeAST(tok) {};
    void accept(ASTVisitor& visitor) override;
    NodeDeadEnd(const NodeDeadEnd& other) : NodeAST(other.tok) {}
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {return "";}
};

struct NodeInt : NodeAST {
	int32_t value;
	inline explicit NodeInt(int32_t v, const Token tok) : NodeAST(tok), value(v) {type = ASTType::Integer;}
	void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeInt(const NodeInt& other) : NodeAST(other.tok), value(other.value) {}
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return std::to_string(value);
    }
};

struct NodeReal : NodeAST {
    double value;
    inline explicit NodeReal(double value, Token tok) : NodeAST(tok), value(value) {type = ASTType::Real;}
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
    inline explicit NodeString(std::string value, Token tok) : NodeAST(tok), value(std::move(value)) {type = ASTType::String;}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeString(const NodeString& other) : NodeAST(other.tok), value(other.value) {}
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return value;
    }
};

struct NodeVariable: NodeAST {
    bool is_used = false;
    bool is_engine = false;
    bool is_persistent;
    bool is_local;
	bool is_global;
    bool is_compiler;
    VarType var_type = VarType::Mutable;
	std::string name;
    NodeAST* declaration; // index in declaration array
	inline NodeVariable(bool is_persistent, std::string name, VarType type, Token tok) : NodeAST(tok), is_persistent(is_persistent), name(std::move(name)), var_type(type) {}
	void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeVariable(const NodeVariable& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override {
        return name;
    }
};

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params;
    inline explicit NodeParamList(std::vector<std::unique_ptr<NodeAST>> params, Token tok) : NodeAST(tok), params(std::move(params)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeParamList(const NodeParamList& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeArray : NodeAST {
    bool is_used = false;
    bool is_engine = false;
    bool is_persistent;
    bool is_local;
	bool is_global;
    bool is_compiler;
    int dimensions = 1;
    VarType var_type = VarType::Array;
    std::string name;
    std::unique_ptr<NodeParamList> sizes = nullptr;
    std::unique_ptr<NodeParamList> indexes;
    NodeAST* declaration;
    inline explicit NodeArray(Token tok) : NodeAST(tok) {}
    inline NodeArray(bool is_persistent, std::string name, VarType var_type, std::unique_ptr<NodeParamList> sizes,
              std::unique_ptr<NodeParamList> indexes, Token tok)
              : NodeAST(tok), is_persistent(is_persistent), name(std::move(name)), var_type(var_type),
              sizes(std::move(sizes)), indexes(std::move(indexes)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeArray(const NodeArray& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        if (sizes) sizes->update_parents(this);
        if (indexes) indexes->update_parents(this);
    }
    std::string get_string() override {
        return name;
    }
    void update_token_data(const Token& token) override {
        if(sizes) sizes -> update_token_data(token);
        if(indexes) indexes ->update_token_data(token);
    }
};

struct NodeUIControl : NodeAST {
    std::string ui_control_type;
    std::unique_ptr<NodeAST> control_var; //Array or Variable
    std::unique_ptr<NodeParamList> params;
	std::vector<ASTType> arg_ast_types;
	std::vector<VarType> arg_var_types;
    inline explicit NodeUIControl(Token tok) : NodeAST(tok) {}
    inline NodeUIControl(std::string uiControlType, std::unique_ptr<NodeAST> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
                : NodeAST(tok), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {}
    void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeUIControl(const NodeUIControl& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        control_var->update_parents(this);
        if (params) params->update_parents(this);
    }
    std::string get_string() override {
        return control_var->get_string();
    }
    void update_token_data(const Token& token) override {
        control_var -> update_token_data(token);
        params -> update_token_data(token);
    }
};

struct NodeUnaryExpr : NodeAST {
    Token op;
    std::unique_ptr<NodeAST> operand;
    inline explicit NodeUnaryExpr(Token tok) : NodeAST(tok) {}
    inline NodeUnaryExpr(Token op, std::unique_ptr<NodeAST> operand, Token tok) : NodeAST(tok), operand(std::move(operand)), op(std::move(op)) {}
    void accept(ASTVisitor& visitor) override;
    void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeUnaryExpr(const NodeUnaryExpr& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
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
    inline explicit NodeBinaryExpr(Token tok) : NodeAST(tok) {}
    inline NodeBinaryExpr(std::string op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right, Token tok)
    : NodeAST(tok), op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeBinaryExpr(const NodeBinaryExpr& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
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
    inline explicit NodeAssignStatement(Token tok) : NodeAST(tok) {}
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok), array_variable(std::move(array_variable)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeAssignStatement(const NodeAssignStatement& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
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
    inline explicit NodeSingleAssignStatement(Token tok) : NodeAST(tok) {}
    NodeSingleAssignStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeAST(tok), array_variable(std::move(arrayVariable)), assignee(std::move(assignee)) {}
    void accept(ASTVisitor& visitor) override;
    virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignStatement(const NodeSingleAssignStatement& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
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
	std::unique_ptr<NodeParamList> to_be_declared;
	std::unique_ptr<NodeParamList> assignee;
//    std::unique_ptr<NodeAST> statement;
    inline explicit NodeDeclareStatement(Token tok) : NodeAST(tok) {}
	inline NodeDeclareStatement(std::unique_ptr<NodeParamList> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok), to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
    // Copy Constructor
    NodeDeclareStatement(const NodeDeclareStatement& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        to_be_declared->update_parents(this);
        if(assignee) assignee->update_parents(this);
    }
    std::string get_string() override {
        return "declare " + to_be_declared->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        to_be_declared -> update_token_data(token);
        assignee -> update_token_data(token);
    }
};

struct NodeSingleDeclareStatement : NodeAST {
    std::unique_ptr<NodeAST> to_be_declared;
    std::unique_ptr<NodeAST> assignee;
    inline explicit NodeSingleDeclareStatement(Token tok) : NodeAST(tok) {}
    NodeSingleDeclareStatement(std::unique_ptr<NodeAST> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
    : NodeAST(tok), to_be_declared(std::move(arrayVariable)), assignee(std::move(assignee)) {}
    void accept(ASTVisitor& visitor) override;
    virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclareStatement(const NodeSingleDeclareStatement& other);
    // Clone Method
    std::unique_ptr<NodeAST> clone() const override;
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
        assignee -> update_token_data(token);
    }
};

struct NodeGetControlStatement : NodeAST {
    std::unique_ptr<NodeAST> ui_id; //array or variable
    std::string control_param;
    inline NodeGetControlStatement(std::unique_ptr<NodeAST> uiId, std::string controlParam, Token tok)
    : NodeAST(tok), ui_id(std::move(uiId)), control_param(std::move(controlParam)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeGetControlStatement(const NodeGetControlStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeSetControlStatement : NodeAST {
    std::unique_ptr<NodeGetControlStatement> get_control;
    std::unique_ptr<NodeAST> assignee;
    inline NodeSetControlStatement(std::unique_ptr<NodeGetControlStatement> getControl, std::unique_ptr<NodeAST> assignee, Token tok)
    : NodeAST(tok), get_control(std::move(getControl)), assignee(std::move(assignee)) {}
    void accept(ASTVisitor& visitor) override;
//	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeSetControlStatement(const NodeSetControlStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        get_control->update_parents(this);
        assignee->update_parents(this);
    }
    std::string get_string() override {
        return get_control->get_string() + " := " + assignee->get_string();
    }
    void update_token_data(const Token& token) override {
        get_control -> update_token_data(token);
        assignee -> update_token_data(token);
    }
};

struct NodeStatementList;

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
    std::vector<NodeAST*> function_inlines = {};
    inline explicit NodeStatement(Token tok) : NodeAST(tok) {type = Statement;}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeAST(tok), statement(std::move(statement)) {type = Statement;}
	void accept(ASTVisitor& visitor) override;
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Kopierkonstruktor
    NodeStatement(const NodeStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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


struct NodeStatementList : NodeAST {
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeStatementList(Token tok) : NodeAST(tok) {type = StatementList;}
    inline NodeStatementList(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeAST(tok), statements(std::move(statements)) {type = StatementList;}
    void accept(ASTVisitor& visitor) override;
    NodeStatementList(const NodeStatementList& other);
    std::unique_ptr<NodeAST> clone() const override;
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
};

struct NodeConstStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeStatement>> constants;
    inline explicit NodeConstStatement(Token tok) : NodeAST(tok) {}
    inline NodeConstStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> constants, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), constants(std::move(constants)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeConstStatement(const NodeConstStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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
    inline explicit NodeStructStatement(Token tok) : NodeAST(tok) {}
    inline NodeStructStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), members(std::move(members)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeStructStatement(const NodeStructStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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
    inline explicit NodeFamilyStatement(Token tok) : NodeAST(tok) {}
    inline NodeFamilyStatement(std::string prefix, std::vector<std::unique_ptr<NodeStatement>> members, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), members(std::move(members)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeFamilyStatement(const NodeFamilyStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeListStatement : NodeAST {
    std::string name;
    int32_t size;
    std::vector<std::unique_ptr<NodeParamList>> body;
    inline explicit NodeListStatement(Token tok) : NodeAST(tok) {}
    inline NodeListStatement(std::string name, int32_t size, std::vector<std::unique_ptr<NodeParamList>> body, Token tok)
    : NodeAST(tok), name(std::move(name)), size(size), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeListStatement(const NodeListStatement& other);
    // Clone Methode
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        for (auto & b : body) {
            b->update_parents(this);
        }
    }
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        for(auto &b : body) {
            b->update_token_data(token);
        }
    }
};

struct NodeIfStatement: NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeStatementList> statements;
	std::unique_ptr<NodeStatementList> else_statements;
    inline explicit NodeIfStatement(Token tok) : NodeAST(tok) {}
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeStatementList> statements,std::unique_ptr<NodeStatementList> elseStatements, Token tok)
    : NodeAST(tok), condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeIfStatement(const NodeIfStatement& other);
    std::unique_ptr<NodeAST> clone() const override;
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
    std::unique_ptr<NodeAssignStatement> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step;
    std::unique_ptr<NodeStatementList> statements;
    inline explicit NodeForStatement(Token tok) : NodeAST(tok) {}
    inline NodeForStatement(std::unique_ptr<NodeAssignStatement> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeStatementList> statements, Token tok)
    : NodeAST(tok), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForStatement(const NodeForStatement& other);
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeWhileStatement : NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeStatementList> statements;
    inline explicit NodeWhileStatement(Token tok) : NodeAST(tok) {}
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeStatementList> statements, Token tok)
    : NodeAST(tok), condition(std::move(condition)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeWhileStatement(const NodeWhileStatement& other);
    std::unique_ptr<NodeAST> clone() const override;
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
	std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> cases;
    inline explicit NodeSelectStatement(Token tok) : NodeAST(tok) {}
	inline NodeSelectStatement(std::unique_ptr<NodeAST> expression, std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> cases, Token tok)
    : NodeAST(tok), expression(std::move(expression)), cases(std::move(cases)) {}
	void accept(ASTVisitor& visitor) override;
    NodeSelectStatement(const NodeSelectStatement& other);
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        expression->update_parents(this);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_parents(this);
            }
            for(auto &stmt : pair.second) {
                stmt->update_parents(this);
            }
        }
    }
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {
        expression -> update_token_data(token);
        for (auto & pair : cases) {
            for(auto &stmt : pair.first) {
                stmt->update_token_data(token);
            }
            for(auto &stmt : pair.second) {
                stmt->update_token_data(token);
            }
        }
    }
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    std::unique_ptr<NodeAST> callback_id;
    std::unique_ptr<NodeStatementList> statements;
    std::string end_callback;
    inline explicit NodeCallback(Token tok) : NodeAST(tok) {}
	inline NodeCallback(std::string begin_callback, std::unique_ptr<NodeStatementList> statements, std::string end_callback, Token tok)
     : NodeAST(tok), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {}
	void accept(ASTVisitor& visitor) override;
    virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeCallback(const NodeCallback& other);
    std::unique_ptr<NodeAST> clone() const override;
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
    : NodeAST(tok), filepath(std::move(filepath)), alias(std::move(alias)) {}
    void accept(ASTVisitor& visitor) override;
    NodeImport(const NodeImport& other);
    std::unique_ptr<NodeAST> clone() const override;
    std::string get_string() override { return ""; }
    void update_token_data(const Token& token) override {}
};

struct NodeFunctionHeader: NodeAST {
    bool is_engine = false;
    std::string name;
    std::unique_ptr<NodeParamList> args;
    std::vector<ASTType> arg_ast_types;
    std::vector<VarType> arg_var_types;
    inline explicit NodeFunctionHeader(Token tok) : NodeAST(tok) {}
    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
    : NodeAST(tok), name(std::move(name)), args(std::move(args)) {};
    void accept(ASTVisitor& visitor) override;
    NodeFunctionHeader(const NodeFunctionHeader& other);
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeFunctionCall : NodeAST {
    bool is_call=false;
    std::unique_ptr<NodeFunctionHeader> function;
    inline explicit NodeFunctionCall(Token tok) : NodeAST(tok) {}
    inline NodeFunctionCall(bool isCall, std::unique_ptr<NodeFunctionHeader> function, Token tok)
    : NodeAST(tok), is_call(isCall), function(std::move(function)) {}
    void accept(ASTVisitor& visitor) override;
    NodeFunctionCall(const NodeFunctionCall& other);
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeFunctionDefinition: NodeAST {
    bool is_used = false;
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeParamList>> return_variable;
    bool override;
    std::unique_ptr<NodeStatementList> body;
    inline explicit NodeFunctionDefinition(Token tok) : NodeAST(tok) {}
    inline NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
	   std::optional<std::unique_ptr<NodeParamList>> returnVariable, bool override,
       std::unique_ptr<NodeStatementList> body, Token tok)
	   : NodeAST(tok), header(std::move(header)), return_variable(std::move(returnVariable)), override(override),
	   body(std::move(body)) {};
    void accept(ASTVisitor& visitor) override;
    NodeFunctionDefinition(const NodeFunctionDefinition& other);
    std::unique_ptr<NodeAST> clone() const override;
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

struct NodeProgram : NodeAST {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    inline explicit NodeProgram(Token tok) : NodeAST(tok) {}
    inline NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions, Token tok)
                       : NodeAST(tok), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)) {}
    void accept(ASTVisitor& visitor) override;
    // Kopierkonstruktor
    NodeProgram(const NodeProgram& other);
    std::unique_ptr<NodeAST> clone() const override;
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




