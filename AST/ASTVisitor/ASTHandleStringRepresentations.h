//
// Created by Mathias Vatter on 14.10.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTHandleStringRepresentations: public ASTVisitor {
public:
	explicit ASTHandleStringRepresentations(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {};

	inline NodeAST * visit(NodeVariable& node) override {
		return &node;
	}
	inline NodeAST * visit(NodeVariableRef& node) override {
		return wrap_in_repr_call(node);
	}
	inline NodeAST * visit(NodePointer& node) override {
		return &node;
	}
	inline NodeAST * visit(NodePointerRef& node) override {
		return wrap_in_repr_call(node);
	}
	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return &node;
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		return wrap_in_repr_call(node);
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return &node;
	}
	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return wrap_in_repr_call(node);
	}
	inline NodeAST * visit(NodeList& node) override {
		return &node;
	}
	inline NodeAST * visit(NodeListRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return wrap_in_repr_call(node);
	}
	inline NodeAST * visit(NodeFunctionDefinition& node) override {
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
		return wrap_in_repr_call(node);
	}

private:
	DefinitionProvider* m_def_provider;
	std::unordered_set<Type*> m_repr_functions_in_use;

	static std::unique_ptr<NodeFunctionCall> construct_repr_call(NodeAST& node, std::string& prefix) {
		auto call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				prefix + OBJ_DELIMITER+"__repr__",
				std::make_unique<NodeParamList>(node.tok, node.clone()),
				node.tok
			),
			node.tok
		);
		call->ty = TypeRegistry::String;
		return call;
	}

	static NodeAST* wrap_in_repr_call(NodeFunctionCall& call) {
		if(call.ty->get_type_kind() != TypeKind::Object and call.ty->get_type_kind() != TypeKind::Composite)
			return nullptr;

		if(call.is_string_env()) {
			std::string prefix = call.ty->to_string();
			return call.replace_with(construct_repr_call(call, prefix));
		}
		return &call;
	}

	NodeAST * wrap_in_repr_call(NodeReference& ref) {
		if(ref.ty->get_type_kind() != TypeKind::Object and ref.ty->get_type_kind() != TypeKind::Composite)
			return nullptr;

		if(ref.is_string_env()) {
			// try to prohibit recursive repr constructions inside repr functions
			if(!m_repr_functions_in_use.empty() and m_repr_functions_in_use.find(ref.ty) != m_repr_functions_in_use.end()) {
				return &ref;
			}

			if(ref.ty->get_type_kind() == TypeKind::Composite) {
				if(auto arr_ref = ref.cast<NodeArrayRef>()) {
					generate_array_repr_method(*arr_ref);
				} else if (auto ndarr_ref = ref.cast<NodeNDArrayRef>()) {
					generate_ndarray_repr_method(*ndarr_ref);
				}
			}

			std::string prefix = ref.ty->to_string();
			return ref.replace_with(construct_repr_call(ref, prefix));
		}
		return &ref;
	}

	static Type* is_repr_header(NodeFunctionHeader& header) {
		if(contains(header.name, OBJ_DELIMITER+"__repr__")) {
			if(header.params.size() == 1) {
				return header.get_param(0)->ty;
			}
		}
		return nullptr;
	}

	/// generate __repr__ function for ArrayRef and add it to the program
	/// if return is false, the function already exists
	bool generate_array_repr_method(NodeArrayRef& node) {
		if(!node.declaration) {
			auto error = get_raw_compile_error(ErrorType::InternalError, node);
			error.m_message = "ArrayRef has no declaration";
			error.exit();
		}
		std::string func_name = node.ty->to_string()+OBJ_DELIMITER+"__repr__";
		if(m_program->function_lookup.find({func_name, 1}) != m_program->function_lookup.end()) {
			return false;
		}

		auto node_self = clone_as<NodeArray>(node.declaration.get());
		node_self->name = "self";
		node_self->size = nullptr;
		auto self_ref = node_self->to_reference();
		auto message = std::make_unique<NodeString>(
			"\"Array of type <"+node.ty->to_string()+">\"",
			node.tok
		);
		auto node_body = std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(
				std::make_unique<NodeReturn>(node.tok, std::move(message)), node.tok
			)
		);
		node_body->scope = true;
		auto function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeFunctionParam>(std::move(node_self)),
				node.tok
			),
			std::nullopt,
			false,
			std::move(node_body),
			node.tok
		);
		function_def->ty = TypeRegistry::String;
		function_def->num_return_params = 1;
		function_def->parent = m_program;
		m_program->additional_function_definitions.push_back(std::move(function_def));
		// update function lookup so that the new function can be found
		m_program->update_function_lookup();
		return true;
	}

	/// generate __repr__ function for NDArrayRef and add it to the program
	/// if return is false, the function already exists
	bool generate_ndarray_repr_method(NodeNDArrayRef& node) {
		if(!node.declaration) {
			auto error = get_raw_compile_error(ErrorType::InternalError, node);
			error.m_message = "NDArrayRef has no declaration";
			error.exit();
		}
		std::string func_name = node.ty->to_string()+OBJ_DELIMITER+"__repr__";
		if(m_program->function_lookup.find({func_name, 1}) != m_program->function_lookup.end()) {
			return false;
		}

		auto node_self = clone_as<NodeNDArray>(node.declaration.get());
		node_self->name = "self";
		node_self->sizes = nullptr;
		auto self_ref = node_self->to_reference();
		auto message = std::make_unique<NodeString>(
			"\"NDArray of type <"+node.ty->to_string()+">\"",
			node.tok
		);
		auto node_body = std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(
				std::make_unique<NodeReturn>(node.tok, std::move(message)), node.tok
			)
		);
		node_body->scope = true;
		auto function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeFunctionParam>(std::move(node_self)),
				node.tok
			),
			std::nullopt,
			false,
			std::move(node_body),
			node.tok
		);
		function_def->ty = TypeRegistry::String;
		function_def->num_return_params = 1;
		function_def->parent = m_program;
		m_program->additional_function_definitions.push_back(std::move(function_def));
		// update function lookup so that the new function can be found
		m_program->update_function_lookup();
		return true;
	}
};