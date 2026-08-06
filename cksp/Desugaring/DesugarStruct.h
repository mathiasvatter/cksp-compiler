//
// Created by Mathias Vatter on 16.06.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * @brief Desugaring of struct objects. Applies namespaces for easier reference/declaration finding.
 * Additionally, checks methods for self keyword, removes it, replaces 'self.' with the struct prefix.
 * Adds __init__ function if not already defined.
 * desugaring example:
 * struct List
 *  declare value: int
 *  declare next: List := nil
 *
 *  function __init__(self, value: int, next: List)
 *      self.value := value
 *      self.next := next
 *  end function
 * end struct
 * post desugaring:
 * struct List
 *  declare self: List
 *	declare List::value: int
 *	declare List::next: List := nil
 *
 *	function List::__init__(value: int, next: List)
 *		List::value := value
 *		List::next := next
 *	end function
 */
class DesugarStruct final : public ASTDesugaring {
	std::stack<NodeStruct*> m_structs;
	bool m_in_static_method = false;
	/// holds all declared members and local declarations to replace self
	std::unordered_map<std::string, NodeDataStructure*> members;
	void add_to_members(NodeDataStructure* node) {
		if (members.contains(node->name)) {
			auto error = Diagnostic(ErrorType::SyntaxError, "", "", node->tok);
			error.message = "Member already exists.";
			error.actual = node->tok.val;
			error.exit();
		}
		members[node->name] = node;
	}
	std::string add_struct_prefix(const std::string& name) {
		if(!m_structs.empty()) {
			return m_structs.top()->name + OBJ_DELIMITER + name;
		}
		return name;
	}
	/// replace 'self.' only when struct member or declared var
	std::string replace_self_struct_prefix(const std::string& name, const Token& tok) {
		if (m_in_static_method && (name == "self" || name.find("self.") == 0)) {
			auto error = Diagnostic(ErrorType::SyntaxError, "", "", tok);
			error.message = "<self> cannot be used inside a <static function>. A static method belongs to "
				"the struct itself and is called without an instance, so there is no <self> to refer to. "
				"Pass the object as a parameter, or remove <static> to make it a regular method.";
			error.actual = name;
			error.exit();
		}
		if (!m_structs.empty() && name.find("self.") == 0) {
			std::string new_name = name;
			new_name.replace(0, 5, m_structs.top()->name + OBJ_DELIMITER); // Ersetze 'self.' durch das Präfix
			if (members.contains(new_name)) {
				return new_name;
			}
		}
		return name;
	}
	std::unique_ptr<NodeAccessChain> try_access_chain_transform(const std::string& name, NodeAST* node) const {
		if (!m_structs.empty() && name.find("self.") == 0) {
			return node->to_method_chain();
		}
		return nullptr;
	}
	/**
	 * @brief Retrieves the operator token for a given operator name and number of arguments.
	 * @param name The name of the operator.
	 * @param num_args The number of arguments the operator takes.
	 * @return std::optional<token> The corresponding token if found, otherwise std::nullopt.
 	 */
	static std::optional<token> get_operator_token(const std::string& name, int num_args) {
		static const std::unordered_map<StringIntKey, token, StringIntKeyHash> operator_overload_methods = [](){
			std::unordered_map<StringIntKey, token, StringIntKeyHash> result;
			for (const auto& [key, value] : OPERATOR_OVERWRITES) {
				result[StringIntKey{value.first, value.second}] = key;
			}
			return result;
		}();

		const auto it = operator_overload_methods.find(StringIntKey{name, num_args});
		if (it != operator_overload_methods.end()) {
			return it->second;
		}
		return std::nullopt;
	}
public:
	explicit DesugarStruct(NodeProgram *program) : ASTDesugaring(program) {};

	/// A member initialiser is emitted on the member's whole storage, which holds every instance
	/// back to back. KSP repeats the last value of an initializer list over the remaining indices,
	/// so a single value still reaches every instance - but a list of several does not: the second
	/// instance gets the last value repeated instead of the pattern.
	///
	/// <row[3]: int[] := (7, 8, 9)> with two instances becomes <%row[6] := (7, 8, 9)>, so the
	/// second instance reads (9, 9, 9). Such a value is moved into the constructor, which runs per
	/// instance. A scalar or a one element list is left alone: KSP already fills it correctly and
	/// an assignment per instance would only cost tokens.
	///
	/// Only for a hand written constructor. The generated one takes every member as a parameter
	/// and assigns it, so the initialiser is already overwritten for every instance.
	///
	/// <const> and <static> members are skipped: they have no per-instance storage. <NDArrays> are
	/// skipped as well - assigning one from an initializer list currently fails during function
	/// inlining, so moving the value there would turn a wrong value into a compile error.
	static void move_member_defaults_into_constructor(NodeStruct& node) {
		std::shared_ptr<NodeFunctionDefinition> constructor = nullptr;
		for (const auto& method : node.methods) {
			if (method->header->name == NodeStruct::CONSTRUCTOR) constructor = method;
		}
		if (!constructor) return;
		// walked backwards because each one is prepended: this keeps the declaration order
		for (auto it = node.members->statements.rbegin(); it != node.members->statements.rend(); ++it) {
			const auto declaration = (*it)->statement->cast<NodeSingleDeclaration>();
			if (!declaration or !declaration->value) continue;
			const auto& member = declaration->variable;
			if (member->data_type == DataType::Const or member->is_shared_member()) continue;
			// only a list of several values spreads wrongly over the instances
			const auto initializer = declaration->value->cast<NodeInitializerList>();
			if (!initializer or initializer->size() <= 1) continue;

			auto member_ref = member->to_reference();
			member_ref->name = "self." + member_ref->name;
			constructor->body->prepend_as_stmt(std::make_unique<NodeSingleAssignment>(
				std::move(member_ref), std::move(declaration->value), member->tok));
		}
	}

	NodeAST* visit(NodeStruct& node) override {
		node.name = add_struct_prefix(node.name);
		m_structs.push(&node);

		node.ty = TypeRegistry::add_object_type(node.name);
		node.node_self->ty = node.ty;

		////// check for existing init and repr methods -> generate if not present
		bool has_init_method = false;
		bool has_repr_method = false;
		for(auto & m: node.methods) {
			if (m->header->name == NodeStruct::CONSTRUCTOR) has_init_method = true;
			if (m->header->name == "__repr__") has_repr_method = true;
			if (m->header->name == "__del__") {
				auto error = Diagnostic(ErrorType::SyntaxError, "", "", m->tok);
				error.message = "Destructor method is generated automatically. Please use another name.";
				error.exit();
			}
		}
		// automatically generate init method if none provided by user
		if(!has_init_method) node.generate_init_method();
		if(!has_repr_method) node.generate_repr_method();
		if(has_init_method) move_member_defaults_into_constructor(node);

		node.members->accept(*this);
		node.members->flatten(); // needed for const blocks nested NodeBlock stmts
		// add self keyword for declarations
		// node.members->prepend_as_stmt(std::make_unique<NodeSingleDeclaration>(node.node_self, nullptr, node.tok));
		for(auto & m: node.methods) {
			m->accept(*this);
		}
		node.rebuild_member_table();
		node.rebuild_method_table();

		auto self_decl = std::make_unique<NodeSingleDeclaration>(
			node.node_self,
			nullptr,
			node.tok
		);
		node.members->prepend_as_stmt(std::move(self_decl));

		m_structs.pop();
		members.clear();
		return &node;
	}

	/// <static function foo()>: belongs to the struct, gets no <self> parameter and is called as
	/// <Foo.foo()>. Everything that needs an instance is rejected here.
	NodeAST* visit_static_method(NodeFunctionDefinition& node) {
		auto error = Diagnostic(ErrorType::SyntaxError, "", "", node.tok);
		if (!node.header->params.empty() and node.header->get_param(0)->name == "self") {
			error.message = "A <static function> must not declare <self> as its first parameter. It is called "
				"on the struct itself, not on an instance. Remove <self>, or remove <static>.";
			error.exit();
		}
		if (node.header->name == NodeStruct::CONSTRUCTOR or node.header->name == "__repr__"
			or node.header->name == "__del__") {
			error.message = "<"+node.header->name+"> operates on an instance and cannot be declared <static>.";
			error.exit();
		}
		if (get_operator_token(node.header->name, node.header->params.size())) {
			error.message = "Operator overloads are resolved through their operands and cannot be declared <static>.";
			error.exit();
		}
		m_in_static_method = true;
		node.header->accept(*this);
		node.header->name = add_struct_prefix(node.header->name);
		node.body->accept(*this);
		m_in_static_method = false;
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		// check if header name has dot in it -> this is not allowed
		if (node.header->name.find('.') != std::string::npos) {
			auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
			error.message = "Method name cannot contain '.' character. This is reserved for struct member access.";
			error.exit();
		}
		if(node.is_static) {
			return visit_static_method(node);
		}
		if(!node.is_method()) {
			auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
			error.message = "Method definition must contain <self> as first parameter.";
			error.exit();
		}
		// every self as first parameter has to be of type object
		node.header->get_param(0)->ty = TypeRegistry::get_object_type(m_structs.top()->name);

		// add constructor type
		if(node.header->name == NodeStruct::CONSTRUCTOR) {
			auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
			if(node.ty != TypeRegistry::Unknown and node.ty != TypeRegistry::get_object_type(m_structs.top()->name)) {
				error.message = "Constructor method has to be of object type.";
				error.actual = node.ty->to_string();
				error.expected = m_structs.top()->name;
				error.exit();
			}
			m_structs.top()->constructor = node.get_shared();
			if(node.num_return_params > 0) {
				error.message = "Constructor method cannot have return values.";
				error.exit();
			}
			node.num_return_params = 1;
			node.header->create_function_type(TypeRegistry::add_object_type(m_structs.top()->name));
			node.ty = TypeRegistry::add_object_type(m_structs.top()->name);
			// delete "self" keyword
			node.header->params.erase(node.header->params.begin());
		}
		if(node.header->name == "__repr__") {
			if(node.num_return_params > 1) {
				auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
				error.message = "Repr method cannot have more than one return value.";
				error.exit();
			}
			if(node.header->params.size() > 1) {
				auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
				error.message = "Repr method cannot have more than one argument.";
				error.exit();
			}
			node.num_return_params = 1;
			node.header->create_function_type(TypeRegistry::String);
			node.ty = TypeRegistry::String;
		}

		// check if method is operator overload
		if(auto token = get_operator_token(node.header->name, node.header->params.size())) {
			m_structs.top()->overloaded_operators.insert({*token, node.get_shared()});
		}

		node.header->accept(*this);
		node.header->name = add_struct_prefix(node.header->name);
		// add_to_members(node.header.get());
		node.body->accept(*this);
		return &node;
	}

	/// const blocks are already desugared at this time
	/// -> declaration statements inside a const blocks
	/// const declaration members already should have the static kind from parsing and desugaring
	NodeAST* visit(NodeConst& node) override {
		ASTVisitor::visit(node);
		return node.replace_with(std::move(node.constants));
	}

	static NodeConst* is_const_block_member(const NodeSingleDeclaration& node) {
		if (node.parent) {
			if (const auto stmt = node.parent->cast<NodeStatement>()) {
				if (stmt->parent and stmt->parent->cast<NodeBlock>()) {
					return stmt->parent->parent->cast<NodeConst>();
				}
			}
		}
		return nullptr;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.variable->is_member() or is_const_block_member(node)) {
			node.variable->name = add_struct_prefix(node.variable->name);
			add_to_members(node.variable.get());
			node.variable->accept(*this);
		} else {
			if(node.variable->name.find("self.") == 0) {
				auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
				error.message = "<self> keyword is only allowed in member declarations.";
			}
		}
		if(node.value) node.value->accept(*this);
		return &node;
	}

	// does currently ONLY visit when member declaration
	NodeAST* visit(NodeVariable& node) override {
		if(node.name == "SIZE") {
			auto error = Diagnostic(ErrorType::VariableError, "", "", node.tok);
			error.message = "Variable name 'SIZE' is reserved for array size constants. Please choose another name.";
			error.exit();
		}
		if(StringUtils::contains(node.name, "SIZE_D")) {
			auto error = Diagnostic(ErrorType::VariableError, "", "", node.tok);
			error.message = "Variable names containing 'SIZE_D' are reserved for multidimensional array size constants. Please choose another name.";
			error.exit();
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		node.name = replace_self_struct_prefix(node.name, node.tok);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}
	NodeAST * visit(NodePointerRef& node) override {
		node.name = replace_self_struct_prefix(node.name, node.tok);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}
	NodeAST * visit(NodeArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name, node.tok);
		if(node.index) node.index->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}
	NodeAST * visit(NodeNDArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name, node.tok);
		if(node.indexes) node.indexes->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}
	NodeAST * visit(NodeListRef& node) override {
		node.name = replace_self_struct_prefix(node.name, node.tok);
		node.indexes->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}
	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->name = replace_self_struct_prefix(node.function->name, node.tok);
		node.function->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.function->name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	}

};