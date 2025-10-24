//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"
#include "../../Lowering/LoweringStruct.h"
#include "../../Lowering/LoweringTernaryOperator.h"
#include "../../Lowering/PreLoweringStruct.h"
#include "../../Lowering/LoweringBoolean.h"
#include "../../Lowering/LoweringBooleanExpression.h"
#include "FunctionHandling/UIControlParamHandling.h"

NodeAST * ASTCollectLowerings::visit(NodeProgram& node) {
	m_program = &node;

	// move all namespaces into global declarations block before inlining them in visitor
	for (auto& ns : node.namespaces) {
		m_program->global_declarations->add_as_stmt(std::move(ns));
	}
	node.namespaces.clear();

	for(const auto & struct_def : node.struct_definitions) {
		static PreLoweringStruct pre_lowering_struct(m_program);
		struct_def->accept(pre_lowering_struct);
	}
	for(const auto & struct_def : node.struct_definitions) {
		struct_def->generate_ref_count_methods(m_program);
	}
	node.update_function_lookup();
	for(const auto & struct_def : node.struct_definitions) {
		static LoweringStructMembers lowering_struct_members(m_program);
		struct_def->accept(lowering_struct_members);
	}
	for(const auto & struct_def : node.struct_definitions) {
		struct_def->lower(m_program);
	}
	node.debug_print();

	m_program->global_declarations->accept(*this);
	visit_all(node.struct_definitions, *this);
	for(const auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	// Merge function here before visiting them again so that newly added functions (to additional_functions)
	// are also visited and lowered -> ternary functions -> short-circuiting
	node.merge_function_definitions();
	for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.reset_function_visited_flag();

	node.debug_print();

	return &node;
}

NodeAST* ASTCollectLowerings::visit(NodeForEach& node) {
	//TRACE();
	if(node.key) node.key->accept(*this);
	if(node.value) node.value->accept(*this);
	node.range->accept(*this);
	node.body->accept(*this);
	// accept again to desugar resulting for loops
	return node.lower(m_program)->accept(*this);
}

NodeAST* ASTCollectLowerings::visit(NodeFor& node) {
	//TRACE();
	node.iterator->accept(*this);
	node.iterator_end->accept(*this);
	if(node.step) node.step->accept(*this);
	node.body->accept(*this);
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeBlock& node) {
	//TRACE();
	visit_all(node.statements, *this);
	node.flatten();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNil& node) {
	//TRACE();
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeBoolean &node) {
	static LoweringBoolean bool_lowering(m_program);
	return node.accept(bool_lowering);
}

NodeAST * ASTCollectLowerings::visit(NodeStruct& node) {
	//TRACE();
	node.members->accept(*this);
	visit_all(node.methods, *this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeFunctionDefinition& node) {
	//TRACE();
	static UIControlParamHandling ui_control_param_handling;
	ui_control_param_handling.handle_ui_params(node);
	node.visited = true;

	node.header ->accept(*this);
	if (node.return_variable.has_value())
		node.return_variable.value()->accept(*this);
	node.body->accept(*this);

	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleDeclaration &node) {
	//TRACE();
	node.variable->accept(*this);
	if(node.value) node.value->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleAssignment& node) {
	//TRACE();
	node.r_value->accept(*this);
	node.l_value->accept(*this);
	node.check_for_constant_assignment();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeGetControl& node) {
	//TRACE();
	node.ui_id->accept(*this);
	// only handles get control
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeSetControl& node) {
	//TRACE();
	node.ui_id->accept(*this);
	node.value->accept(*this);
	// only handles get control
	return node.lower(m_program)->accept(*this);
}


NodeAST * ASTCollectLowerings::visit(NodeFunctionCall& node) {
	//TRACE();
	node.function->accept(*this);
	node.bind_definition(m_program, true);
	if (const auto& definition = node.get_definition()) {
		if(!definition->visited) {
			definition->accept(*this);
		}
		definition->visited = true;
	}
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeArray& node) {
	//TRACE();
    if(node.size) node.size->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return node.data_lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeNDArray& node) {
	//TRACE();
	if(node.sizes) node.sizes->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNDArrayRef& node) {
	//TRACE();
	if(node.indexes) node.indexes->accept(*this);
	if(node.sizes) node.sizes->accept(*this);
	return &node;
}


NodeAST * ASTCollectLowerings::visit(NodeArrayRef& node) {
	//TRACE();
	if(node.index) node.index->accept(*this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeVariableRef &node) {
	//TRACE();
	return ASTVisitor::visit(node);
}

NodeAST * ASTCollectLowerings::visit(NodeListRef& node) {
	//TRACE();
	node.indexes->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeList& node) {
	//TRACE();
	visit_all(node.body, *this);
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodePointer& node) {
	//TRACE();
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodePointerRef& node) {
	//TRACE();
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeAccessChain& node) {
	//TRACE();
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleRetain& node) {
	//TRACE();
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleDelete& node) {
	//TRACE();
	return node.lower(m_program)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeConst &node) {
	//TRACE();
    return node.replace_with(std::move(node.constants));
}

NodeAST * ASTCollectLowerings::visit(NodeNamespace &node) {
	//TRACE();
	ASTVisitor::visit(node);
	for(auto & func: node.function_definitions) {
		m_program->add_function_definition(func);
		// func->parent = m_program;
		// m_program->function_definitions.push_back(func);
	}
	node.function_definitions.clear();
	return node.replace_with(std::move(node.members));
}

NodeAST * ASTCollectLowerings::visit(NodeWhile& node) {
	//TRACE();
	return ASTVisitor::visit(node)->lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeIf &node) {
	//TRACE();
	ASTVisitor::visit(node);
	return node.do_short_circuit_transform(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeTernary &node) {
	ASTVisitor::visit(node);
	static LoweringTernaryOperator ternary(m_program);
	return node.accept(ternary);
}

NodeAST * ASTCollectLowerings::visit(NodeBreak& node) {
	//TRACE();
	// node.get_nearest_loop();
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeNumElements& node) {
	//TRACE();
	return ASTVisitor::visit(node);
}

NodeAST * ASTCollectLowerings::visit(NodeUseCount& node) {
	//TRACE();
	return ASTVisitor::visit(node)->lower(m_program);
}


NodeAST * ASTCollectLowerings::visit(NodeInitializerList &node) {
	//TRACE();
	return ASTVisitor::visit(node)->lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeRange &node) {
	//TRACE();
	return ASTVisitor::visit(node)->lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeBinaryExpr &node) {
	ASTVisitor::visit(node);
	static LoweringBooleanExpression bool_expr_lowering(m_program);
	return bool_expr_lowering.lower_expression(node);
}

NodeAST * ASTCollectLowerings::visit(NodeUnaryExpr &node) {
	ASTVisitor::visit(node);
	static LoweringBooleanExpression bool_expr_lowering(m_program);
	return bool_expr_lowering.lower_expression(node);
}



