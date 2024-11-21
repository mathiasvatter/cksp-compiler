//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugar.h"

NodeAST* ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;

	m_program->global_declarations->accept(*this);
	for(auto & struct_def : node.struct_definitions) {
		struct_def->accept(*this);
	}
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & func_def : node.function_definitions) {
        func_def->accept(*this);
    }
	// update because function parameters might have been added which might cause problems in typechecking
//	m_program->update_function_lookup();
	m_program->global_declarations->prepend_body(NodeStruct::declare_struct_constants());
	m_program->init_callback->statements->prepend_body(NodeProgram::declare_compiler_variables());
//	m_program->global_declarations->append_body(declare_compiler_variables());
	m_program->global_declarations->append_body(std::move(m_global_variable_declarations));
	return &node;
}

NodeAST* ASTDesugar::visit(NodeBlock& node) {
    for(auto & stmt : node.statements) {
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
    if(node.variable->is_global) {
        m_global_variable_declarations->add_as_stmt(
			std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok)
		);
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

NodeAST* ASTDesugar::visit(NodeForEach& node) {
    node.body->accept(*this);
	// accept again to desugar resulting for loops
	return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeFor& node) {
    node.body->accept(*this);
	return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeFamily &node) {
    node.members->accept(*this);
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeConst &node) {
	node.constants->accept(*this);
	return node.desugar(m_program);
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

NodeAST *ASTDesugar::visit(NodeBinaryExpr &node) {
	node.left->accept(*this);
	node.right->accept(*this);
	return node.desugar(m_program);
}


