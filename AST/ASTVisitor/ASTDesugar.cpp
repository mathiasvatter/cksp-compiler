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
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
	// update because function parameters might have been added which might cause problems in typechecking
	m_program->update_function_lookup();
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

NodeAST* ASTDesugar::visit(NodeDeclaration& node) {
	// desugar first into single declarations and then visit them
    return node.desugar(m_program)->accept(*this);
}

NodeAST* ASTDesugar::visit(NodeSingleDeclaration& node) {
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);

    if(node.variable->is_global) {
        m_global_variable_declarations->statements.push_back(
			std::make_unique<NodeStatement>(
				node.clone(),
				node.tok
			)
		);
		return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
    }
	return &node;
}

NodeAST* ASTDesugar::visit(NodeAssignment &node) {
	return node.desugar(m_program)->accept(*this);
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

NodeAST* ASTDesugar::visit(NodeParamList &node) {
	for(auto & param : node.params) {
		param->accept(*this);
	}
	// in case it is a double param_list [[0,1,2,3]] move params up to parent
	if(auto node_param_list = cast_node<NodeParamList>(node.parent)) {
		if(node_param_list->params.size() == 1) {
			node_param_list->params.insert(
				node_param_list->params.begin(),
				std::make_move_iterator(node.params.begin()),
				std::make_move_iterator(node.params.end())
			);
			node_param_list->params.pop_back();
			node_param_list->set_child_parents();
		}
	}
	return &node;
}

NodeAST* ASTDesugar::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	return node.desugar(m_program);
}


