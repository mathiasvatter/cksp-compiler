//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTLowerTypes: public ASTVisitor {
public:
	explicit ASTLowerTypes(NodeProgram *main): m_def_provider(main->def_provider) {
		m_program = main;
	};

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		// most func defs will be visited when called, keeping local scopes in mind
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	};

	inline NodeAST * visit(NodeVariable& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeVariableRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointer& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointerRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeList& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeListRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeFunctionDefinition& node) override {
		if(node.ty->get_element_type()->cast<ObjectType>()) {
			node.set_element_type(TypeRegistry::Integer);
		}

		node.header->accept(*this);
		node.body->accept(*this);
		node.visited = true;

		return &node;
	}
	inline NodeAST * visit(NodeFunctionCall& node) override {
		// go first into definition to lower the header type first und give that to the header ref
		if(node.bind_definition(m_program)) {
			if(!node.get_definition()->visited) node.get_definition()->accept(*this);
			node.get_definition()->visited = true;
		}
		node.function->accept(*this);
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		return &node;
	}
	inline NodeAST * visit(NodeFunctionHeader& node) override {
		for(auto &param : node.params) param->accept(*this);
		if(auto func_type = node.ty->cast<FunctionType>()) {
			auto return_type = func_type->get_return_type();
			if (return_type->cast<ObjectType>()) {
				return_type = TypeRegistry::Integer;
			}
			node.create_function_type(return_type);
		}
		return &node;
	}
	inline NodeAST * visit(NodeFunctionHeaderRef& node) override {
		if(node.args) node.args->accept(*this);
		if(auto decl = node.get_declaration()) {
			node.ty = decl->ty;
		}
		return node.lower_type();
	}

private:
	DefinitionProvider* m_def_provider;
};