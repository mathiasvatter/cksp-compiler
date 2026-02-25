//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "PreASTHelper.h"
#include "../../../AST/ASTNodes/ASTHelper.h"
#include "../../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../../misc/HashFunctions.h"
#include "../../../utils/StringUtils.h"

struct PreNodeAST {
	SourceRange range;
	Token tok;
	PreNodeType type;
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(Token tok, PreNodeAST* parent=nullptr, PreNodeType type=PreNodeType::DEAD_CODE)
		: range(tok), tok(std::move(tok)), type(type), parent(parent) {}
    virtual ~PreNodeAST() = default;
	PreNodeAST(const PreNodeAST& other) = default;
    virtual PreNodeAST *accept(class PreASTVisitor &visitor) {return nullptr;}
    // Virtuelle clone()-Methode für tiefe Kopien
    [[nodiscard]] virtual std::unique_ptr<PreNodeAST> clone() const = 0;
	virtual PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) {
		auto error = CompileError(ErrorType::InternalError, "replace_child not implemented", "", tok);
		error.exit();
		// throw std::runtime_error("replace_child not implemented for this node type");
		return nullptr;
	}
	PreNodeAST *replace_with(std::unique_ptr<PreNodeAST> newNode);
	virtual void set_child_parents() {};
    virtual void update_parents(PreNodeAST* new_parent) {
        parent = new_parent;
    }
    virtual void update_token_data(const Token &token) {
    	tok.line = token.line; tok.file = token.file;
    }
	void set_range(const Token& start, const Token& end) {
    	range = {start, end};
    }
	void set_range(const Token& token) {
    	range = SourceRange{token};
    }
	void set_range(const SourceRange& start, const SourceRange& end) {
    	range = SourceRange{start, end};
    }
	void set_range(const PreNodeAST& start, const PreNodeAST& end) {
    	range = SourceRange{start.range, end.range};
    }
	virtual std::string get_string() = 0;
	[[nodiscard]] PreNodeType get_node_type() const { return type; }
	// Template-Method for casting
	template <typename TargetType>
	TargetType* cast() {
		// Überprüfen, ob der Typ des Objekts mit `TargetType` übereinstimmt
		if (std::type_index(typeid(*this)) == std::type_index(typeid(TargetType))) {
			return static_cast<TargetType*>(this);
		}
		return nullptr;
	}
	void debug_print(const std::string &path = PRINTER_OUTPUT);
	// does import passes
	PreNodeAST* do_preprocessing(const std::string& current_file, const std::unordered_set<std::string> &imported_files, const std::unordered_map<std::string, std::string> &basename_map);
};

// Template-Funktion für sicheren Cast
template<typename T>
T* safe_cast(PreNodeAST* node, PreNodeType type) {
	if (node && node->get_node_type() == type) {  // Überprüfen des Typs
		return static_cast<T*>(node);
	}
	return nullptr;
}

template<typename T>
std::unique_ptr<T> clone_as(PreNodeAST* node) {
	return std::unique_ptr<T>(static_cast<T*>(node->clone().release()));
}

struct PreNodeLiteral : PreNodeAST {
	PreNodeLiteral(Token tok, PreNodeAST* parent, const PreNodeType type=PreNodeType::DEAD_CODE) : PreNodeAST(std::move(tok), parent, type) {}
	PreNodeLiteral(const PreNodeLiteral& other) = default;
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return tok.val;
	}

};

struct PreNodeNumber final : PreNodeLiteral {
    PreNodeNumber(Token tok, PreNodeAST* parent) : PreNodeLiteral(std::move(tok), parent, PreNodeType::NUMBER) {}
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeNumber(const PreNodeNumber& other) = default;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeInt final : PreNodeLiteral {
    int32_t integer;
    PreNodeInt(const int32_t integer, Token tok, PreNodeAST* parent) : PreNodeLiteral(std::move(tok), parent, PreNodeType::INT), integer(integer) {}
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeInt(const PreNodeInt& other) = default;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeKeyword final : PreNodeLiteral {
	PreNodeKeyword(Token tok, PreNodeAST *parent) : PreNodeLiteral(std::move(tok), parent, PreNodeType::KEYWORD) {}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeKeyword(const PreNodeKeyword& other) = default;
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeUnaryExpr final : PreNodeAST {
    token op;
    std::unique_ptr<PreNodeAST> operand;
	PreNodeUnaryExpr(Token tok, PreNodeAST *parent): PreNodeAST(std::move(tok), parent, PreNodeType::UNARY_EXPR) {
		op = tok.type;
	}
  //   PreNodeUnaryExpr(const token op, std::unique_ptr<PreNodeAST> operand, Token tok, PreNodeAST *parent)
		// : PreNodeAST(std::move(tok), parent, PreNodeType::UNARY_EXPR), op(op), operand(std::move(operand)) {
	 //    set_child_parents();
  //   }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeUnaryExpr(const PreNodeUnaryExpr& other);
    PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		operand->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        operand->update_parents(this);
    }
	std::string get_string() override {
		return get_token_string(op) + operand->get_string();
	}
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
        operand->update_token_data(token);
    }
};

struct PreNodeBinaryExpr final : PreNodeAST {
    std::unique_ptr<PreNodeAST> left, right;
    token op;
    PreNodeBinaryExpr(const token op, std::unique_ptr<PreNodeAST> left, std::unique_ptr<PreNodeAST> right, Token tok, PreNodeAST *parent)
            : PreNodeAST(std::move(tok), parent, PreNodeType::BINARY_EXPR), left(std::move(left)), right(std::move(right)), op(op) {
	    set_child_parents();
    }

    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeBinaryExpr(const PreNodeBinaryExpr& other);
    PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		left->parent = this;
		right->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        left->update_parents(this);
        right->update_parents(this);
    }
	std::string get_string() override {
		return left->get_string() + get_token_string(op) + left->get_string();;
	}
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
        left->update_token_data(token);
        right->update_token_data(token);
    }
};

struct PreNodeOther final : PreNodeAST {
	PreNodeOther(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::OTHER) {}
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeOther(const PreNodeOther& other) = default;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return tok.val;
	}
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
    }
};

struct PreNodeDeadCode final : PreNodeAST {
    PreNodeDeadCode(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::DEAD_CODE) {}
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeDeadCode(const PreNodeDeadCode& other) : PreNodeAST(other) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    std::string get_string() override {
        return tok.val;
    }
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
    }
};

struct PreNodePragma final : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> option;
    std::unique_ptr<PreNodeKeyword> argument;
	PreNodePragma(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::PRAGMA) {}
    PreNodePragma(std::unique_ptr<PreNodeKeyword> opt, std::unique_ptr<PreNodeKeyword> arg, Token tok, PreNodeAST *parent)
    	: PreNodeAST(std::move(tok), parent, PreNodeType::PRAGMA), option(std::move(opt)), argument(std::move(arg)) {
	    set_child_parents();
    }

    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodePragma(const PreNodePragma& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		option->parent = this;
		argument->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		option->update_parents(this);
		argument->update_parents(this);
	}
    std::string get_string() override {
        return option->get_string();
    }
    void update_token_data(const Token &token) override {
        option->update_token_data(token);
        argument->update_token_data(token);
    }
};

struct PreNodeStatement final : PreNodeAST {
    std::unique_ptr<PreNodeAST> statement;
	PreNodeStatement(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::STATEMENT) {}
    PreNodeStatement(std::unique_ptr<PreNodeAST> statement, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::STATEMENT), statement(std::move(statement)) {
	    set_child_parents();
    }

    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeStatement(const PreNodeStatement& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) override;
	void set_child_parents() override {
		statement->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
    	parent = new_parent;
        statement->update_parents(this);
    }
	std::string get_string() override {
		return statement->get_string();
	}
    void update_token_data(const Token &token) override {
        statement->update_token_data(token);
    }
};

struct PreNodeChunk final : PreNodeAST {
	std::vector<std::unique_ptr<PreNodeAST>> chunk = {};
	PreNodeChunk(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::CHUNK) {}
    PreNodeChunk(std::vector<std::unique_ptr<PreNodeAST>> chunk, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::CHUNK), chunk(std::move(chunk)) {
		set_child_parents();
	}

	PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeChunk(const PreNodeChunk& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) override;
	void set_child_parents() override {
		for(const auto &c : chunk) {
			c->parent = this;
		}
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for(const auto &c : chunk) {
            c->update_parents(this);
        }
    }
	std::string get_string() override {
		std::string str;
		for(const auto &c : chunk) {
			str += c->get_string();
		}
		return str;
	}
    void update_token_data(const Token &token) override {
        for(const auto & c : chunk) {
            c->update_token_data(token);
        }
    }
	/// puts nested statement list in one, returns new vector to replace node->statements with
	void flatten();
	PreNodeAST* add_chunk(std::unique_ptr<PreNodeAST> ch) {
		ch->parent = this;
		chunk.push_back(std::move(ch));
		return chunk.back().get();
	}
	PreNodeAST* prepend_chunk(std::unique_ptr<PreNodeAST> ch) {
		ch->parent = this;
		chunk.insert(chunk.begin(), std::move(ch));
		return chunk.front().get();
	}
	PreNodeAST* get_chunk(const size_t index) const {
		return chunk[index].get();
	}
	size_t num_chunks() const {
		return chunk.size();
	}
};

struct PreNodeList final : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeChunk>> params;
	PreNodeList(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::LIST) {}
    PreNodeList(std::vector<std::unique_ptr<PreNodeChunk>> params, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::LIST), params(std::move(params)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeList(const PreNodeList& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		for(const auto &p : params) {
			p->parent = this;
		}
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for(const auto &p : params) {
            p->update_parents(this);
        }
    }
	std::string get_string() override {
		std::string str;
		for(auto & p : params) {
			str += p->get_string();
		}
		return str;
	}
    void update_token_data(const Token &token) override {
        for(auto & p : params) {
            p->update_token_data(token);
        }
    }
	PreNodeChunk* add_element(std::unique_ptr<PreNodeChunk> element) {
	    element->parent = this;
    	params.push_back(std::move(element));
    	return params.back().get();
    }
	PreNodeChunk* prepend_element(std::unique_ptr<PreNodeChunk> element) {
	    element->parent = this;
    	params.insert(params.begin(), std::move(element));
    	return params.front().get();
    }
	PreNodeChunk* set_element(const size_t idx, std::unique_ptr<PreNodeChunk> element) {
	    element->parent = this;
    	if (idx < params.size()) {
			params[idx] = std::move(element);
			return params[idx].get();
    	}
    	return add_element(std::move(element));
	}
	PreNodeChunk* get_element(const size_t idx) const {
	    return params[idx].get();
	}
};

struct PreNodeImport final : PreNodeAST {
	std::string path;
	std::unique_ptr<PreNodeKeyword> alias;
	PreNodeImport(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::IMPORT) {}
	PreNodeImport(std::string path, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::IMPORT), path(std::move(path)) {
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeImport(const PreNodeImport& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		if (alias) alias->update_parents(this);
	}
	void set_child_parents() override {
		if (alias) alias->parent = this;
	}
	std::string get_string() override {
		return "import " + path + (alias ? " as " + alias->get_string() : "");
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		if (alias) alias->update_token_data(token);
	}
	void set_alias(std::unique_ptr<PreNodeKeyword> new_alias) {
		alias = std::move(new_alias);
		if (alias) alias->parent = this;
	}
};

struct PreNodeImportNCKP final : PreNodeAST {
	std::string path;
	PreNodeImportNCKP(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::IMPORT_NCKP) {}
	PreNodeImportNCKP(std::string path, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::IMPORT_NCKP), path(std::move(path)) {
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeImportNCKP(const PreNodeImportNCKP& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
	}
	std::string get_string() override {
		return "import_nckp " + path;
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
	}
};

struct PreNodeUseCodeIf final : PreNodeAST {
	std::unique_ptr<PreNodeKeyword> condition;
	std::unique_ptr<PreNodeChunk> if_branch;
	std::unique_ptr<PreNodeChunk> else_branch;
	PreNodeUseCodeIf(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::USE_CODE_IF) {}
	PreNodeUseCodeIf(std::unique_ptr<PreNodeKeyword> condition, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::USE_CODE_IF), condition(std::move(condition)) {
	    set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeUseCodeIf(const PreNodeUseCodeIf& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		condition->parent = this;
		if (if_branch) if_branch->parent = this;
		if (else_branch) else_branch->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		condition->update_parents(this);
		if (if_branch) if_branch->update_parents(this);
		if (else_branch) else_branch->update_parents(this);
	}
	std::string get_string() override {
		return "use_code_if " + condition->get_string();
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		condition->update_token_data(token);
		if (if_branch) if_branch->update_token_data(token);
		if (else_branch) else_branch->update_token_data(token);
	}
	void set_if_branch(std::unique_ptr<PreNodeChunk> chunk) {
		this->if_branch = std::move(chunk);
		if (this->if_branch) this->if_branch->parent = this;
	}
	void set_else_branch(std::unique_ptr<PreNodeChunk> chunk) {
		this->else_branch = std::move(chunk);
		if (this->else_branch) this->else_branch->parent = this;
	}
	void set_condition(std::unique_ptr<PreNodeKeyword> cond) {
		this->condition = std::move(cond);
		if (this->condition) this->condition->parent = this;
	}
};

struct PreNodeSetCondition final : PreNodeAST {
	std::unique_ptr<PreNodeKeyword> condition;
	PreNodeSetCondition(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::SET_CONDITION) {}
	PreNodeSetCondition(std::unique_ptr<PreNodeKeyword> condition, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::SET_CONDITION), condition(std::move(condition)) {
	    set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeSetCondition(const PreNodeSetCondition& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		condition->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		condition->update_parents(this);
	}
	std::string get_string() override {
		return "set_condition " + condition->get_string();
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		condition->update_token_data(token);
	}
};

struct PreNodeSetGlobalCondition final : PreNodeAST {
	std::unique_ptr<PreNodeKeyword> condition;
	PreNodeSetGlobalCondition(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::SET_GLOBAL_CONDITION) {}
	PreNodeSetGlobalCondition(std::unique_ptr<PreNodeKeyword> condition, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::SET_GLOBAL_CONDITION), condition(std::move(condition)) {
		set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeSetGlobalCondition(const PreNodeSetGlobalCondition& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		condition->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		condition->update_parents(this);
	}
	std::string get_string() override {
		return "set_global_condition " + condition->get_string();
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		condition->update_token_data(token);
	}
};

struct PreNodeResetCondition final : PreNodeAST {
	std::unique_ptr<PreNodeKeyword> condition;
	PreNodeResetCondition(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::RESET_CONDITION) {}
	PreNodeResetCondition(std::unique_ptr<PreNodeKeyword> condition, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::RESET_CONDITION), condition(std::move(condition)) {
	    set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeResetCondition(const PreNodeResetCondition& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		condition->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		condition->update_parents(this);
	}
	std::string get_string() override {
		return "reset_condition " + condition->get_string();
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		condition->update_token_data(token);
	}
};

struct PreNodeResetGlobalCondition final : PreNodeAST {
	std::unique_ptr<PreNodeKeyword> condition;
	PreNodeResetGlobalCondition(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::RESET_GLOBAL_CONDITION) {}
	PreNodeResetGlobalCondition(std::unique_ptr<PreNodeKeyword> condition, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::RESET_GLOBAL_CONDITION), condition(std::move(condition)) {
		set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeResetGlobalCondition(const PreNodeResetGlobalCondition& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		condition->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		condition->update_parents(this);
	}
	std::string get_string() override {
		return "reset_global_condition " + condition->get_string();
	}
	void update_token_data(const Token &token) override {
		tok.line = token.line; tok.file = token.file;
		condition->update_token_data(token);
	}
};



struct PreNodeMacroHeader final : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> name;
    std::unique_ptr<PreNodeList> args;
	bool has_parenth = false;
	PreNodeMacroHeader(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_HEADER) {}
    PreNodeMacroHeader(std::unique_ptr<PreNodeKeyword> name, std::unique_ptr<PreNodeList> args, Token tok, PreNodeAST *parent)
    : PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_HEADER), name(std::move(name)), args(std::move(args)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeMacroHeader(const PreNodeMacroHeader& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    PreNodeAST *replace_child(PreNodeAST *oldChild, std::unique_ptr<PreNodeAST> newChild) override;
	void set_child_parents() override {
		name->parent = this;
		args->parent = this;
	}
	std::string get_name() const {
		return name->tok.val;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        name->update_parents(this);
        args->update_parents(this);
    }
	std::string get_string() override {
		return name->get_string() + args->get_string();
	}
    void update_token_data(const Token &token) override {
        name->update_token_data(token);
        args->update_token_data(token);
    }
	void set_args(std::unique_ptr<PreNodeList> new_args) {
		args = std::move(new_args);
		args->parent = this;
	}
	PreNodeAST* add_arg(std::unique_ptr<PreNodeChunk> arg) const {
    	return args->add_element(std::move(arg));
    }
	PreNodeAST* prepend_arg(std::unique_ptr<PreNodeChunk> arg) const {
	    return args->prepend_element(std::move(arg));
    }
	PreNodeChunk* set_arg(const size_t idx, std::unique_ptr<PreNodeChunk> arg) const {
	    return args->set_element(idx, std::move(arg));
    }
	size_t get_arg(const std::string &str) const {
		for (int i=0; i< args->params.size(); i++) {
			const auto& el = args->params[i];
			if (el->get_string() == str) {
				return i;
			}
		}
		return -1;
	}
	bool has_args() const {
	    return !args->params.empty();
    }
	PreNodeChunk* get_arg(const size_t idx) const {
	    return args->params[idx].get();
    }
	size_t num_args() const {
		return args->params.size();
	}
};


struct PreNodeDefineHeader final : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> name;
    std::unique_ptr<PreNodeList> args;
	PreNodeDefineHeader(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_HEADER) {}
	PreNodeDefineHeader(std::unique_ptr<PreNodeKeyword> name, std::unique_ptr<PreNodeList> args, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_HEADER), name(std::move(name)), args(std::move(args)) {
		set_child_parents();
	}
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeDefineHeader(const PreNodeDefineHeader& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		name->parent = this;
		args->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
		name->update_parents(this);
        args->update_parents(this);
    }
	std::string get_name() const {
		return name->tok.val;
	}
	std::string get_string() override {
		return name->get_string() + args->get_string();
	}
    void update_token_data(const Token &token) override {
        name->update_token_data(token);
        args->update_token_data(token);
    }
	void set_args(std::unique_ptr<PreNodeList> new_args) {
		args = std::move(new_args);
		args->parent = this;
	}
	PreNodeChunk* add_arg(std::unique_ptr<PreNodeChunk> arg) const {
	    return args->add_element(std::move(arg));
    }
	bool has_args() const {
		return !args->params.empty();
	}
	size_t num_args() const {
		return args->params.size();
	}
	PreNodeChunk* get_arg(const size_t idx) const {
		return args->params[idx].get();
	}
};

struct PreNodeMacroDefinition final : PreNodeAST {
    std::unique_ptr<PreNodeMacroHeader> header;
    std::unique_ptr<PreNodeChunk> body;
	PreNodeMacroDefinition(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_DEFINITION) {}
    PreNodeMacroDefinition(std::unique_ptr<PreNodeMacroHeader> header, std::unique_ptr<PreNodeChunk> body, Token tok, PreNodeAST* parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_DEFINITION), header(std::move(header)), body(std::move(body)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeMacroDefinition(const PreNodeMacroDefinition& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		header->parent = this;
		body->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
    }
	std::string get_string() override {
		return header->get_string() + body->get_string();
	}
    void update_token_data(const Token &token) override {
        header->update_token_data(token);
        body->update_token_data(token);
    }
};

struct PreNodeDefineStatement final : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> header;
    std::unique_ptr<PreNodeChunk> body;
	PreNodeDefineStatement(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_STATEMENT) {}
    PreNodeDefineStatement(std::unique_ptr<PreNodeDefineHeader> header, std::unique_ptr<PreNodeChunk> body, Token tok, PreNodeAST* parent)
    : PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_STATEMENT), header(std::move(header)), body(std::move(body)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeDefineStatement(const PreNodeDefineStatement& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		header->parent = this;
		body->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
    }
	std::string get_string() override {
		return header->get_string() + body->get_string();
	}
    void update_token_data(const Token &token) override {
        header->update_token_data(token);
        body->update_token_data(token);
    }
};

struct PreNodeMacroCall final : PreNodeAST {
    std::unique_ptr<PreNodeMacroHeader> macro;
	bool is_iterate_macro = false;
	PreNodeMacroCall(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_CALL) {}
    PreNodeMacroCall(std::unique_ptr<PreNodeMacroHeader> macro, Token tok, PreNodeAST* parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::MACRO_CALL), macro(std::move(macro)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeMacroCall(const PreNodeMacroCall& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		macro->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro->update_parents(this);
    }
	std::string get_string() override {
		return macro->get_string();
	}
    void update_token_data(const Token &token) override {
        macro->update_token_data(token);
    }
};

struct PreNodeFunctionCall final : PreNodeAST {
	std::unique_ptr<PreNodeMacroHeader> function;
	PreNodeFunctionCall(std::unique_ptr<PreNodeMacroHeader> function, Token tok, PreNodeAST* parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::FUNCTION_CALL), function(std::move(function)) {
		set_child_parents();
	}
	PreNodeAST *accept(PreASTVisitor &visitor) override;
	PreNodeFunctionCall(const PreNodeFunctionCall& other);
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		function->parent = this;
	}
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		function->update_parents(this);
	}
	std::string get_string() override {
		return function->get_string();
	}
	void update_token_data(const Token &token) override {
		function->update_token_data(token);
	}
};

struct PreNodeDefineCall final : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> define;
	PreNodeDefineCall(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_CALL) {}
    PreNodeDefineCall(std::unique_ptr<PreNodeDefineHeader> define, Token tok, PreNodeAST* parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::DEFINE_CALL), define(std::move(define)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeDefineCall(const PreNodeDefineCall& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		define->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        define->update_parents(this);
    }
	std::string get_string() override {
		return define->get_string();
	}
    void update_token_data(const Token &token) override {
        define->update_token_data(token);
    }
};

struct PreNodeIterateMacro final : PreNodeAST {
    std::unique_ptr<PreNodeList> macro_call;
    std::unique_ptr<PreNodeChunk> iterator_start;
    Token to;
    std::unique_ptr<PreNodeChunk> iterator_end;
    std::unique_ptr<PreNodeChunk> step;
    explicit PreNodeIterateMacro(Token tok, PreNodeAST *parent) : PreNodeAST(tok, parent) {};
    PreNodeIterateMacro(std::unique_ptr<PreNodeList> macroCall, std::unique_ptr<PreNodeChunk> iteratorStart,
				Token to, std::unique_ptr<PreNodeChunk> iteratorEnd, std::unique_ptr<PreNodeChunk> step, Token tok, PreNodeAST *parent)
				: PreNodeAST(std::move(tok), parent, PreNodeType::ITERATE_MACRO), macro_call(std::move(macroCall)),
				iterator_start(std::move(iteratorStart)), to(std::move(to)), iterator_end(std::move(iteratorEnd)), step(std::move(step)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeIterateMacro(const PreNodeIterateMacro& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		macro_call->parent = this;
		iterator_start->parent = this;
		iterator_end->parent = this;
		step->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro_call->update_parents(this);
        iterator_start->update_parents(this);
        iterator_end->update_parents(this);
        step->update_parents(this);
    }
	std::string get_string() override {
		return macro_call->get_string() + iterator_start->get_string() + to.val + iterator_end->get_string() + step->get_string();
	}
    void update_token_data(const Token &token) override {
        macro_call->update_token_data(token);
        iterator_start->update_token_data(token);
        to.line = token.line; to.file = token.file;
        iterator_end->update_token_data(token);
        step->update_token_data(token);
    }
};

struct PreNodeLiterateMacro final : PreNodeAST {
    std::unique_ptr<PreNodeList> macro_call;
    std::unique_ptr<PreNodeChunk> literate_tokens;
	PreNodeLiterateMacro(Token tok, PreNodeAST *parent) : PreNodeAST(std::move(tok), parent, PreNodeType::LITERATE_MACRO) {}
    PreNodeLiterateMacro(std::unique_ptr<PreNodeList> macro_call, std::unique_ptr<PreNodeChunk> literate_tokens, Token tok, PreNodeAST *parent) :
            PreNodeAST(std::move(tok), parent, PreNodeType::LITERATE_MACRO), macro_call(std::move(macro_call)), literate_tokens(std::move(literate_tokens)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeLiterateMacro(const PreNodeLiterateMacro& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		macro_call->parent = this;
		literate_tokens->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro_call->update_parents(this);
        literate_tokens->update_parents(this);
    }
	std::string get_string() override {
		return macro_call->get_string() + literate_tokens->get_string();
	}
    void update_token_data(const Token &token) override {
        macro_call->update_token_data(token);
        literate_tokens->update_token_data(token);
    }
};

struct PreNodeIncrementer final : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeChunk>> body{};
    std::unique_ptr<PreNodeAST> counter;
    std::unique_ptr<PreNodeChunk> iterator_start;
    std::unique_ptr<PreNodeChunk> iterator_step;
    std::vector<bool> incrementation{};
	PreNodeIncrementer(Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::INCREMENTER) {}
    PreNodeIncrementer(std::vector<std::unique_ptr<PreNodeChunk>> body, std::unique_ptr<PreNodeAST> counter, std::unique_ptr<PreNodeChunk> iterator_start, std::unique_ptr<PreNodeChunk> iterator_step, std::vector<bool> incrementation, Token tok, PreNodeAST *parent)
		: PreNodeAST(std::move(tok), parent, PreNodeType::INCREMENTER), body(std::move(body)), counter(std::move(counter)), iterator_start(std::move(iterator_start)), iterator_step(std::move(iterator_step)), incrementation(std::move(incrementation)) {
	    set_child_parents();
    }
    PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeIncrementer(const PreNodeIncrementer& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		for (const auto & b : body) b->parent = this;
		counter->parent = this;
		iterator_start->parent = this;
		iterator_step->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for (const auto & b : body) b->update_parents(this);
        counter->update_parents(this);
        iterator_start->update_parents(this);
        iterator_step->update_parents(this);
    }
    std::string get_string() override {
        return counter->get_string() + iterator_start->get_string() + iterator_step->get_string();
    }
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
        for(const auto & b : body) b->update_token_data(token);
        counter->update_token_data(token);
        iterator_start->update_token_data(token);
        iterator_step->update_token_data(token);
    }
	void set_counter(std::unique_ptr<PreNodeAST> new_counter) {
		counter = std::move(new_counter);
		counter->parent = this;
	}
	void set_iterator_start(std::unique_ptr<PreNodeChunk> new_start) {
		iterator_start = std::move(new_start);
		iterator_start->parent = this;
	}
	void set_iterator_step(std::unique_ptr<PreNodeChunk> new_step) {
		iterator_step = std::move(new_step);
		iterator_step->parent = this;
	}
};

struct PreNodeProgram final : PreNodeAST {
	DefinitionProvider* def_provider = nullptr;
	std::stack<PreNodeDefineStatement*> define_call_stack;
	std::stack<PreNodeMacroCall*> macro_call_stack;
	std::unique_ptr<PreNodeChunk> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions;
    PreNodeProgram(std::unique_ptr<PreNodeChunk> program, std::vector<std::unique_ptr<PreNodeDefineStatement>> defines,
                   std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions, Token tok, PreNodeAST* parent)
    	: PreNodeAST(std::move(tok), parent, PreNodeType::PROGRAM), program(std::move(program)), define_statements(std::move(defines)), macro_definitions(std::move(macro_definitions)) {}

	PreNodeAST *accept(PreASTVisitor &visitor) override;
    PreNodeProgram(const PreNodeProgram& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void set_child_parents() override {
		program->parent = this;
		for(const auto & def : define_statements) def ->parent = this;
		for(const auto & def : macro_definitions) def ->parent = this;
	}
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
		program->update_parents(this);
        for(const auto & def : define_statements) def ->update_parents(this);
        for(const auto & def : macro_definitions) def ->update_parents(this);
    }
	std::string get_string() override {
		return "";
	}
    void update_token_data(const Token &token) override {}

};



