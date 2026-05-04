//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugar.h"

#include "../../Desugaring/DesugarNamespace.h"

NodeAST* ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;

	// add boolean funcs to program to ensure correct var and type checking
	for (auto val : m_program->def_provider->boolean_functions | std::views::values) {
		m_program->add_function_definition(val);
	}
	m_program->merge_function_definitions();

	// first desugar namespaces to assign correct prefixes
	static DesugarNamespace ns_desugar(m_program);
	visit_all(node.namespaces, ns_desugar);
	// move all namespaces into global declarations block
	for (auto & ns : node.namespaces) {
		m_program->global_declarations->add_as_stmt(std::move(ns));
	}
	node.namespaces.clear();

	is_global_declaration = true;
	m_program->global_declarations->accept(*this);
	is_global_declaration = false;
	m_program->add_global_iterator();


	// m_program->global_declarations->prepend_as_stmt(m_program->declare_global_iterators());
	visit_all(node.struct_definitions, *this);
	visit_all(node.callbacks, *this);
	visit_all(node.function_definitions, *this);

	// update because function parameters might have been added which might cause problems in typechecking
//	m_program->update_function_lookup();
//	m_program->global_declarations->append_body(declare_compiler_variables());
	m_program->global_declarations->append_body(std::move(m_global_variable_declarations));
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
	node.header->accept(*this);
	node.body->accept(*this);
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeFunctionCall& node) {
	node.function->accept(*this);
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeDeclaration& node) {
	// desugar first into single declarations and then visit them
    return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeSingleDeclaration& node) {
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);

	// if var is global -> make assignment and move declaration to global declarations
    if(node.variable->is_global and !is_global_declaration) {
    	node.variable->is_global = true;
        m_global_variable_declarations->add_as_stmt(
			std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok)
		);
		return node.remove_node();
    }
	return node.desugar(m_program);
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
	return node.desugar(m_program);
}

NodeAST * ASTDesugar::visit(NodeFormatString &node) {
	return node.desugar(m_program)->accept(*this);
}

NodeAST *ASTDesugar::visit(NodeBinaryExpr &node) {
	node.left->accept(*this);
	node.right->accept(*this);
	return node.desugar(m_program);
}


