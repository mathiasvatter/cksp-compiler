//
// Created by Mathias Vatter on 20.04.24.
//

#include "ASTCollectLowerings.h"
#include "../Lowering/LoweringStruct.h"
#include "../Lowering/LoweringTernaryOperator.h"
#include "../Lowering/LoweringArrayQuery.h"
#include "../Lowering/PreLoweringStruct.h"
#include "../Lowering/LoweringBoolean.h"
#include "../Lowering/LoweringBooleanExpression.h"
#include "../Lowering/LoweringOptionalChaining.h"
#include "FunctionHandling/UIControlParamHandling.h"

NodeAST * ASTCollectLowerings::visit(NodeProgram& node) {
	m_program = &node;
	m_program->function_definition_stack = {};

	// move all namespaces into global declarations block before inlining them in visitor
	// for (auto& ns : node.namespaces) {
	// 	m_program->global_declarations->add_as_stmt(std::move(ns));
	// }
	// node.namespaces.clear();

	for(const auto & struct_def : node.struct_definitions) {
		static PreLoweringStruct pre_lowering_struct(m_program);
		pre_lowering_struct.set_program(m_program);
		struct_def->accept(pre_lowering_struct);
	}
	for(const auto & struct_def : node.struct_definitions) {
		struct_def->generate_ref_count_methods(m_program);
	}
	node.update_function_lookup();
	for(const auto & struct_def : node.struct_definitions) {
		static LoweringStructMembers lowering_struct_members(m_program);
		lowering_struct_members.set_program(m_program);
		struct_def->accept(lowering_struct_members);
	}
	for(const auto & struct_def : node.struct_definitions) {
		struct_def->lower(m_program);
	}
	node.debug_print();

	m_program->global_declarations->accept(*this);
	// visit_all(node.struct_definitions, *this);
	for(const auto & callback : node.callbacks) {
		callback->accept(*this);
	}
	m_program->function_definition_stack = {};
	// Merge function here before visiting them again so that newly added functions (to additional_functions)
	// are also visited and lowered -> ternary functions -> short-circuiting
	node.merge_function_definitions();
	for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.reset_function_visited_flag();


	node.struct_definitions.clear();
	node.update_struct_lookup();
	// No table hands out a struct pointer anymore, so the lowered nodes can go.
	node.lowered_structs.clear();
	// node.reset_function_visited_flag();
	node.global_declarations->prepend_body(NodeStruct::declare_struct_constants());
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
	bool_lowering.set_program(m_program);
	return node.accept(bool_lowering);
}

NodeAST * ASTCollectLowerings::visit(NodeStruct& node) {
	//TRACE();
	node.members->accept(*this);
	visit_all(node.methods, *this);
	node.inline_struct(m_program);
	// program->global_declarations->append_body(std::move(members));
	// // program->init_callback->statements->prepend_body(std::move(members));
	// The struct leaves the AST here but stays alive until every struct is lowered: the
	// callbacks and functions below still resolve members and constructors through
	// <struct_lookup>, which keeps handing out this pointer until <visit(NodeProgram)>
	// clears it.
	return m_program->retire_lowered_struct(node);
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
	return &node;
}

NodeAST * ASTCollectLowerings::visit(NodeSingleDeclaration &node) {
	//TRACE();
	node.check_constant_initialization();
	node.variable->accept(*this);
	if(node.value) node.value->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeSingleAssignment& node) {
	//TRACE();
	// obj?.member := value is wrapped into a nil-guard before the chain lowering runs
	LoweringOptionalChaining opt_chaining(m_program);
	if (const auto new_node = node.accept(opt_chaining); new_node != &node) {
		return new_node->accept(*this);
	}
	node.r_value->accept(*this);
	node.l_value->accept(*this);
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


/// Moves an initializer list handed to a call into a local array declared in front of it. The
/// array is a copy of the formal parameter: the callee copies as many elements as its parameter
/// declares, and a list of its own shape would leave that copy reading past the end. This has to
/// happen before <ASTLowerTypes>, where the object element type turns into an integer one and an
/// empty slot is no longer tellable from object 0.
void ASTCollectLowerings::hoist_initializer_list_arguments(NodeFunctionCall& node) {
	const auto definition = node.get_definition();
	if (!definition or node.is_builtin_kind()) return;
	const auto statement = node.get_parent_statement();
	if (!statement) return;

	std::vector<std::unique_ptr<NodeSingleDeclaration>> declarations;
	const int param_offset = node.get_param_offset(definition.get());
	for (int argument = 0; argument < node.function->get_num_args(); argument++) {
		if (!node.function->get_arg(argument)->cast<NodeInitializerList>()) continue;
		const int param_index = argument + param_offset;
		if (param_index < 0 or param_index >= definition->header->get_num_params()) continue;
		auto declaration = declare_argument_list(
			unique_ptr_cast<NodeInitializerList>(std::move(node.function->get_arg(argument))),
			*definition->header->get_param(param_index));
		node.function->set_arg(argument, declaration->variable->to_reference());
		declarations.push_back(std::move(declaration));
	}
	if (declarations.empty()) return;

	auto body = std::make_unique<NodeBlock>(statement->tok);
	// A declaration keeps its variable visible after the statement, so this block must not open a
	// scope around it - the same reason ReturnFunctionCallHoisting has for it.
	body->scope = statement->statement->cast<NodeSingleDeclaration>() == nullptr;
	const size_t num_declarations = declarations.size();
	for (auto& declaration : declarations) {
		body->add_as_stmt(std::move(declaration));
	}
	body->add_as_stmt(std::move(statement->statement));
	statement->set_statement(std::move(body));

	// The statement this pass is currently walking is behind them now, so the declarations get
	// their turn here - they are lowered like any other one, which is where an array without a
	// declared size takes it from the list.
	const auto hoisted = statement->statement->cast<NodeBlock>();
	for (size_t declaration = 0; declaration < num_declarations; ++declaration) {
		hoisted->statements[declaration]->accept(*this);
	}
}

std::unique_ptr<NodeSingleDeclaration> ASTCollectLowerings::declare_argument_list(
		std::unique_ptr<NodeInitializerList> init_list, const NodeDataStructure& parameter) {
	auto array = clone_as<NodeDataStructure>(&const_cast<NodeDataStructure&>(parameter));
	array->name = m_def_provider->get_fresh_name("_arr");
	array->is_local = true;
	array->kind = NodeDataStructure::Kind::Throwaway;
	const auto tok = init_list->tok;
	return std::make_unique<NodeSingleDeclaration>(std::move(array), std::move(init_list), tok);
}

NodeAST * ASTCollectLowerings::visit(NodeFunctionCall& node) {
	//TRACE();
	node.function->accept(*this);
	node.bind_definition(m_program, true);
	hoist_initializer_list_arguments(node);
	if (const auto& definition = node.get_definition()) {
		if(!definition->visited) {
			FunctionCallStackScope diagnostic_frame(*m_program, node);
			m_program->function_definition_stack.emplace(definition);
			definition->accept(*this);
			m_program->function_definition_stack.pop();
		}
		definition->visited = true;
	}
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeArray& node) {
	//TRACE();
    if(node.size) node.size->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return node.lower(m_program);
}

NodeAST * ASTCollectLowerings::visit(NodeNDArray& node) {
	//TRACE();
	if(node.sizes) node.sizes->accept(*this);
	if(node.num_elements) node.num_elements->accept(*this);
	return node.lower(m_program);
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
	LoweringOptionalChaining opt_chaining(m_program);
	const auto new_node = node.accept(opt_chaining);
	//TRACE();
	return new_node->lower(m_program)->accept(*this);
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
	ternary.set_program(m_program);
	return node.accept(ternary);
}

NodeAST * ASTCollectLowerings::visit(NodeArrayQuery& node) {
	static LoweringArrayQuery array_query(m_program);
	array_query.set_program(m_program);
	return node.accept(array_query)->accept(*this);
}

NodeAST * ASTCollectLowerings::visit(NodeNullCoalesce &node) {
	// only lower the fallback here: the chain has to stay untouched so the
	// nullish coalescing lowering can build the nil guards from it
	node.fallback->accept(*this);
	static LoweringNullCoalescing coalesce(m_program);
	coalesce.set_program(m_program);
	const auto call = node.accept(coalesce);
	// the generated function body still contains the raw chain -> lower it now
	if (const auto func_call = call->cast<NodeFunctionCall>()) {
		if (const auto def = func_call->get_definition()) {
			def->body->accept(*this);
		}
	}
	return call;
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
	bool_expr_lowering.set_program(m_program);
	return bool_expr_lowering.lower_expression(node);
}

NodeAST * ASTCollectLowerings::visit(NodeUnaryExpr &node) {
	ASTVisitor::visit(node);
	static LoweringBooleanExpression bool_expr_lowering(m_program);
	bool_expr_lowering.set_program(m_program);
	return bool_expr_lowering.lower_expression(node);
}
