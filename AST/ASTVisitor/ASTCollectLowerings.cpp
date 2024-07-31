//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"
#include "ASTLowerTypes.h"

ASTCollectLowerings::ASTCollectLowerings(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTCollectLowerings::visit(NodeProgram& node) {
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
	node.merge_function_definitions();

	ASTLowerTypes lowering_types(m_def_provider);
	node.accept(lowering_types);
}

//void ASTCollectLowerings::visit(NodeReturn& node) {
//}


void ASTCollectLowerings::visit(NodeBlock& node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	node.flatten();
}

void ASTCollectLowerings::visit(NodeNil& node) {
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}


void ASTCollectLowerings::visit(NodeStruct& node) {
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
	node.members->accept(*this);
	for(auto &m : node.methods) {
		m->accept(*this);
	}
}

void ASTCollectLowerings::visit(NodeFunctionDefinition& node) {
	node.header ->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
};

void ASTCollectLowerings::visit(NodeSingleDeclaration &node) {
	if(node.value) node.value->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeSingleAssignment& node) {
	if(node.r_value) node.r_value->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	} else {
		node.l_value->accept(*this);
//		if(node.r_value) node.r_value->accept(*this);
	}
}

void ASTCollectLowerings::visit(NodeGetControl& node) {
	node.ui_id->accept(*this);

	/*
	 * checks if the parent of the current node is a single assignment statement.
	 * If the l_value of the assignment is the get_control statement, then it is a set_control statement.
	 * E.g. if it would be
	 * 	int_buffer := mnu_output[i+1]->x, this block of code would need to handle it.
	 * However, in the case of:
	 * 	mnu_output[i+1]->x := int_buffer, the NodeSingleAssignment lowering would handle it.
	 */
	// is set control
	if(node.parent->get_node_type() == NodeType::SingleAssignment) {
		if(auto node_assign_stmt = static_cast<NodeSingleAssignment*>(node.parent)) {
			if(node_assign_stmt->l_value.get() == &node) return;
		}
	}

	// only handles get control
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeFunctionCall& node) {
	node.function->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeArray& node) {
    if(node.size) node.size->accept(*this);
    if(auto lowering = node.get_lowering(m_program)) {
        node.accept(*lowering);
    }
}

void ASTCollectLowerings::visit(NodeNDArray& node) {
	if(node.parent->get_node_type() == NodeType::SingleAssignment) {
		return;
	}
	if(node.sizes) node.sizes->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeList& node) {
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodePointerRef& node) {
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
}

void ASTCollectLowerings::visit(NodeAccessChain& node) {
	if(auto lowering = node.get_lowering(m_program)) {
		node.accept(*lowering);
	}
	node.chain.back()->accept(*this);
	node.replace_with(std::move(node.chain.back()));
}


void ASTCollectLowerings::visit(NodeConst &node) {
    node.replace_with(std::move(node.constants));
}

//void ASTCollectLowerings::visit(NodeFamily &node) {
//    node.members->accept(*this);
//    if(auto lowering = node.get_lowering()) {
//        node.accept(*lowering);
//    }
//    node.replace_with(std::move(node.members));
//}




