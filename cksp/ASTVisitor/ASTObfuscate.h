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
 * Uses EngineConstantsIntegers.h to substitute readable KSP constants with their integer counterparts
 */
class ASTObfuscate final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	IdentifierObfuscator gen;

public:
	explicit ASTObfuscate(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* run(NodeProgram& node) {
		node.accept(*this);
		return &node;
	}

private:

	void generate_new_name(NodeDataStructure& node) {
		if (node.kind == NodeDataStructure::Kind::Builtin) return;
		if (node.ty == TypeRegistry::PGS) return;
		node.name = gen.next();
		// parallel_for_each(node.references.begin(), node.references.end(),
		// 	[&](const auto& ref) {
		// 		ref->name = node.name;
		// 	}
		// );
	}

	static void get_new_name(NodeReference& node) {
		if (node.kind == NodeReference::Kind::Builtin) return;
		if (node.ty == TypeRegistry::PGS) return;
		if (auto decl = node.get_declaration()) {
			node.name = decl->name;
		// -> there are errors sometimes with builtin func headers "search" so no error throwing here
		// } else {
		// 	DefinitionProvider::internal_missing_declaration_error(node).exit();
		}
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program->global_declarations->accept(*this);
		// m_program->init_callback->accept(*this);
		visit_all(node.callbacks, *this);
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariable& node) override {
		generate_new_name(node);
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if (node.kind == NodeReference::Kind::Builtin) {
			auto substitute = EngineConstantsIntegers::get_constant_node(node.name, node.tok);
			if (substitute) {
				return node.replace_with(std::move(substitute));
			}
		}
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
