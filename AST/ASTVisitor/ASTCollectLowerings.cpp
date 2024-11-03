//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"
#include "ASTLowerTypes.h"
#include "ASTHandleStringRepresentations.h"

ASTCollectLowerings::ASTCollectLowerings(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

NodeAST * ASTCollectLowerings::visit(NodeProgram& node) {
	m_program = &node;
	m_program->global_declarations->accept(*this);
	for(auto & struct_def : node.struct_definitions) {
		struct_def->pre_lower(m_program);
	}
	for(auto & struct_def : node.struct_definitions) {
		struct_def->generate_ref_count_methods();
	}
//	node.struct_definitions.at(0)->debug_print();
	for(auto & struct_def : node.struct_definitions) {
		struct_def->accept(*this);
	}
	for(auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.merge_function_definitions();
	node.reset_function_visited_flag();

	ASTHandleStringRepresentations handle_string_representations(m_def_provider);
	node.accept(handle_string_representations);

	ASTLowerTypes lowering_types(m_def_provider);
	node.accept(lowering_types);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeBlock& node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	node.flatten();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNil& node) {
	return node.lower(m_program);
}


NodeAST * ASTCollectLowerings::visit(NodeStruct& node) {
	node.lower(m_program);
	node.members->accept(*this);
	for(auto &m : node.methods) {
		m->accept(*this);
	}
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeFunctionDefinition& node) {
	node.header ->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleDeclaration &node) {
	if(node.value) node.value->accept(*this);
	if(node.variable -> get_node_type() == NodeType::NDArray || node.variable -> get_node_type() == NodeType::Array) {
		return &node;
	}
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleAssignment& node) {
	if(node.r_value -> get_node_type() != NodeType::NDArrayRef) {
		node.r_value->accept(*this);
	}
	if(node.l_value -> get_node_type() == NodeType::NDArrayRef) {
		return &node;
	}
	if(auto lowering = node.get_lowering(m_program)) {
		return node.accept(*lowering)->accept(*this);
	} else {
		node.l_value->accept(*this);
//		if(node.r_value) node.r_value->accept(*this);
		return &node;
	}
}

NodeAST * ASTCollectLowerings::visit(NodeGetControl& node) {
	node.ui_id->accept(*this);

//	/*
//	 * checks if the parent of the current node is a single assignment statement.
//	 * If the l_value of the assignment is the get_control statement, then it is a set_control statement.
//	 * E.g. if it would be
//	 * 	int_buffer := mnu_output[i+1]->x, this block of code would need to handle it.
//	 * However, in the case of:
//	 * 	mnu_output[i+1]->x := int_buffer, the NodeSingleAssignment lowering would handle it.
//	 */
//	// is set control
//	if(node.parent->get_node_type() == NodeType::SingleAssignment) {
//		if(auto node_assign_stmt = static_cast<NodeSingleAssignment*>(node.parent)) {
//			if(node_assign_stmt->l_value.get() == &node) return &node;
//		}
//	}

	// only handles get control
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeSetControl& node) {
	node.ui_id->accept(*this);
	node.value->accept(*this);

	// only handles get control
	return node.lower(m_program)->accept(*this);
}


NodeAST * ASTCollectLowerings::visit(NodeFunctionCall& node) {
	node.function->accept(*this);
	if(node.get_definition(m_program)) {
		if(!node.definition->visited) node.definition->accept(*this);
		node.definition->visited = true;
	}
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeArray& node) {
    if(node.size) node.size->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeNDArray& node) {
	if(node.parent->get_node_type() == NodeType::SingleAssignment) {
		return &node;
	}
	if(node.sizes) node.sizes->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeArrayRef& node) {
	if(node.index) node.index->accept(*this);
	return &node;
};

NodeAST * ASTCollectLowerings::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeList& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodePointer& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodePointerRef& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeAccessChain& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleRetain& node) {
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleDelete& node) {
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeConst &node) {
    return node.replace_with(std::move(node.constants));
}

NodeAST * ASTCollectLowerings::visit(NodeWhile& node) {
	node.condition->accept(*this);
	node.body->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeBreak& node) {
	node.get_nearest_loop();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNumElements& node) {
	node.array->accept(*this);
	if(node.dimension) node.dimension->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeUseCount& node) {
	node.ref->accept(*this);
	return node.lower(m_program);
}


NodeAST * ASTCollectLowerings::visit(NodeInitializerList &node) {
	node.flatten();
	for(auto & init : node.elements) {
		init->accept(*this);
	}
	return &node;
}



