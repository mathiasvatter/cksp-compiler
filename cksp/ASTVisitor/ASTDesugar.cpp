//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugar.h"

#include "../CompilerConfig.h"
#include "../Desugaring/DesugarNamespace.h"
#include "FunctionHandling/DeprecatedReturnSyntaxAnalyzer.h"

NodeAST* ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;

	// add boolean funcs to program to ensure correct var and type checking
	for (auto val : m_program->def_provider->boolean_functions | std::views::values) {
		m_program->add_function_definition(val);
	}
	m_program->merge_function_definitions();

	// first desugar namespaces to assign correct prefixes.
	// Not static: the collected namespace paths and prefixed names belong to this AST,
	// and the language server runs many analyses in one process.
	DesugarNamespace ns_desugar(m_program);
	// visit_all(node.namespaces, ns_desugar);
	// move all namespaces into global declarations block
	for (auto & ns : node.namespaces) {
		ns->accept(ns_desugar);
	}
	node.namespaces.clear();

	m_program->add_global_iterator(); // has to be added before visiting global_declarations, in case we have
	// a ui control array there which adds a global iterator, causing the NodeBlock iteration to crash
	is_global_declaration = true;
	m_program->global_declarations->accept(*this);
	is_global_declaration = false;


	// m_program->global_declarations->prepend_as_stmt(m_program->declare_global_iterators());
	// visit_all(node.struct_definitions, *this);
	visit_all(node.callbacks, *this);
	visit_all(node.function_definitions, *this);

	// update because function parameters might have been added which might cause problems in typechecking
//	m_program->update_function_lookup();
//	m_program->global_declarations->append_body(declare_compiler_variables());
	m_program->global_declarations->prepend_body(std::move(m_global_variable_declarations));

	m_program->update_struct_lookup(); // in case a struct is in a namespace and changed its typename
	return &node;
}

NodeAST* ASTDesugar::visit(NodeBlock& node) {
    for(const auto & stmt : node.statements) {
        stmt->accept(*this);
    }
	node.flatten();
	return &node;
}

NodeAST* ASTDesugar::visit(NodeFunctionDefinition& node) {
	m_program->function_definition_stack.push(node.weak_from_this());
	node.header->accept(*this);
	node.body->accept(*this);
	m_program->function_definition_stack.pop();
	if (m_program->compiler_config->lsp && node.return_variable) {
		DeprecatedReturnSyntaxAnalyzer deprecated_returns(m_program);
		deprecated_returns.analyze(node);
	}
	return node.desugar(m_program);
}


NodeAST* ASTDesugar::visit(NodeFunctionCall& node) {
	node.function->accept(*this);
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeDeclaration& node) {
	// desugar first into single declarations and then visit them
    const auto new_node = node.desugar(m_program);
	return new_node->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeSingleDeclaration& node) {
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);

	// if var is global -> make assignment and move declaration to global declarations
    if(node.variable->is_global and (!is_global_declaration or !m_program->function_definition_stack.empty())) {
    	node.variable->is_global = true;
        m_global_variable_declarations->add_as_stmt(
			std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok)
		);
    	// m_global_variable_declarations->get_last_statement()->desugar(m_program);
		return node.remove_node();
    }
	return &node;
}

NodeAST* ASTDesugar::visit(NodeAssignment &node) {
	return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	node.r_value->accept(*this);
	return node.desugar(m_program);
}

NodeAST * ASTDesugar::visit(NodeCompoundAssignment &node) {
	return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeFamily &node) {
    // node.members->accept(*this);
	return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeConst &node) {
	node.constants->accept(*this);
	return node.desugar(m_program);
}

NodeAST * ASTDesugar::visit(NodeNamespace &node) {
	// node.members->accept(*this);
	// for(const auto & fun: node.function_definitions) {
	// 	fun->accept(*this);
	// 	m_program->function_lookup[{fun->header->name, (int)fun->get_num_params()}].push_back(fun);
	// }
	ASTVisitor::visit(node);
	for(auto & func: node.function_definitions) {
		m_program->add_function_or_override(func);
	}
	node.function_definitions.clear();
	return node.replace_with(std::move(node.members));
	return &node;
}

NodeAST* ASTDesugar::visit(NodeDelete &node) {
	for(auto & ptr : node.ptrs) {
		ptr->accept(*this);
	}
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeParamList &node) {
	for(auto & param : node.params) {
		param->accept(*this);
	}
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	// DesugarStruct gives the struct its final, namespace qualified name, which the blocks need
	// as their prefix - so they can only be hoisted once it has run
	const auto desugared = node.desugar(m_program);
	hoist_const_blocks(node);
	return desugared;
}

void ASTDesugar::hoist_const_blocks(NodeStruct& node) {
	if(!node.const_blocks) return;
	for(auto& stmt : node.const_blocks->statements) {
		// <Voice.State.IDLE>: the struct name is joined with a '.' rather than the '::' used for
		// members, so the entry resolves as one flat constant name instead of being taken apart
		// into an access chain through a member that does not exist
		if(const auto node_const = stmt->statement->cast<NodeConst>()) {
			node_const->const_prefix.val = node.name + "." + node_const->const_prefix.val;
			node_const->name = node_const->const_prefix.val;
		}
		// staged rather than added to <global_declarations> directly: that block is being
		// traversed right now. It is prepended once the traversal is done.
		m_global_variable_declarations->add_stmt(std::move(stmt))->accept(*this);
	}
	node.const_blocks = nullptr;
}

NodeAST * ASTDesugar::visit(NodeFormatString &node) {
	return node.desugar(m_program)->accept(*this);
}

NodeAST *ASTDesugar::visit(NodeBinaryExpr &node) {
	node.left->accept(*this);
	node.right->accept(*this);
	return node.desugar(m_program);
}
