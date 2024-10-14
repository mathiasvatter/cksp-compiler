//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTLowerTypes: public ASTVisitor {
public:
	explicit ASTLowerTypes(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {};

	inline NodeAST * visit(NodeVariable& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeVariableRef& node) override {
		if(auto new_node = wrap_in_repr_call(node)) return new_node;
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointer& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointerRef& node) override {
		if(auto new_node = wrap_in_repr_call(node)) return new_node;
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(auto new_node = wrap_in_repr_call(node)) return new_node;
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(auto new_node = wrap_in_repr_call(node)) return new_node;
		return node.lower_type();
	}
	inline NodeAST * visit(NodeList& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeListRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(auto new_node = wrap_in_repr_call(node)) return new_node;
		return node.lower_type();
	}
	inline NodeAST * visit(NodeFunctionDefinition& node) override {
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		auto repr_type = is_repr_header(*node.header);
		// check because of recursion
		if(repr_type) m_repr_functions_in_use.insert(repr_type);

		node.header->accept(*this);
		node.body->accept(*this);

		if(repr_type) m_repr_functions_in_use.erase(repr_type);
		return &node;
	}
	inline NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			if(node.is_string_env()) {
				auto prefix = node.ty->to_string();
				node.set_element_type(TypeRegistry::Integer);
				return construct_repr_call(node, prefix)->accept(*this);
			}
			node.set_element_type(TypeRegistry::Integer);
		}
		return &node;
	}

private:
	DefinitionProvider* m_def_provider;
	std::unordered_set<Type*> m_repr_functions_in_use;

	static std::unique_ptr<NodeFunctionCall> construct_repr_call(NodeAST& node, std::string& prefix) {
		return std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionVarRef>(
				std::make_unique<NodeFunctionHeader>(
					prefix + OBJ_DELIMITER+"__repr__",
					std::make_unique<NodeParamList>(node.tok, node.clone()),
					node.tok
				),
				node.tok
			),
			node.tok
		);
	}

	NodeAST * wrap_in_repr_call(NodeReference& ref) {
		if(ref.ty->get_type_kind() != TypeKind::Object) return nullptr;

		if(ref.is_string_env()) {
			// try to prohibit recursive repr constructions inside repr functions
			if(!m_repr_functions_in_use.empty() and m_repr_functions_in_use.find(ref.ty) != m_repr_functions_in_use.end()) {
				return nullptr;
			}
			auto prefix = ref.ty->to_string();

			ref.lower_type();
			return ref.replace_with(construct_repr_call(ref, prefix));
		}
		return nullptr;
	}

	static Type* is_repr_header(NodeFunctionHeader& header) {
		if(contains(header.name, OBJ_DELIMITER+"__repr__")) {
			if(header.args->params.size() == 1) {
				return header.args->params[0]->ty;
			}
		}
		return nullptr;
	}
};