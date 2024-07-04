//
// Created by Mathias Vatter on 21.01.24.
//

#include "ASTDesugar.h"

void ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;
	for(auto & struct_def : node.struct_definitions) {
		struct_def->accept(*this);
	}
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function_definition : node.function_definitions) {
        function_definition->accept(*this);
    }
	m_program->global_declarations->prepend_body(NodeStruct::declare_struct_constants());
	m_program->init_callback->statements->prepend_body(NodeProgram::declare_compiler_variables());
//	m_program->global_declarations->append_body(declare_compiler_variables());
	m_program->global_declarations->append_body(std::move(m_global_variable_declarations));
}

void ASTDesugar::visit(NodeBlock& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
	node.flatten_body();
}

void ASTDesugar::visit(NodeFunctionDefinition& node) {
	node.header->accept(*this);
	node.body->accept(*this);
	if(auto desugaring = node.get_desugaring(m_program)) {
		node.accept(*desugaring);
	}
}

void ASTDesugar::visit(NodeDeclaration& node) {
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugar::visit(NodeSingleDeclaration& node) {
    node.variable->accept(*this);
    if(node.value) node.value->accept(*this);

    if(node.variable->is_global) {
        m_global_variable_declarations->statements.push_back(
			std::make_unique<NodeStatement>(
				node.clone(),
				node.tok
			)
		);
		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		return;
    }
}

void ASTDesugar::visit(NodeAssignment &node) {
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        desugaring->replacement_node->accept(*this);
        node.replace_with(std::move(desugaring->replacement_node));
    }
}

void ASTDesugar::visit(NodeForEach& node) {
    node.body->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        // accept again to desugar resulting for loops
        replacement->accept(*this);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugar::visit(NodeFor& node) {
    node.body->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
        // move replacement to this visitor in case of nested for loops
        auto replacement = std::move(desugaring->replacement_node);
        node.replace_with(std::move(replacement));
    }
}

void ASTDesugar::visit(NodeFamily &node) {
    node.members->accept(*this);
    if(auto desugaring = node.get_desugaring(m_program)) {
        node.accept(*desugaring);
    }
    node.replace_with(std::move(node.members));
}

void ASTDesugar::visit(NodeParamList &node) {
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
}

void ASTDesugar::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	if(auto desugaring = node.get_desugaring(m_program)) {
		node.accept(*desugaring);
	}
}


