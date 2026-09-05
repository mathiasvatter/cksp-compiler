//
// Created by Mathias Vatter on 13.07.26.
//

#pragma once
#include "ASTVisitor.h"
#include "../../utils/IdentifierObfuscator.h"
#include "../BuiltinsProcessing/EngineConstantsIntegers.h"

/**
 * Assumes AST is in global scope and all references have their declarations set and all
 * declarations have their reference-set set.
 * -> not builtin vars/commands are obfuscated and no PGS variables (since they are dependent on vars
 * declared in other scripts)
 * Uses IdentifierObfuscator class to make variable names random and hard to read
 * DEPRECATED: Uses EngineConstantsIntegers.h to substitute readable KSP constants with their integer counterparts
 * important: does not scramble names of ui control because those ids need to be deterministic for KUI scripts
 * important: does not scramble names of arrays used in array_load_save_functions (load_array(...))
 * important: does not scramble names of vars used in watch_var or watch_idx
 */
class ASTObfuscate final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	IdentifierObfuscator gen;
	std::unordered_map<NodeDataStructure*, std::string> m_og_var_names;
public:
	explicit ASTObfuscate(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* run(NodeProgram& node) {
		m_og_var_names.clear();
		node.accept(*this);
		return &node;
	}

private:

	void generate_new_name(NodeDataStructure& node) {
		if (node.kind == NodeDataStructure::Kind::Builtin) return;
		if (node.ty == TypeRegistry::PGS) return;
		m_og_var_names.insert({&node, node.name});
		node.name = gen.next();
	}

	void get_new_name(NodeReference& node) {
		if (node.kind == NodeReference::Kind::Builtin) return;
		if (node.ty == TypeRegistry::PGS) return;

		// check if this is arg in array_load_save_func or is ksp log func
		if (const auto func = node.is_direct_func_arg()) {
			if (func->kind == NodeReference::Builtin and
				(BuiltinRestrictionValidator::is_load_save_function(func->name) or
					BuiltinRestrictionValidator::is_ksp_log_func(func->name))) {
				// rename back all references and og data structure
				rename_references_back(node);
				return;
			}
		}

		if (auto decl = node.get_declaration()) {
			node.name = decl->name;
		}
	}

	// can be used once we find out that one reference
	void rename_references_back(const NodeReference& ref) {
		const auto node = ref.get_declaration();
		if (!node) return;
		const auto it = m_og_var_names.find(node.get());
		if (it == m_og_var_names.end()) {
			return;
		}
		const std::string og_name = it->second;
		for (const auto& reff : node->references) {
			reff->name = og_name;
		}
		node->name = og_name;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program->global_declarations->accept(*this);
		// m_program->init_callback->accept(*this);
		visit_all(node.callbacks, *this);
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		// do not scramble names of ui control variables because of identifiers and KUI they need
		// to be deterministic
		if (!node.variable->cast<NodeUIControl>()) {
			node.variable->accept(*this);
		}
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeVariable& node) override {
		generate_new_name(node);
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		// if (node.kind == NodeReference::Kind::Builtin) {
		// 	auto substitute = EngineConstantsIntegers::get_constant_node(node.name, node.tok);
		// 	if (substitute) {
		// 		return node.replace_with(std::move(substitute));
		// 	}
		// }
		get_new_name(node);
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		generate_new_name(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeArrayRef& node) override {
		get_new_name(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		// if (node.kind == NodeFunctionHeader::Builtin) return &node;
		generate_new_name(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		// if (node.kind == NodeFunctionHeaderRef::Kind::Builtin) return &node;
		get_new_name(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			if(node.is_builtin_kind()) return &node;
			const auto definition = node.get_definition();
			if (!definition -> visited) definition->body->accept(*this);
			node.get_definition()->visited = true;
		}
		return &node;
	}


};
