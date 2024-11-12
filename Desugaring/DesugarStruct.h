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
 *	declare List::value: int
 *	declare List::next: List := nil
 *
 *	function List::__init__(value: int, next: List)
 *		List::value := value
 *		List::next := next
 *	end function
 */
class DesugarStruct : public ASTDesugaring {
private:
	std::stack<NodeStruct*> m_structs;
	/// holds all declared members and local declarations to replace self
	std::unordered_map<std::string, NodeDataStructure*> members;
	void add_to_members(NodeDataStructure* node) {
		if (members.find(node->name) != members.end()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
			error.m_message = "Member already exists.";
			error.m_got = node->tok.val;
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
	std::string replace_self_struct_prefix(const std::string& name) {
		if (!m_structs.empty() && name.find("self.") == 0) {
			std::string new_name = name;
			new_name.replace(0, 5, m_structs.top()->name + OBJ_DELIMITER); // Ersetze 'self.' durch das Präfix
			if (members.find(new_name) != members.end()) {
				return new_name;
			}
		}
		return name;
	}
	std::unique_ptr<NodeAccessChain> try_access_chain_transform(const std::string& name, NodeAST* node) {
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

		auto it = operator_overload_methods.find(StringIntKey{name, num_args});
		if (it != operator_overload_methods.end()) {
			return it->second;
		}
		return std::nullopt;
	}
public:
	explicit DesugarStruct(NodeProgram *program) : ASTDesugaring(program) {};

	inline NodeAST* visit(NodeStruct& node) override {
		node.name = add_struct_prefix(node.name);
		m_structs.push(&node);

		////// check for existing init and repr methods -> generate if not present
		bool has_init_method = false;
		bool has_repr_method = false;
		for(auto & m: node.methods) {
			if (m->header->name == "__init__") has_init_method = true;
			if (m->header->name == "__repr__") has_repr_method = true;
			if (m->header->name == "__del__") {
				auto error = CompileError(ErrorType::SyntaxError, "", "", m->tok);
				error.m_message = "Destructor method is generated automatically. Please use another name.";
				error.exit();
			}
		}
		// automatically generate init method if none provided by user
		if(!has_init_method) node.generate_init_method();
		if(!has_repr_method) node.generate_repr_method();

		node.members->accept(*this);
		// add self keyword for declarations
		node.members->prepend_as_stmt(std::make_unique<NodeSingleDeclaration>(node.node_self, nullptr, node.tok));
		for(auto & m: node.methods) {
			m->accept(*this);
		}
		node.update_member_table();
		node.update_method_table();

		m_structs.pop();
		members.clear();
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		if(!node.is_method()) {
			auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
			error.m_message = "Method definition must contain <self> as first parameter.";
			error.exit();
		}
		// every self as first parameter has to be of type object
		node.header->get_param(0)->ty = TypeRegistry::get_object_type(m_structs.top()->name);

		// add constructor type
		if(node.header->name == "__init__") {
			auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
			if(node.ty != TypeRegistry::Unknown and node.ty != TypeRegistry::get_object_type(m_structs.top()->name)) {
				error.m_message = "Constructor method has to be of object type.";
				error.m_got = node.ty->to_string();
				error.m_expected = m_structs.top()->name;
				error.exit();
			}
			m_structs.top()->constructor = &node;
			if(node.num_return_params > 0) {
				error.m_message = "Constructor method cannot have return values.";
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
				auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
				error.m_message = "Repr method cannot have more than one return value.";
				error.exit();
			}
			if(node.header->params.size() > 1) {
				auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
				error.m_message = "Repr method cannot have more than one argument.";
				error.exit();
			}
			node.num_return_params = 1;
			node.header->create_function_type(TypeRegistry::String);
			node.ty = TypeRegistry::String;
		}

		// check if method is operator overload
		if(auto token = get_operator_token(node.header->name, node.header->params.size())) {
			m_structs.top()->overloaded_operators.insert({*token, &node});
		}

		node.header->accept(*this);
		node.header->name = add_struct_prefix(node.header->name);

		node.body->accept(*this);
		return &node;
	}


	inline NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.variable->is_member()) {
			node.variable->name = add_struct_prefix(node.variable->name);
			add_to_members(node.variable.get());
			node.variable->accept(*this);
		} else {
			if(node.variable->name.find("self.") == 0) {
				auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
				error.m_message = "<self> keyword is only allowed in member declarations.";
			}
		}
		if(node.value) node.value->accept(*this);
		return &node;
	}

	// does currently ONLY visit when member declaration
	NodeAST* visit(NodeVariable& node) override {
		if(node.name == "SIZE") {
			auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
			error.m_message = "Variable name 'SIZE' is reserved for array size constants. Please choose another name.";
			error.exit();
		}
		if(contains(node.name, "SIZE_D")) {
			auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
			error.m_message = "Variable names containing 'SIZE_D' are reserved for multidimensional array size constants. Please choose another name.";
			error.exit();
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};
	NodeAST * visit(NodePointerRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};
	NodeAST * visit(NodeArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(node.index) node.index->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};
	NodeAST * visit(NodeNDArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(node.indexes) node.indexes->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};
	NodeAST * visit(NodeListRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		node.indexes->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};
	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->name = replace_self_struct_prefix(node.function->name);
		node.function->accept(*this);
		if(auto access_chain = try_access_chain_transform(node.function->name, &node)) {
			return node.replace_with(std::move(access_chain));
		}
		return &node;
	};

};