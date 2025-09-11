//
// Created by Mathias Vatter on 25.04.24.
//

#pragma once

#include <functional>
#include "AST.h"
#include "../TypeRegistry.h"

struct NodeNumElements;

struct NodeVariableRef final : NodeReference {
	NodeVariableRef(std::string name, const Token& tok, DataType data_type=DataType::Mutable)
		: NodeReference(std::move(name), NodeType::VariableRef, tok, data_type) {
		set_range(tok);
	}
	NodeVariableRef(std::string name, Type* ty, const Token& tok, DataType data_type=DataType::Mutable)
		: NodeReference(std::move(name), NodeType::VariableRef, tok, data_type) {
		set_range(tok);
		this->ty = ty;
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeVariableRef(const NodeVariableRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	std::string get_string() override {
		return name;
	}
	std::unique_ptr<NodeArrayRef> to_array_ref(std::unique_ptr<NodeAST> index) override;
	/// this_list.next.next
	std::unique_ptr<NodeAccessChain> to_method_chain() override;
	std::unique_ptr<NodePointerRef> to_pointer_ref() override;

	std::unique_ptr<NodeNumElements> transform_ndarray_constant();
	std::unique_ptr<NodeNumElements> transform_array_constant();
	std::unique_ptr<NodeReference> expand_dimension(std::unique_ptr<NodeAST> new_index) override;
//	ASTLowering* get_lowering(NodeProgram *program) const override;
	NodeAST* try_constant_value_replace();

};

struct NodeCompositeRef : NodeReference {
	// Konstruktor
	NodeCompositeRef(std::string name, NodeType node_type, Token tok, DataType data_type = DataType::Mutable)
		: NodeReference(std::move(name), node_type, std::move(tok), data_type) {}
	// Kopierkonstruktor
	NodeCompositeRef(const NodeCompositeRef& other) : NodeReference(other) {}
	// Standardmethoden für gemeinsame Funktionalitäten
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
	}
	void set_child_parents() override = 0;  // Wird in den abgeleiteten Klassen implementiert
	/// returns the raw version of ndarray with an index
	virtual std::unique_ptr<NodeArrayRef> get_indexed_raw_ref(std::unique_ptr<NodeAST> new_index) = 0;
	[[nodiscard]] virtual int num_wildcards() const = 0;
	/// returns the most inner body of desugared for loop over array ref depending on wildcards
	/// if no wildcards are present -> iterates over whole ndarray
	virtual NodeBlock* iterate_over(std::unique_ptr<NodeBlock>& body, NodeProgram* program) = 0;
	virtual void remove_index() = 0;
	/// returns true if array reference index is not nullptr
	virtual bool has_index() const {
		return false;
	}
	virtual NodeAST* get_index(size_t i) const = 0;
	virtual std::unique_ptr<NodeAST> take_index(size_t i) = 0;

};

struct NodeArrayRef final : NodeCompositeRef {
	std::unique_ptr<NodeAST> index;
	NodeArrayRef(std::string name, std::unique_ptr<NodeAST> index, Token tok, DataType data_type=DataType::Mutable)
		: NodeCompositeRef(std::move(name), NodeType::ArrayRef, std::move(tok), data_type), index(std::move(index)) {
		set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeArrayRef(const NodeArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(index) index ->update_parents(this);
	}
	void set_child_parents() override {
		if(index) index ->parent = this;
	}
	std::string get_string() override {
		return name;
	}
//    ASTLowering* get_lowering(NodeProgram *program) const override;
	std::unique_ptr<NodeNDArrayRef> to_ndarray_ref() override;
	/// this_list.next.value[6]
	std::unique_ptr<NodeAccessChain> to_method_chain() override;
	std::unique_ptr<NodeAST> get_size() override;
	/// check if array ref is <list_ref>.size[] array
	bool is_list_sizes() const;
	std::unique_ptr<NodeReference> expand_dimension(std::unique_ptr<NodeAST> new_index) override;
	void set_index(std::unique_ptr<NodeAST> new_index) {
		index = std::move(new_index);
		index->parent = this;
	}
	std::unique_ptr<NodeArrayRef> get_indexed_raw_ref(std::unique_ptr<NodeAST> new_index) override {
		auto new_ref = clone_as<NodeArrayRef>(this);
		new_ref->set_index(std::move(new_index));
		return new_ref;
	}
	[[nodiscard]] int num_wildcards() const override {
		if(index and index->cast<NodeWildcard>()) {
			return 1;
		}
		if(!index) return 1;
		return 0;
	}
	NodeBlock* iterate_over(std::unique_ptr<NodeBlock>& body, NodeProgram* program) override;
	void remove_index() override {
		index = nullptr;
	}
	bool has_index() const override {
		return index != nullptr;
	}
	NodeAST* get_index(const size_t i) const override {
		if(index) return index.get();
		auto error = CompileError(ErrorType::InternalError, "", "", tok);
		error.m_message = "ArrayRef has no index: " + name + ".";
		error.exit();
		return nullptr;
	}
	std::unique_ptr<NodeAST> take_index(size_t i) override {
		return std::move(index);
	}

};

struct NodeNDArrayRef final : NodeCompositeRef {
	std::unique_ptr<NodeParamList> indexes = nullptr;
    std::unique_ptr<NodeParamList> sizes = nullptr;
	NodeNDArrayRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok, DataType data_type=DataType::Mutable)
		: NodeCompositeRef(std::move(name), NodeType::NDArrayRef, std::move(tok), data_type), indexes(std::move(indexes)) {
		NodeNDArrayRef::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeNDArrayRef(const NodeNDArrayRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(indexes) indexes ->update_parents(this);
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	};
	std::string get_string() override {
		return name;
	}
//    ASTLowering* get_lowering(NodeProgram *program) const override;
	ASTLowering *get_data_lowering(NodeProgram *program) const override;

	std::unique_ptr<NodeAST> get_size() override;
	std::unique_ptr<NodeAST> get_size(std::unique_ptr<NodeAST> dim);
	void set_sizes(std::unique_ptr<NodeParamList> new_sizes) {
		sizes = std::move(new_sizes);
		sizes->parent = this;
	}
	void set_indexes(std::unique_ptr<NodeParamList> new_indexes) {
		indexes = std::move(new_indexes);
		indexes->parent = this;
	}
	std::unique_ptr<NodeArrayRef> to_array_ref(std::unique_ptr<NodeAST> index) override;
	/// this_list.next.value[6,4]
	std::unique_ptr<NodeAccessChain> to_method_chain() override;

	[[nodiscard]] int num_wildcards() const override;
	/// clones sizes list from declaration if it is a NDArray
	bool determine_sizes();
	[[nodiscard]] CompileError throw_missing_indexes_error() const {
		auto compile_error = CompileError(ErrorType::SyntaxError, "","", tok);
		compile_error.m_message = "NDArray reference requires indexes in this context: " + tok.val + ".";
		compile_error.m_expected = "Valid indexes";
		return compile_error;
	}
	[[nodiscard]] CompileError throw_missing_sizes_error() const {
		auto compile_error = CompileError(ErrorType::InternalError, "","", tok);
		compile_error.m_message = "NDArray reference has unknown sizes: " + tok.val + ".";
		compile_error.m_expected = "Valid sizes";
		return compile_error;
	}
	/// adds wildcard indexes to ndarray reference if there are no indexes present
	/// depends on the size -> size has to be known beforehand
	bool add_wildcards();

	std::unique_ptr<NodeReference> expand_dimension(std::unique_ptr<NodeAST> new_index) override;
	std::unique_ptr<NodeArrayRef> get_indexed_raw_ref(std::unique_ptr<NodeAST> new_index) override {
		auto array_ref = this->to_array_ref(std::move(new_index));
		return array_ref;
	}
	/// Checks number of wildcards and wildcard position, returns position of wildcard and checks if they are
	/// contiguous and right aligned (if more than one wildcard) ndarray[*, 2] is allowed
	/// ndarray[2, *] := (0) -> function returns position of *, returns 2:2
	/// ndarray[2, *, *] := (0) -> returns 2:3
	[[nodiscard]] std::pair<int, int> get_wildcard_dimensions() const;
	/// replaces next wildcard with given index
	void replace_next_wildcard_with_index(std::unique_ptr<NodeInt> new_index) const;
	NodeBlock* iterate_over(std::unique_ptr<NodeBlock>& body, NodeProgram* program) override;
	void remove_index() override {
		indexes = nullptr;
	}
	bool has_index() const override {
		return indexes != nullptr;
	}
	NodeAST* get_index(const size_t i) const override {
		return indexes->params[i].get();
	}
	std::unique_ptr<NodeAST> take_index(const size_t i) override {
		return std::move(indexes->params[i]);
	}
};

struct NodeFunctionHeaderRef final : NodeReference {
	bool has_forced_parenth = false;
	std::unique_ptr<NodeParamList> args;
	NodeFunctionHeaderRef(std::string name, Token tok) : NodeReference(std::move(name), NodeType::FunctionHeaderRef, std::move(tok), DataType::Mutable) {
		set_args(std::make_unique<NodeParamList>(tok));
	}
	NodeFunctionHeaderRef(std::string name, std::unique_ptr<NodeParamList> args, Token tok, DataType data_type=DataType::Mutable) :
	NodeReference(std::move(name), NodeType::FunctionHeaderRef, std::move(tok), data_type), args(std::move(args)) {
		set_child_parents();
	}
	~NodeFunctionHeaderRef() override = default;
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeFunctionHeaderRef(const NodeFunctionHeaderRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override;
	std::string get_string() override {
		return name + "(" + (args ? args->get_string() : "") + ")";
	}
	void set_child_parents() override;
	[[nodiscard]] int get_num_args() const;
	[[nodiscard]] bool has_no_args() const;
	[[nodiscard]] std::unique_ptr<NodeAST>& get_arg(int i) const;
	void set_arg(const int i, std::unique_ptr<NodeAST> arg) const {
		args->set_param(i, std::move(arg));
//		if(i >= get_num_args()) {
//			auto error = CompileError(ErrorType::InternalError, "Index out of bounds", "", tok);
//			error.exit();
//		}
//		args->params[i] = std::move(arg);
//		args->params[i]->parent = args.get();
	}
	void prepend_arg(std::unique_ptr<NodeAST> arg) const;
	void add_arg(std::unique_ptr<NodeAST> arg) const;
	void set_args(std::unique_ptr<NodeParamList> new_args);
	Type* create_function_type(Type* return_type = TypeRegistry::Unknown) {
		std::vector<Type*> func_arg_types;
		for(const auto &param : args->params) func_arg_types.push_back(param->ty);
		ty = TypeRegistry::add_function_type(func_arg_types, return_type);
		return ty;
	}
};

struct NodeListRef final : NodeReference {
	NodeParamList* sizes = nullptr; // param list of size of the lists in the list
	NodeParamList* pos = nullptr; // param list of positions in the list
	std::unique_ptr<NodeParamList> indexes;
	NodeListRef(std::string name, std::unique_ptr<NodeParamList> indexes, Token tok, DataType data_type=DataType::Mutable)
		: NodeReference(std::move(name), NodeType::ListRef, std::move(tok), data_type), indexes(std::move(indexes)) {
		NodeListRef::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeListRef(const NodeListRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		if(indexes) indexes->update_parents(this);
	}
	std::string get_string() override {
		return name;
	}
	void set_child_parents() override {
		if(indexes) indexes->parent = this;
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;

};

struct NodePointerRef : NodeReference {
	NodePointerRef(std::string name, Token tok, DataType data_type=DataType::Mutable)
		: NodeReference(std::move(name), NodeType::PointerRef, std::move(tok), data_type) {
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodePointerRef(const NodePointerRef& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;

	std::unique_ptr<NodeArrayRef> to_array_ref(std::unique_ptr<NodeAST> index) override;
	std::unique_ptr<NodeVariableRef> to_variable_ref() override;

	ASTLowering* get_lowering(NodeProgram *program) const override;

	std::unique_ptr<NodeReference> expand_dimension(std::unique_ptr<NodeAST> new_index) override;
};

struct NodeNil final : NodePointerRef {
	explicit NodeNil(Token tok) : NodePointerRef(std::move("nil"), std::move(tok)) {node_type = NodeType::Nil;}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeNil(const NodeNil& other) : NodePointerRef(other) {}
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	[[nodiscard]] ASTLowering *get_lowering(NodeProgram *program) const override;
};

struct NodeAccessChain final : NodeReference {
	std::vector<std::unique_ptr<NodeAST>> chain;
	std::vector<Type*> types;
	NodeAccessChain(std::vector<std::unique_ptr<NodeAST>> method_chain, Token tok, DataType data_type=DataType::Mutable)
		: NodeReference("", NodeType::AccessChain, std::move(tok), data_type), chain(std::move(method_chain)) {
		NodeAccessChain::set_child_parents();
	}
	explicit NodeAccessChain(Token tok)
		: NodeReference("", NodeType::AccessChain, std::move(tok), DataType::Mutable) {}
	// Variadischer Template-Konstruktor
	template<typename... Member>
	explicit NodeAccessChain(Token tok, Member&&... member) : NodeReference("", NodeType::AccessChain, std::move(tok), DataType::Mutable) {
		(add_method(std::move(member)), ...);
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	// Kopierkonstruktor
	NodeAccessChain(const NodeAccessChain& other);
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		for(const auto & c: chain) c->update_parents(this);
	}
	void set_child_parents() override {
		for(const auto & c: chain) c->parent = this;
	}
	void add_method(std::unique_ptr<NodeAST> m) {
		m->parent = this;
		chain.push_back(std::move(m));
	}
	void update_types() {
		for(const auto& c: chain) {
			types.push_back(c->ty);
		}
	}
	std::string get_string() override {
		std::string str = "";
		for(const auto & c: chain) {
			str += c->get_string() + ".";
		}
		return str.erase(str.size() - 1);
	}
	void flatten() {
		std::vector<std::unique_ptr<NodeAST>> flat_list;
		// Rekursive Funktion, um die Parameterliste abzuflachen
		std::function<void(std::vector<std::unique_ptr<NodeAST>>)> flatten = [&](std::vector<std::unique_ptr<NodeAST>> current_node) {
		  for (auto& ptr : current_node) {
			  if (ptr->get_node_type() == NodeType::AccessChain) {
				  flatten(std::move(static_cast<NodeAccessChain*>(ptr.get())->chain));
			  } else {
				  // Wenn es kein NodeParamList ist, fügen wir es direkt zur Liste hinzu
				  ptr->parent = this;
				  flat_list.push_back(std::move(ptr));
			  }
		  }
		};
		flatten(std::move(chain));
		chain = std::move(flat_list);
	}

	/// returns variable ref if access chain is incorrectly detected array/list/ndarray size constant
//	std::unique_ptr<NodeVariableRef> is_size_constant();

	ASTLowering* get_lowering(NodeProgram *program) const override;

};

struct NodeGetControl final : NodeReference {
	std::unique_ptr<NodeAST> ui_id; //array or variable
	std::string control_param;
	NodeGetControl(std::unique_ptr<NodeAST> ui_id, std::string controlParam, Token tok)
		: NodeReference(controlParam, NodeType::GetControl, std::move(tok), DataType::Mutable), ui_id(std::move(ui_id)), control_param(std::move(controlParam)) {
		NodeGetControl::set_child_parents();
	}
	NodeAST * accept(ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Kopierkonstruktor
	NodeGetControl(const NodeGetControl& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		ui_id->update_parents(this);
	}
	void set_child_parents() override {
		if(ui_id) ui_id->parent = this;
	}
	std::string get_string() override {
		return ui_id->get_string() + " -> " + control_param;
	}
	void update_token_data(const Token& token) override {
		ui_id -> update_token_data(token);
	}
	ASTLowering* get_lowering(NodeProgram *program) const override;

//	std::unique_ptr<NodeReference> get_full_control_param(DefinitionProvider* def_provider);
	Type* get_control_type() const;
};

struct NodeSetControl final : NodeReference {
	std::unique_ptr<NodeAST> ui_id; //array or variable
	std::string control_param;
	std::unique_ptr<NodeAST> value;
	NodeSetControl(std::unique_ptr<NodeAST> ui_id, std::string controlParam, std::unique_ptr<NodeAST> value, Token tok)
		: NodeReference(controlParam, NodeType::SetControl, std::move(tok), DataType::Mutable),
		ui_id(std::move(ui_id)), control_param(std::move(controlParam)), value(std::move(value)) {
		set_child_parents();
	}
	NodeAST * accept(struct ASTVisitor &visitor) override;
	NodeAST * replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
	// Kopierkonstruktor
	NodeSetControl(const NodeSetControl& other);
	// Clone Methode
	[[nodiscard]] std::unique_ptr<NodeAST> clone() const override;
	void update_parents(NodeAST* new_parent) override {
		parent = new_parent;
		ui_id->update_parents(this);
		value->update_parents(this);
	}
	void set_child_parents() override {
		ui_id->parent = this;
		value->parent = this;
	};
	std::string get_string() override {
		return ui_id->get_string() + " -> " + control_param + " := " + value->get_string();
	}
	void update_token_data(const Token& token) override {
		ui_id -> update_token_data(token);
		value -> update_token_data(token);
	}
	ASTLowering* get_lowering(struct NodeProgram *program) const override;
};

