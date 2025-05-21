//
// Created by Mathias Vatter on 17.05.25.
//

#pragma once
#include "ASTVisitor.h"
#include "UniqueParameterNamesProvider.h"
#include "ReferenceManagement/ASTCollectDeclarations.h"

class ArraySizeFromRValue : public ASTVisitor {
	NodeComposite* m_variable = nullptr;

public:
	explicit ArraySizeFromRValue() = default;
	void get_array_size(NodeDataStructure& variable, NodeAST& r_value) {
		m_variable = nullptr;
		variable.accept(*this);
		if (m_variable) r_value.accept(*this);
	}

private:
	// possible r_values: ArrayRef, NDArrayRef, NodeInitializerList
	NodeAST* visit(NodeArrayRef& node) override {
		if (auto decl = node.get_declaration()) {
			m_variable->set_size(decl->get_size());
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "ArraySizeFromRValue: ArrayRef has to have a declaration.";
			error.exit();
		}
		// m_variable->set_size(node.get_size());
		return &node;
	}
	NodeAST* visit(NodeNDArrayRef& node) override {
		if (auto decl = node.get_declaration()) {
			m_variable->set_size(decl->get_size());
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "ArraySizeFromRValue: NDArrayRef has to have a declaration.";
			error.exit();
		}

		// m_variable->set_size(node.get_size());
		return &node;
	}
	NodeAST* visit(NodeInitializerList& node) override {
		auto dims = node.get_dimensions();
		auto node_dims = std::make_unique<NodeParamList>(node.tok);
		for (auto dim : dims) {
			node_dims->add_param(std::make_unique<NodeInt>(dim, node.tok));
		}
		m_variable->set_size(std::move(node_dims));
		return &node;
	}

	// possible l_values: Array, NDArray
	NodeAST* visit(NodeArray& node) override {
		m_variable = &node;
		return &node;
	}
	NodeAST* visit(NodeNDArray& node) override {
		m_variable = &node;
		return &node;
	}


	/// do not visit other nodes
	NodeAST* visit(NodeBinaryExpr& node) override {
		return &node;
	}
	NodeAST* visit(NodeUnaryExpr& node) override {
		return &node;
	}
	NodeAST* visit(NodeFunctionCall& node) override {
		return &node;
	}

};

class ASTParameterQualifier : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

	/// maps function hashes (name + num_params) to map of hashes (name + non-reference array sizes) to function definitions
	std::unordered_map<StringIntKey, std::unordered_map<std::string, NodeFunctionDefinition*>, StringIntKeyHash> m_function_identifiers;

	/// returns special function hash with function name, parameter types and the sizes of non reference arrays
	static std::string get_size_function_hash(const NodeFunctionHeaderRef& header_ref, const NodeFunctionHeader& header) {
		std::string hash{};
		for (int i = 0; i<header_ref.get_num_args(); i++) {
			auto& arg = header_ref.get_arg(i);
			auto& param = header.params[i];
			std::unique_ptr<NodeAST> size = nullptr;
			// if (!param->is_pass_by_ref and arg->ty->cast<CompositeType>()) {
			// 	if (auto arr_ref = arg->cast<NodeArrayRef>()) {
			// 		if (auto decl = arr_ref->get_declaration()) {
			// 			size = decl->get_size();
			// 		} else {
			// 			auto error = CompileError(ErrorType::InternalError, "", "", header_ref.tok);
			// 			error.m_message = "ArrayRef has to have a declaration to be transformed.";
			// 			error.exit();
			// 		}
			// 	} else if (auto ndarr_ref = arg->cast<NodeNDArrayRef>()) {
			// 		if (auto decl = ndarr_ref->get_declaration()) {
			// 			size = decl->get_size();
			// 		} else {
			// 			auto error = CompileError(ErrorType::InternalError, "", "", header_ref.tok);
			// 			error.m_message = "NDArrayRef has to have a declaration to be transformed.";
			// 			error.exit();
			// 		}
			// 	}
			// 	if (size) {
			// 		hash += "_" + size->get_string();
			// 	}
			// }
			if (arg->cast<NodeInitializerList>()) {
				hash += "_" + arg->get_string();
			}
		}
		return hash;
	}

	static bool fill_in_array_sizes(const NodeFunctionCall& call, const NodeFunctionDefinition& def) {
		for (int i = 0; i < call.function->get_num_args(); ++i) {
			auto& arg = call.function->get_arg(i);
			auto& param = def.get_param(i);
			static ArraySizeFromRValue arr;
			arr.get_array_size(*param, *arg);
		}
		return true;
	}

public:

	explicit ASTParameterQualifier(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.merge_function_definitions();
		node.update_function_lookup();
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		auto definition = node.get_definition();
		if (!definition) return &node;
		if (node.is_builtin_kind()) return &node;

		auto size_hash = get_size_function_hash(*node.function, *definition->header);
		// if function is not yet in map -> not processed yet -> add to map and set array sizes
		StringIntKey hash({definition->header->name, (int)definition->get_num_params()});
		if (!m_function_identifiers.contains(hash)) {
			fill_in_array_sizes(node, *definition);
			m_function_identifiers[hash][size_hash] = definition.get();
		} else {
			// function was already processed -> check if sizes match
			auto it = m_function_identifiers[hash].find(size_hash);
			if (it != m_function_identifiers[hash].end()) {
				// sizes match -> use existing function, do nothing
				auto new_definition = it->second;
				node.definition = new_definition->get_shared();
				node.function->name = new_definition->header->name;
			} else {
				// sizes do not match -> create new function
				auto new_def = clone_as<NodeFunctionDefinition>(definition.get());
				new_def->header->name = m_def_provider->get_fresh_name(definition->header->name);
				node.function->name = new_def->header->name;
				fill_in_array_sizes(node, *new_def);
				m_function_identifiers[hash][size_hash] = new_def.get();

				new_def->remove_references();
				static ASTCollectDeclarations collect(m_program);
				new_def->accept(collect);
				new_def->collect_references();

				static UniqueParameterNamesProvider unique_params(m_program);
				unique_params.do_renaming(*new_def);

				new_def->call_sites.clear();
				auto func_ptr = m_program->add_function_definition(std::move(new_def));
				node.definition = func_ptr->get_shared();
			}
		}

		// important, definition might have changed since beginning of this visit func
		node.get_definition()->call_sites.insert(&node);

		if (!definition->visited) {
			definition->accept(*this);
			definition->visited = true;
		}

		return &node;
	}

};
