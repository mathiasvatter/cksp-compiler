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
 *	declare List.value: int
 *	declare List.next: List := nil
 *
 *	function List.__init__(value: int, next: List)
 *		List.value := value
 *		List.next := next
 *	end function
 */
class DesugarStruct : public ASTDesugaring {
private:
	std::stack<std::string> m_struct_prefixes;
	std::string add_struct_prefix(const std::string& name) {
		if(!m_struct_prefixes.empty()) {
			return m_struct_prefixes.top() + "." + name;
		}
		return name;
	}
	std::string replace_self_struct_prefix(const std::string& name) {
		if (!m_struct_prefixes.empty() && name.find("self.") == 0) {
			std::string new_name = name;
			new_name.replace(0, 4, m_struct_prefixes.top()); // Ersetze 'self.' durch das Präfix
			return new_name;
		}
		return name;
	}
public:
	explicit DesugarStruct(NodeProgram *program) : ASTDesugaring(program) {};

	inline void visit(NodeStruct& node) override {
		node.name = add_struct_prefix(node.name);
		m_struct_prefixes.push(node.name);

		node.members->accept(*this);
		for(auto & m: node.methods) {
			m->accept(*this);
		}
		node.update_member_table();
		node.update_method_table();
	}

	inline void visit(NodeFunctionDefinition& node) override {
		if(!node.is_method()) {
			auto error = CompileError(ErrorType::SyntaxError,"", "", node.tok);
			error.m_message = "Method definition must contain <self> as first parameter.";
			error.exit();
		}
		node.header->accept(*this);
		node.header->name = add_struct_prefix(node.header->name);
		// delete "self" keyword
		node.header->args->params.erase(node.header->args->params.begin());

		node.body->accept(*this);
	}

	inline void visit(NodeSingleDeclaration& node) override {
		if(node.variable->is_member()) {
			node.variable->name = add_struct_prefix(node.variable->name);
		}
		if(node.value) node.value->accept(*this);
	}

	void visit(NodeVariableRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
	};
	void visit(NodePointerRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
	};
	void visit(NodeArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(node.index) node.index->accept(*this);
	};
	void visit(NodeNDArrayRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		if(node.indexes) node.indexes->accept(*this);
	};
	void visit(NodeListRef& node) override {
		node.name = replace_self_struct_prefix(node.name);
		node.indexes->accept(*this);
	};

private:

};