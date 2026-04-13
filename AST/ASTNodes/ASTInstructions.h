//
// Created by Mathias Vatter on 08.06.24.
//

#pragma once

#include "AST.h"
#include "../Types/TypeRegistry.h"
#include <array>

// can be assign_statement, if_statement etc.
struct NodeStatement final : NodeInstruction {
    std::unique_ptr<NodeAST> statement;
    explicit NodeStatement(Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)) {}
    NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeInstruction(NodeType::Statement, std::move(tok)), statement(std::move(statement)) {
	    NodeStatement::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
	std::string get_token_string() override {
		return statement ? statement->get_token_string() : "";
	}
    void update_token_data(const Token& token) override {
        statement -> update_token_data(token);
    }
	void set_statement(std::unique_ptr<NodeAST> new_statement) {
		statement = std::move(new_statement);
		statement->parent = this;
	}
};

struct NodeFunctionCall final : NodeInstruction {
    enum Kind{Property, Builtin, UserDefined, Undefined, Method, Constructor, Operator};
	inline static const std::array<std::string, 7> KindStrings = {"Property","Builtin","UserDefined","Undefined","Method","Constructor","Operator"};
	enum Strategy{Inlining, PreemptiveInlining, ParameterStack, Call, None, ExpressionFunc};
	inline static const std::array<std::string, 7> StrategyStrings = {"Inlining","PreemptiveInlining","ParameterStack","Call","None","ExpressionFunc"};
    Kind kind = Undefined;
	Strategy strategy = None;
    bool is_call = false;
	bool is_callable = false;
	bool is_new = false;
	bool is_temporary_constructor = false;
    std::unique_ptr<NodeFunctionHeaderRef> function;
    std::weak_ptr<NodeFunctionDefinition> definition;
	explicit NodeFunctionCall(Token tok);
	NodeFunctionCall(bool is_call, std::unique_ptr<NodeFunctionHeaderRef> function, Token tok);
	~NodeFunctionCall() override;
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeFunctionCall(const NodeFunctionCall& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override;
    void set_child_parents() override;
    std::string get_string() override;
	std::string get_token_string() override;
	[[nodiscard]] std::string get_kind_as_string() const {
		if (static_cast<size_t>(kind) < KindStrings.size()) {
			return KindStrings[static_cast<size_t>(kind)];
		}
		return "";
	}
	[[nodiscard]] std::string get_strategy_string() const {
		if (static_cast<size_t>(strategy) < StrategyStrings.size()) {
			return StrategyStrings[static_cast<size_t>(strategy)];
		}
		return "";
	}
    void update_token_data(const Token& token) override;
    ASTLowering* get_lowering(NodeProgram *program) const override;
    /// attempts to get and set the definition pointer of the function call and updates the call sites of the definition
    std::shared_ptr<NodeFunctionDefinition> find_definition(NodeProgram *program);
	std::shared_ptr<NodeFunctionDefinition> find_definition(NodeProgram *program, const std::string& name, int num_args, const Type *ty);
    /// attempts to get and match metadata from builtin function to this
    std::shared_ptr<NodeFunctionDefinition> find_builtin_definition(NodeProgram *program);
    /// attempts to get property function that and set definition pointer + error handling
    std::shared_ptr<NodeFunctionDefinition> find_property_definition(NodeProgram *program);
	std::shared_ptr<NodeFunctionDefinition> find_constructor_definition(NodeProgram* program);
    /// gets and sets definition ptr or matches builtin func metadata -> throws error if not found when fail set to true
    /// forces to research for definition if force is set to true
    bool bind_definition(NodeProgram* program, bool fail= false, bool force=false);
	std::unique_ptr<NodeAccessChain> to_method_chain() override;

	/// returns true if function call is of kind: Undefined, Builtin or Property
	[[nodiscard]] bool is_builtin_kind() const;
	[[nodiscard]] std::string get_object_name() const;
	[[nodiscard]] std::string get_method_name() const;
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	[[nodiscard]] std::shared_ptr<NodeFunctionDefinition> get_definition() const {
		return definition.lock();
	}
	NodeAST* do_function_call_hoisting(NodeProgram* program);
	NodeAST* do_function_inlining(NodeProgram* program);

	/// builtin functions with side effects and alter the value (variable) put in
	inline static const std::unordered_set<std::string> destructive_functions = {
		"inc", "dec",
	};
	bool is_destructive_builtin_func() const;
	bool check_restricted_environment(NodeCallback *current_callback) const;
	void determine_function_strategy(NodeProgram* program, NodeCallback* current_callback);
	bool is_in_access_chain() const;
	/// Checks if the function call or any of its arguments has side effects
	/// this gets checked by giving a set of free variables that are being modified inside
	/// or checking for builtin functions with side effects (message etc)
	bool has_side_effects(const std::unordered_set<std::string>& free_vars);
};

struct NodeSortSearch final : NodeInstruction {
	std::string name;
	std::unique_ptr<NodeReference> array;
	std::unique_ptr<NodeAST> value;
	std::unique_ptr<NodeAST> from;
	std::unique_ptr<NodeAST> to;
	explicit NodeSortSearch(std::string name, Token tok) : NodeInstruction(NodeType::Search, std::move(tok)), name(std::move(name)) {}
	NodeSortSearch(std::string name, std::unique_ptr<NodeReference> array, std::unique_ptr<NodeAST> value, Token tok)
	: NodeInstruction(NodeType::Search, std::move(tok)), name(std::move(name)), array(std::move(array)), value(std::move(value)) {
		NodeSortSearch::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeSortSearch(const NodeSortSearch& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		array->update_parents(this);
		value->update_parents(this);
		if(from) from->update_parents(this);
		if(to) to->update_parents(this);
	}
	void set_child_parents() override {
		array->parent = this;
		value->parent = this;
		if(from) from->parent = this;
		if(to) to->parent = this;
	}
	std::string get_string() override {
		std::string search = name+"[" + array->get_string();
		search += ", " + value->get_string();
		if(from) {
			search += ", " + from->get_string();
		}
		if(to) {
			search += ", " + to->get_string();
		}
		return search += "]";
	}
	std::string get_token_string() override {
		std::string search = name + "[" + array->get_token_string();
		search += ", " + value->get_token_string();
		if (from) search += ", " + from->get_token_string();
		if (to) search += ", " + to->get_token_string();
		return search + "]";
	}
	void update_token_data(const Token& token) override {
		array->update_token_data(token);
		value->update_token_data(token);
		if(from) from->update_token_data(token);
		if(to) to->update_token_data(token);
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering* get_post_lowering(NodeProgram *program) const override;
	void set_from(std::unique_ptr<NodeAST> new_from) {
		from = std::move(new_from);
		from->parent = this;
	}
	void set_to(std::unique_ptr<NodeAST> new_to) {
		to = std::move(new_to);
		to->parent = this;
	}
};

struct NodeNumElements final : NodeInstruction {
	std::unique_ptr<NodeReference> array;
	std::unique_ptr<NodeAST> dimension;
	explicit NodeNumElements(Token tok) : NodeInstruction(NodeType::NumElements, std::move(tok)) {}
	NodeNumElements(std::unique_ptr<NodeReference> array, std::unique_ptr<NodeAST> dimension, Token tok)
		: NodeInstruction(NodeType::NumElements, std::move(tok)), array(std::move(array)), dimension(std::move(dimension)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeNumElements(const NodeNumElements& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		array->update_parents(this);
		if(dimension) dimension->update_parents(this);
	}
	void set_child_parents() override {
		array->parent = this;
		if(dimension) dimension->parent = this;
	}
	std::string get_string() override {
		std::string num_elements = "num_elements[" + array->get_string();
		if(dimension) {
			num_elements += ", " + dimension->get_string() + "]";
		}
		return num_elements;
	}
	std::string get_token_string() override {
		std::string num_elements = "num_elements[" + array->get_token_string();
		if (dimension) num_elements += ", " + dimension->get_token_string();
		return num_elements + "]";
	}
	void update_token_data(const Token& token) override {
		array->update_token_data(token);
		if(dimension) dimension->update_token_data(token);
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering* get_post_lowering(NodeProgram *program) const override;
	void set_dimension(std::unique_ptr<NodeAST> new_dimension) {
		dimension = std::move(new_dimension);
		dimension->parent = this;
	}
};

struct NodeUseCount final : NodeInstruction {
	std::unique_ptr<NodeReference> ref;
	explicit NodeUseCount(std::unique_ptr<NodeReference> ref)
		: NodeInstruction(NodeType::UseCount, ref->tok), ref(std::move(ref)) {
		NodeUseCount::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeUseCount(const NodeUseCount& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		ref->update_parents(this);
	}
	void set_child_parents() override {
		ref->parent = this;
	}
	std::string get_string() override {
		const std::string use_count = "use_count[" + ref->get_string();
		return use_count + "]";
	}
	std::string get_token_string() override {
		return "use_count[" + ref->get_token_string() + "]";
	}
	void update_token_data(const Token& token) override {
		ref->update_token_data(token);
	}
	void set_ref(std::unique_ptr<NodeReference> new_ref) {
		ref = std::move(new_ref);
		ref->parent = this;
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering* get_post_lowering(NodeProgram *program) const override;
};

struct NodeDelete final : NodeInstruction {
	std::vector<std::unique_ptr<NodeReference>> ptrs;
	explicit NodeDelete(Token tok) : NodeInstruction(NodeType::Delete, std::move(tok)) {}
	NodeDelete(std::vector<std::unique_ptr<NodeReference>> delete_pointer, Token tok)
		: NodeInstruction(NodeType::Delete, std::move(tok)), ptrs(std::move(delete_pointer)) {
		NodeDelete::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeDelete(const NodeDelete& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(const auto &del : ptrs) {
			del->update_parents(this);
		}
	}
	void set_child_parents() override {
		for(auto& del : ptrs) {
			if(del) del->parent = this;
		}
	}
	std::string get_string() override {
		std::string del = "delete ";
		for(const auto &d : ptrs) {
			del += d->get_string() + ", ";
		}
		return del;
	}
	std::string get_token_string() override {
		std::string del = "delete ";
		for (const auto& d : ptrs) del += d->get_token_string() + ", ";
		return del;
	}
	void update_token_data(const Token& token) override {
		for(const auto &del : ptrs) {
			del->update_token_data(token);
		}
	}
	void add_pointer(std::unique_ptr<NodeReference> ptr) {
		ptr->parent = this;
		ptrs.push_back(std::move(ptr));
	}
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;

};

struct NodeSingleDelete final : NodeInstruction {
	std::unique_ptr<NodeReference> ptr;
	std::unique_ptr<NodeAST> num;
	explicit NodeSingleDelete(Token tok) : NodeInstruction(NodeType::SingleDelete, std::move(tok)) {}
	NodeSingleDelete(std::unique_ptr<NodeReference> ptr, std::unique_ptr<NodeAST> num, Token tok)
		: NodeInstruction(NodeType::SingleDelete, std::move(tok)), ptr(std::move(ptr)), num(std::move(num)) {
		NodeSingleDelete::set_child_parents();
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
		if(num) num->update_parents(this);
	}
	void set_child_parents() override {
		ptr->parent = this;
		if(num) num->parent = this;
	}
	std::string get_string() override {
		return "delete " + ptr->get_string();
	}
	std::string get_token_string() override {
		std::string str = "delete " + ptr->get_token_string();
		if (num) str += ", " + num->get_token_string();
		return str;
	}
	void update_token_data(const Token& token) override {
		ptr->update_token_data(token);
		if(num) num->update_token_data(token);
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;

};

/// Node to retain a single pointer
/// only used internally in AST
struct NodeSingleRetain final : NodeInstruction {
	std::unique_ptr<NodeReference> ptr;
	std::unique_ptr<NodeAST> num; // number of times to retain
	explicit NodeSingleRetain(Token tok) : NodeInstruction(NodeType::SingleRetain, std::move(tok)) {}
	NodeSingleRetain(std::unique_ptr<NodeReference> ptr, std::unique_ptr<NodeAST> num, Token tok)
		: NodeInstruction(NodeType::SingleRetain, std::move(tok)), ptr(std::move(ptr)),
		num(std::move(num)) {
		NodeSingleRetain::set_child_parents();
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
	}
	std::string get_string() override {
		return "retain " + ptr->get_string() + ", " + num->get_string();
	}
	std::string get_token_string() override {
		return "retain " + ptr->get_token_string() + ", " + num->get_token_string();
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
struct NodeRetain final : NodeInstruction {
	std::vector<std::unique_ptr<NodeSingleRetain>> ptrs;
	explicit NodeRetain(Token tok) : NodeInstruction(NodeType::Retain, std::move(tok)) {}
	NodeRetain(std::vector<std::unique_ptr<NodeSingleRetain>> ptrs, Token tok)
		: NodeInstruction(NodeType::Retain, std::move(tok)), ptrs(std::move(ptrs)) {
		NodeRetain::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Copy Constructor
	NodeRetain(const NodeRetain& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(const auto & ptr: ptrs) ptr->update_parents(this);
	}
	void set_child_parents() override {
		for(const auto & ptr: ptrs) ptr->parent = this;
	};
	std::string get_string() override {
		std::string output = "delete ";
		for(const auto & ptr: ptrs) output += ptr->get_string() + ", ";
		return output;
	}
	std::string get_token_string() override {
		std::string output = "delete ";
		for (const auto& ptr : ptrs) output += ptr->get_token_string() + ", ";
		return output;
	}
	void update_token_data(const Token& token) override {
		for(const auto & ptr: ptrs) ptr->update_token_data(token);
	}
	void add_single_retain(std::unique_ptr<NodeReference> ref, std::unique_ptr<NodeAST> num) {
		auto retain = std::make_unique<NodeSingleRetain>(std::move(ref), std::move(num), tok);
		retain->parent = this;
		ptrs.push_back(std::move(retain));
	}
};

struct NodeAssignment final : NodeInstruction {
    std::vector<std::unique_ptr<NodeReference>> l_values;
    std::unique_ptr<NodeParamList> r_values;
    explicit NodeAssignment(Token tok) : NodeInstruction(NodeType::Assignment, std::move(tok)) {}
    NodeAssignment(std::vector<std::unique_ptr<NodeReference>> array_variables, std::unique_ptr<NodeParamList> assignees, Token tok)
            : NodeInstruction(NodeType::Assignment, std::move(tok)), l_values(std::move(array_variables)), r_values(std::move(assignees)) {
        NodeAssignment::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
    // Copy Constructor
    NodeAssignment(const NodeAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
		for(const auto& l_val : l_values) l_val->update_parents(this);
		r_values->update_parents(this);
    }
    void set_child_parents() override {
		for(const auto& l_val : l_values) l_val->parent = this;
		if(r_values) r_values->parent = this;
    };
    std::string get_string() override {
		std::string output;
		for(const auto& l_val : l_values) output += l_val->get_string();
        return output + " := " + r_values->get_string();
    }
	std::string get_token_string() override {
		std::string output;
		for (const auto& l_val : l_values) output += l_val->get_token_string();
		return output + " := " + r_values->get_token_string();
	}
    void update_token_data(const Token& token) override {
		for(const auto& l_val : l_values) l_val->update_token_data(token);
		r_values -> update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeSingleAssignment final : NodeInstruction {
    std::unique_ptr<NodeReference> l_value;
    std::unique_ptr<NodeAST> r_value;
	bool has_object = false;
	bool is_parameter_stack = false; // if true, the assignment is a parameter stack assignment and the l_value is mutable
    explicit NodeSingleAssignment(Token tok) : NodeInstruction(NodeType::SingleAssignment, std::move(tok)) {}
    NodeSingleAssignment(std::unique_ptr<NodeReference> arrayVariable, std::unique_ptr<NodeAST> assignee, Token tok)
            : NodeInstruction(NodeType::SingleAssignment, std::move(tok)), l_value(std::move(arrayVariable)), r_value(std::move(assignee)) {
        NodeSingleAssignment::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleAssignment(const NodeSingleAssignment& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        l_value->update_parents(this);
        r_value->update_parents(this);
    }
    void set_child_parents() override {
        if(l_value) l_value->parent = this;
        if(r_value) r_value->parent = this;
    }
    std::string get_string() override {
        return l_value->get_string() + " := " + r_value->get_string();
    }
	std::string get_token_string() override {
		return l_value->get_token_string() + " := " + r_value->get_token_string();
	}
    void update_token_data(const Token& token) override {
        l_value -> update_token_data(token);
        r_value -> update_token_data(token);
    }
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	NodeAST* do_array_normalization(NodeProgram *program);
    void check_for_constant_assignment() const;
};

struct NodeCompoundAssignment final : NodeInstruction {
	std::unique_ptr<NodeReference> l_value;
	std::unique_ptr<NodeAST> r_value;
	token op;
	explicit NodeCompoundAssignment(Token tok) : NodeInstruction(NodeType::CompoundAssignment, std::move(tok)) {}
	NodeCompoundAssignment(std::unique_ptr<NodeReference> arrayVariable, std::unique_ptr<NodeAST> assignee, token op, Token tok)
			: NodeInstruction(NodeType::CompoundAssignment, std::move(tok)), l_value(std::move(arrayVariable)), r_value(std::move(assignee)), op(op) {
		NodeCompoundAssignment::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeCompoundAssignment(const NodeCompoundAssignment& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		l_value->update_parents(this);
		r_value->update_parents(this);
	}
	void set_child_parents() override {
		if(l_value) l_value->parent = this;
		if(r_value) r_value->parent = this;
	}
	std::string get_string() override {
		return l_value->get_string() + " " + ::get_token_string(op) + "= " + r_value->get_string();
	}
	std::string get_token_string() override {
		return l_value->get_token_string() + " " + ::get_token_string(op) + "= " + r_value->get_token_string();
	}
	void update_token_data(const Token& token) override {
		l_value -> update_token_data(token);
		r_value -> update_token_data(token);
	}
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeDeclaration final : NodeInstruction {
    std::vector<std::unique_ptr<NodeDataStructure>> variable;
    std::unique_ptr<NodeParamList> value;
    explicit NodeDeclaration(Token tok) : NodeInstruction(NodeType::Declaration, std::move(tok)) {}
    NodeDeclaration(std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
            : NodeInstruction(NodeType::Declaration, std::move(tok)), variable(std::move(to_be_declared)), value(std::move(assignee)) {
        NodeDeclaration::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
    }
    std::string get_string() override {
        std::string str = "declare ";
        for(const auto & decl : variable) {
            str += decl->get_string() + ", ";
        }
        return str.erase(str.size() - 2) + value->get_string();
    }
	std::string get_token_string() override {
		std::string str = "declare ";
		for (const auto& decl : variable) str += decl->get_token_string() + ", ";
		if (!variable.empty()) str.erase(str.size() - 2);
		return str + value->get_token_string();
	}
    void update_token_data(const Token& token) override {
        for(auto const &decl : variable) {
            decl->update_token_data(token);
        }
        value -> update_token_data(token);
    }
    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeSingleDeclaration final : NodeInstruction {
    std::shared_ptr<NodeDataStructure> variable;
    std::unique_ptr<NodeAST> value = nullptr;
    explicit NodeSingleDeclaration(Token tok) : NodeInstruction(NodeType::SingleDeclaration, std::move(tok)) {}
	NodeSingleDeclaration(std::shared_ptr<NodeDataStructure> variable, std::unique_ptr<NodeAST> value, Token tok)
		: NodeInstruction(NodeType::SingleDeclaration, std::move(tok)),
		  variable(std::move(variable)),
		  value(std::move(value)) {
		NodeSingleDeclaration::set_child_parents();
	}
	NodeSingleDeclaration(std::unique_ptr<NodeDataStructure> variable, Token tok)
		: NodeInstruction(NodeType::SingleDeclaration, std::move(tok)),
		  variable(std::move(variable)) {
		set_child_parents();
	}
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeSingleDeclaration(const NodeSingleDeclaration& other);
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
    }
    std::string get_string() override {
        auto string = variable->get_string();
        if(value)
            string += " := " + value->get_string();
        return string;
    }
	std::string get_token_string() override {
		auto string = variable->get_token_string();
		if (value) string += " := " + value->get_token_string();
		return string;
	}
    void update_token_data(const Token& token) override {
        variable -> update_token_data(token);
        if(value) value -> update_token_data(token);
    }
	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
    ASTLowering* get_lowering(NodeProgram *program) const override;
    /// returns new assign statement with the declared variable and r_value or neutral element. Can optionally take new
    /// variable to make reference of
    [[nodiscard]] std::unique_ptr<NodeSingleAssignment> to_assign_stmt(NodeDataStructure* var=nullptr);
	void set_value(std::unique_ptr<NodeAST> new_value) {
		value = std::move(new_value);
		value->parent = this;
	}
	/// checks if declaration of a constant is properly initialized
	void check_constant_initialization() const {
		if (variable->is_member()) return;
		if (variable->data_type == DataType::Const and !value) {
			auto error = CompileError(ErrorType::VariableError, "", "", tok);
			error.m_message = "Constant variables must be initialized upon declaration.";
			error.exit();
		}
	}
};

struct NodeFunctionParam final : NodeInstruction {
	bool is_pass_by_ref = false;
	std::shared_ptr<NodeDataStructure> variable;
	std::unique_ptr<NodeAST> value = nullptr;
	explicit NodeFunctionParam(Token tok) : NodeInstruction(NodeType::FunctionParam, std::move(tok)) {}
	NodeFunctionParam(std::shared_ptr<NodeDataStructure> variable, std::unique_ptr<NodeAST> assignee, Token tok)
		: NodeInstruction(NodeType::FunctionParam, std::move(tok)),
		variable(std::move(variable)),
		value(std::move(assignee)) {
		NodeFunctionParam::set_child_parents();
	}
	explicit NodeFunctionParam(std::unique_ptr<NodeDataStructure> variable)
		: NodeInstruction(NodeType::FunctionParam, std::move(variable->tok)), variable(std::move(variable)) {
		NodeFunctionParam::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
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
	}
	std::string get_string() override {
		auto string = variable->get_string();
		if(value)
			string += " := " + value->get_string();
		return string;
	}
	std::string get_token_string() override {
		auto string = variable->get_token_string();
		if (value) string += " := " + value->get_token_string();
		return string;
	}
	void update_token_data(const Token& token) override {
		variable -> update_token_data(token);
		if(value) value -> update_token_data(token);
	}
	/// returns index of the parameter in the function header (-1 if not found)
	size_t get_index();
	ASTLowering* get_lowering(NodeProgram *program) const override;
};

struct NodeReturn final : NodeInstruction {
    std::vector<std::unique_ptr<NodeAST>> return_variables{};
	std::weak_ptr<NodeFunctionDefinition> definition;
    explicit NodeReturn(Token tok) : NodeInstruction(NodeType::Return, std::move(tok)) {}
    NodeReturn(std::vector<std::unique_ptr<NodeAST>> return_variables, Token tok)
            : NodeInstruction(NodeType::Return, std::move(tok)), return_variables(std::move(return_variables)) {
        NodeReturn::set_child_parents();
    }
	// Variadischer Template-Konstruktor
	template<typename... Params>
	explicit NodeReturn(Token tok, Params&&... params) : NodeInstruction(NodeType::Return, std::move(tok)) {
		(return_variables.push_back(std::move(params)), ...);
		NodeReturn::set_child_parents();
	}
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    // Copy Constructor
    NodeReturn(const NodeReturn& other);
    // Clone Method
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for(const auto &ret : return_variables) {
            ret->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& ret : return_variables) {
            if(ret) ret->parent = this;
        }
    }
    void update_token_data(const Token& token) override {
        for(const auto &ret : return_variables) {
            ret->update_token_data(token);
        }
    }
	std::string get_string() override {
		std::string str = "return ";
		for(const auto &ret : return_variables) {
			str += ret->get_string() + ", ";
		}
		return str.erase(str.size() - 2);
	}
	std::string get_token_string() override {
		std::string str = "return ";
		for (const auto& ret : return_variables) str += ret->get_token_string() + ", ";
		return return_variables.empty() ? str : str.erase(str.size() - 2);
	}
	void add_return_param(std::unique_ptr<NodeAST> param) {
		param->parent = this;
		return_variables.push_back(std::move(param));
	}
	[[nodiscard]] std::shared_ptr<NodeFunctionDefinition> get_definition() const {
		return definition.lock();
	}
};

struct NodeSingleReturn final : NodeInstruction {
	std::unique_ptr<NodeAST> return_variable;
	NodeFunctionDefinition* definition = nullptr;
	explicit NodeSingleReturn(Token tok) : NodeInstruction(NodeType::SingleReturn, std::move(tok)) {}
	NodeSingleReturn(std::unique_ptr<NodeAST> return_variable, Token tok)
		: NodeInstruction(NodeType::SingleReturn, std::move(tok)), return_variable(std::move(return_variable)) {
		NodeSingleReturn::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Copy Constructor
	NodeSingleReturn(const NodeSingleReturn& other);
	// Clone Method
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return "return " + return_variable->get_string();
	}
	std::string get_token_string() override {
		return "return " + return_variable->get_token_string();
	}
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		return_variable->update_parents(this);
	}
	void set_child_parents() override {
		return_variable->parent = this;
	}
	void update_token_data(const Token& token) override {
		return_variable->update_token_data(token);
	}
};

struct NodeBlock final : NodeInstruction {
    bool scope = false;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    explicit NodeBlock(Token tok, bool scope=false) : NodeInstruction(NodeType::Block, std::move(tok)), scope(scope) {}
    NodeBlock(std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
            : NodeInstruction(NodeType::Block, std::move(tok)), statements(std::move(statements)) {
        NodeBlock::set_child_parents();
    }
	// Variadischer Template-Konstruktor
	template<typename... Stmts>
	explicit NodeBlock(Token tok, Stmts&&... stmts) : NodeInstruction(NodeType::Block, std::move(tok)) {
		(statements.push_back(std::forward<Stmts>(stmts)), ...);
		NodeBlock::set_child_parents();
	}
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeBlock(const NodeBlock& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        for (const auto & stmt : statements) {
            stmt->update_parents(this);
        }
    }
    void set_child_parents() override {
        for(auto& stmt : statements) {
            if(stmt) stmt->parent = this;
        }
    }
    std::string get_string() override {
        std::string str;
        for(const auto & stmt : statements) {
            str += stmt->get_string();
        }
        return str;
    }
	std::string get_token_string() override {
		std::string str;
		for (const auto& stmt : statements) str += stmt->get_token_string();
		return str;
	}
    void update_token_data(const Token& token) override {
        for(const auto &stmt : statements) {
            stmt->update_token_data(token);
        }
    }
    [[nodiscard]] bool empty() const {
	    return statements.empty();
    }
	size_t size() const {
	    return statements.size();
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
		return add_stmt(std::move(node_stmt));
	}
	NodeStatement* add_as_single_retain(std::unique_ptr<NodeReference> ref, std::unique_ptr<NodeAST> num) {
		auto retain = std::make_unique<NodeSingleRetain>(std::move(ref), std::move(num), tok);
		return add_as_stmt(std::move(retain));
	}
	NodeStatement* add_as_single_delete(std::unique_ptr<NodeReference> ref, std::unique_ptr<NodeAST> num=nullptr) {
		auto del = std::make_unique<NodeSingleDelete>(std::move(ref), std::move(num), tok);
		return add_as_stmt(std::move(del));
	}
    /// puts nested statement list in current one
    void flatten(bool force=false);
	/// returns true if the block is a scope block and sets node.scope. returns false if
	/// parent is namespace or struct
	bool determine_scope();
    NodeBlock* wrap_in_loop_nest(std::vector<std::shared_ptr<NodeDataStructure>> iterators,
                                 std::vector<std::unique_ptr<NodeAST>> lower_bounds,
                                 std::vector<std::unique_ptr<NodeAST>> upper_bounds, bool declare=true);
	NodeBlock* wrap_in_loop(const std::shared_ptr<NodeDataStructure>& iterator, std::unique_ptr<NodeAST> lower_bound, std::unique_ptr<NodeAST> upper_bound, bool declare=true);
	[[nodiscard]] std::unique_ptr<NodeAST>& get_statement(const size_t index) const {
		return statements[index]->statement;
	}
	[[nodiscard]] std::unique_ptr<NodeAST>& get_last_statement() const {
		return statements.back()->statement;
	}
	std::unique_ptr<NodeAST>& get_first_statement() const {
		return statements.front()->statement;
	}
};

struct NodeFamily final : NodeInstruction {
    std::string prefix;
    std::unique_ptr<NodeBlock> members;
    explicit NodeFamily(Token tok) : NodeInstruction(NodeType::Family, std::move(tok)) {}
    NodeFamily(std::string prefix, std::unique_ptr<NodeBlock> members, Token tok)
            : NodeInstruction(NodeType::Family, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)) {
        NodeFamily::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		return "family " + prefix + " " + members->get_token_string();
	}
    void update_token_data(const Token& token) override {
        members->update_token_data(token);
    }

    [[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
};

struct NodeIf final : NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBlock> if_body;
    std::unique_ptr<NodeBlock> else_body;
    explicit NodeIf(Token tok) : NodeInstruction(NodeType::If, std::move(tok)) {
	    set_else_body(std::make_unique<NodeBlock>(tok, true));
    	set_if_body(std::make_unique<NodeBlock>(tok, true));
    }
    NodeIf(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBlock> statements, std::unique_ptr<NodeBlock> elseStatements, Token tok)
            : NodeInstruction(NodeType::If, std::move(tok)), condition(std::move(condition)), if_body(std::move(statements)), else_body(std::move(elseStatements)) {
            NodeIf::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		std::string str = "if " + condition->get_token_string();
		if (if_body) str += " " + if_body->get_token_string();
		if (else_body && !else_body->empty()) str += " else " + else_body->get_token_string();
		return str;
	}
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        if_body->update_token_data(token);
        else_body->update_token_data(token);
    }
	void set_if_body(std::unique_ptr<NodeBlock> new_body) {
		if_body = std::move(new_body);
		if_body->parent = this;
	}
	void set_else_body(std::unique_ptr<NodeBlock> new_body) {
    	else_body = std::move(new_body);
    	else_body->parent = this;
    }
	void set_condition(std::unique_ptr<NodeAST> new_condition) {
		condition = std::move(new_condition);
		condition->parent = this;
	}
	NodeAST* do_short_circuit_transform(NodeProgram* program);
};

struct NodeTernary final : NodeInstruction {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeAST> if_branch;
    std::unique_ptr<NodeAST> else_branch;
    explicit NodeTernary(Token tok) : NodeInstruction(NodeType::Ternary, std::move(tok)) {}
    NodeTernary(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeAST> if_branch, std::unique_ptr<NodeAST> else_branch, Token tok)
            : NodeInstruction(NodeType::Ternary, std::move(tok)), condition(std::move(condition)), if_branch(std::move(if_branch)), else_branch(std::move(else_branch)) {
            NodeTernary::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeTernary(const NodeTernary& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        condition->update_parents(this);
        if_branch->update_parents(this);
        else_branch->update_parents(this);
    }
    void set_child_parents() override {
        condition->parent = this;
        if_branch->parent = this;
        else_branch->parent = this;
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		return condition->get_token_string() + " ? " + if_branch->get_token_string() + " : " + else_branch->get_token_string();
	}
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        if_branch->update_token_data(token);
        else_branch->update_token_data(token);
    }

};

struct NodeLoop : NodeInstruction {
	bool is_linear = false;
	explicit NodeLoop(const NodeType node_type, Token tok) : NodeInstruction(node_type, std::move(tok)) {};
	~NodeLoop() override = default;
	NodeLoop(const NodeLoop& other) : NodeInstruction(other), is_linear(other.is_linear) {};
	virtual bool determine_linear() {return false;};
	virtual std::unique_ptr<NodeRange> determine_loop_range() {return nullptr;};
	virtual std::unique_ptr<NodeAST> get_num_iterations() {return nullptr;};
};

struct NodeFor final : NodeLoop {
    std::unique_ptr<NodeSingleAssignment> iterator;
    token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::unique_ptr<NodeAST> step = nullptr;
    std::unique_ptr<NodeBlock> body;
    explicit NodeFor(Token tok) : NodeLoop(NodeType::For, std::move(tok)) {}
    NodeFor(std::unique_ptr<NodeSingleAssignment> iterator, token to, std::unique_ptr<NodeAST> iterator_end, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeLoop(NodeType::For, std::move(tok)), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), body(std::move(statements)) {
        set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
	std::string get_token_string() override {
		std::string str = "for ";
		if (iterator) str += iterator->get_token_string();
		str += " " + tok.val + " ";
		if (iterator_end) str += iterator_end->get_token_string();
		if (step) str += " step " + step->get_token_string();
		if (body) str += " " + body->get_token_string();
		return str;
	}
    void update_token_data(const Token& token) override {
        iterator -> update_token_data(token);
        iterator_end -> update_token_data(token);
        if(step) step ->update_token_data(token);
        body->update_token_data(token);
    }
//    ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	ASTLowering* get_lowering(NodeProgram *program) const override;
	bool determine_linear() override;
	std::unique_ptr<NodeRange> determine_loop_range() override;
	std::unique_ptr<NodeAST> get_num_iterations() override;

};

struct NodeForEach final : NodeLoop {
	std::unique_ptr<NodeFunctionParam> key;
    std::unique_ptr<NodeFunctionParam> value;
    std::unique_ptr<NodeAST> range;
    std::unique_ptr<NodeBlock> body;
    explicit NodeForEach(Token tok) : NodeLoop(NodeType::ForEach, std::move(tok)) {}
    NodeForEach(std::unique_ptr<NodeFunctionParam> key, std::unique_ptr<NodeAST> range, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeLoop(NodeType::ForEach, std::move(tok)), key(std::move(key)), range(std::move(range)), body(std::move(statements)) {
        set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    NodeForEach(const NodeForEach& other);
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
		if(key) key->update_parents(this);
		if(value) value->update_parents(this);
        range->update_parents(this);
        body->update_parents(this);
    }
    void set_child_parents() override {
		if(key) key->parent = this;
		if(value) value->parent = this;
        range->parent = this;
        body->parent = this;
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		std::string str = "for ";
		if (key) str += key->get_token_string();
		if (value) str += ", " + value->get_token_string();
		str += " in " + range->get_token_string();
		if (body) str += " " + body->get_token_string();
		return str;
	}
    void update_token_data(const Token& token) override {
		if(key) key->update_token_data(token);
		if(value) value->update_token_data(token);
        range -> update_token_data(token);
        body->update_token_data(token);
    }
//    ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	ASTLowering* get_lowering(NodeProgram *program) const override;
	bool determine_linear() override;
	std::unique_ptr<NodeRange> determine_loop_range() override;
	std::unique_ptr<NodeAST> get_num_iterations() override;

};

// for key, val in pairs(array)
struct NodePairs final : NodeInstruction {
	std::unique_ptr<NodeAST> range;
	explicit NodePairs(std::unique_ptr<NodeAST> range) : NodeInstruction(NodeType::Pairs, range->tok),
		range(std::move(range)) {
		NodePairs::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	NodePairs(const NodePairs& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		range->update_parents(this);
	}
	void set_child_parents() override {
		range->parent = this;
	}
	std::string get_string() override { return "pairs"; }
	std::string get_token_string() override { return "pairs(" + range->get_token_string() + ")"; }
	void update_token_data(const Token& token) override {
		range -> update_token_data(token);
	}
	bool check_environment() const;
};

// for key, val in range(10)
struct NodeRange final : NodeInstruction {
	std::unique_ptr<NodeAST> start;
	std::unique_ptr<NodeAST> stop;
	std::unique_ptr<NodeAST> step;
	explicit NodeRange(std::unique_ptr<NodeAST> start, std::unique_ptr<NodeAST> stop, std::unique_ptr<NodeAST> step, Token tok)
	: NodeInstruction(NodeType::Range, std::move(tok)), start(std::move(start)), stop(std::move(stop)), step(std::move(step)) {
		NodeRange::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	NodeRange(const NodeRange& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(start) start->update_parents(this);
		stop->update_parents(this);
		if(step) step->update_parents(this);
	}
	void set_child_parents() override {
		if(start) start->parent = this;
		stop->parent = this;
		if(step) step->parent = this;
	}
	std::string get_string() override { return "range"; }
	std::string get_token_string() override {
		std::string str = "range(";
		if (start) str += start->get_token_string() + ", ";
		str += stop->get_token_string();
		if (step) str += ", " + step->get_token_string();
		return str + ")";
	}
	void update_token_data(const Token& token) override {
		if(start) start->update_token_data(token);
		stop->update_token_data(token);
		if(step) step->update_token_data(token);
	}
	[[nodiscard]] bool all_literals() const {
		bool all_literals = true;
		all_literals &= start->cast<NodeInt>() or start->cast<NodeReal>();
		all_literals &= stop->cast<NodeInt>() or stop->cast<NodeReal>();
		all_literals &= step->cast<NodeInt>() or step->cast<NodeReal>();
		return all_literals;
	}
	std::unique_ptr<NodeAST> get_num_iterations();
	std::unique_ptr<NodeInitializerList> to_initializer_list() const;
	bool check_environment() const;
	ASTLowering* get_lowering(NodeProgram *program) const override;

};

struct NodeWhile final : NodeLoop {
    std::unique_ptr<NodeAST> condition;
    std::unique_ptr<NodeBlock> body;
    explicit NodeWhile(Token tok) : NodeLoop(NodeType::While, std::move(tok)) {}
    NodeWhile(std::unique_ptr<NodeAST> condition, std::unique_ptr<NodeBlock> statements, Token tok)
            : NodeLoop(NodeType::While, std::move(tok)), condition(std::move(condition)), body(std::move(statements)) {
        NodeWhile::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
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
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		return "while " + condition->get_token_string() + " " + body->get_token_string();
	}
    void update_token_data(const Token& token) override {
        condition -> update_token_data(token);
        body->update_token_data(token);
    }
	ASTLowering* get_lowering(NodeProgram *program) const override;
	void set_condition(std::unique_ptr<NodeBinaryExpr> condition) {
		condition->parent = this;
		this->condition = std::move(condition);
	}
};

struct NodeSelect final : NodeInstruction {
    std::unique_ptr<NodeAST> expression;
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cases;
    explicit NodeSelect(Token tok) : NodeInstruction(NodeType::Select, std::move(tok)) {}
    NodeSelect(std::unique_ptr<NodeAST> expression, std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cases, Token tok)
            : NodeInstruction(NodeType::Select, std::move(tok)), expression(std::move(expression)), cases(std::move(cases)) {
        NodeSelect::set_child_parents();
    }
    NodeAST * accept(ASTVisitor &visitor) override;
    NodeSelect(const NodeSelect& other);
    NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
    void update_parents(NodeAST* new_parent) override {
        parent = new_parent;
        expression->update_parents(this);
        for (auto & pair : cases) {
            for(const auto &stmt : pair.first) {
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
    }
    std::string get_string() override { return ""; }
	std::string get_token_string() override {
		std::string str = "select " + expression->get_token_string();
		for (const auto& pair : cases) {
			str += " case ";
			for (const auto& stmt : pair.first) str += stmt->get_token_string() + ", ";
			str += pair.second->get_token_string();
		}
		return str;
	}
    void update_token_data(const Token& token) override {
        expression -> update_token_data(token);
        for (auto & pair : cases) {
            for(const auto &stmt : pair.first) {
                stmt->update_token_data(token);
            }
            pair.second->update_token_data(token);
        }
    }
};

struct NodeBreak final : NodeInstruction {
	explicit NodeBreak(Token tok) : NodeInstruction(NodeType::SingleDeclaration, std::move(tok)) {}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeBreak(const NodeBreak& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
	}
	std::string get_string() override {
		return "break";
	}
	std::string get_token_string() override {
		return "break";
	}
	NodeWhile* get_nearest_loop() const {
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

struct NodeNamespace final : NodeInstruction {
	std::string prefix;
	std::unique_ptr<NodeBlock> members;
	std::vector<std::shared_ptr<NodeFunctionDefinition>> function_definitions{};

	explicit NodeNamespace(Token tok) : NodeInstruction(NodeType::Namespace, std::move(tok)) {}
	NodeNamespace(std::string prefix, std::unique_ptr<NodeBlock> members, std::vector<std::shared_ptr<NodeFunctionDefinition>> funcs, Token tok)
			: NodeInstruction(NodeType::Namespace, std::move(tok)), prefix(std::move(prefix)), members(std::move(members)),
			function_definitions(std::move(funcs)) {
		NodeNamespace::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeNamespace(const NodeNamespace& other);
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		members->update_parents(this);
		for(const auto& func_def : function_definitions) {
			func_def->update_parents(this);
		}
	}
	void set_child_parents() override {
		members->parent = this;
		for(const auto& func_def : function_definitions) {
			func_def->parent = this;
		}
	}
	std::string get_string() override { return ""; }
	std::string get_token_string() override {
		std::string str = "namespace " + prefix;
		if (members) str += " " + members->get_token_string();
		for (const auto& func_def : function_definitions) str += " " + func_def->get_token_string();
		return str;
	}
	void update_token_data(const Token& token) override {
		members->update_token_data(token);
		for(const auto& func_def : function_definitions) {
			func_def->update_token_data(token);
		}
	}

	[[nodiscard]] ASTDesugaring *get_desugaring(NodeProgram *program) const override;
	void inline_namespace(NodeProgram* program);
};
