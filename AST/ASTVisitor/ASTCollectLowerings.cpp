//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"
#include "ASTHandleStringRepresentations.h"
#include "../../Lowering/LoweringStruct.h"
#include "../../Lowering/PreLoweringStruct.h"
#include "FunctionHandling/UIControlParamHandling.h"

NodeAST * ASTCollectLowerings::visit(NodeProgram& node) {
	m_program = &node;
	m_program->global_declarations->accept(*this);
	for(auto & struct_def : node.struct_definitions) {
		static PreLoweringStruct pre_lowering_struct(m_program);
		struct_def->accept(pre_lowering_struct);
	}
	for(auto & struct_def : node.struct_definitions) {
		struct_def->generate_ref_count_methods(m_program);
	}
	for(auto & struct_def : node.struct_definitions) {
		static LoweringStructMembers lowering_struct_members(m_program);
		struct_def->accept(lowering_struct_members);
	}
	for(const auto & struct_def : node.struct_definitions) {
		struct_def->accept(*this);
	}
	for(const auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.merge_function_definitions();
	node.reset_function_visited_flag();

	ASTHandleStringRepresentations handle_string_representations(m_def_provider);
	node.accept(handle_string_representations);

	return &node;
}

NodeAST* ASTCollectLowerings::visit(NodeForEach& node) {
	if(node.key) node.key->accept(*this);
	if(node.value) node.value->accept(*this);
	node.range->accept(*this);
	node.body->accept(*this);
	// accept again to desugar resulting for loops
	return node.lower(m_program)->accept(*this);
}

NodeAST* ASTCollectLowerings::visit(NodeFor& node) {
	node.iterator->accept(*this);
	node.iterator_end->accept(*this);
	if(node.step) node.step->accept(*this);
	node.body->accept(*this);
	return node.lower(m_program)->accept(*this);
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
	node.variable->accept(*this);
	if(node.value) node.value->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleAssignment& node) {
	node.r_value->accept(*this);
	node.l_value->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeGetControl& node) {
	node.ui_id->accept(*this);
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
	node.bind_definition(m_program);
	if (const auto& definition = node.get_definition()) {
		if(!definition->visited) {
			static UIControlParamHandling ui_control_param_handling;
			ui_control_param_handling.handle_ui_params(*definition);
			definition->accept(*this);
		}
		definition->visited = true;
	}
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeArray& node) {
    if(node.size) node.size->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return node.data_lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeNDArray& node) {
	if(node.sizes) node.sizes->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNDArrayRef& node) {
	if(node.indexes) node.indexes->accept(*this);
	if(node.sizes) node.sizes->accept(*this);
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
	for(auto & b : node.body) {
		b->accept(*this);
	}
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodePointer& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodePointerRef& node) {
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeAccessChain& node) {
	return node.lower(m_program)->accept(*this);
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
	// node.get_nearest_loop();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNumElements& node) {
	node.array->accept(*this);
	if(node.dimension) node.dimension->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeUseCount& node) {
	node.ref->accept(*this);
	return node.lower(m_program);
}


NodeAST * ASTCollectLowerings::visit(NodeInitializerList &node) {
	for(auto & init : node.elements) {
		init->accept(*this);
	}
	return node.lower(m_program);
}



