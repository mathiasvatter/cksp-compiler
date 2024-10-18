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
//		// if global variable is const, check if it has a value and move it to global declarations
//		if(node.variable->data_type == DataType::Const) {
//			if(!node.value) {
//				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
//				error.m_message = "Found incorrect declare statement syntax. Const variables must be assigned a value.";
//				error.exit();
//			}
//			m_global_variable_declarations->add_stmt(std::make_unique<NodeStatement>(node.clone(), node.tok));
//			node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//		// if not const, move declaration to global declarations and make assignment, if no value is given, make it a dead code
//		} else {
//			std::unique_ptr<NodeAST> assignment = std::make_unique<NodeDeadCode>(node.tok);
//			if(node.value) {
//				assignment = std::make_unique<NodeSingleAssignment>(
//					node.variable->to_reference(),
//					std::move(node.value),
//					node.tok
//				);
//			}
//			auto declaration = std::make_unique<NodeSingleDeclaration>(
//				std::move(node.variable),
//				nullptr,
//				node.tok);
//			m_global_variable_declarations->add_stmt(std::make_unique<NodeStatement>(std::move(declaration), node.tok));
//			return node.replace_with(std::move(assignment));
//		}
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
//	// in case it is a double param_list [[0,1,2,3]] move params up to parent
//	if(node.parent->get_node_type() == NodeType::ParamList) {
//		auto node_param_list = static_cast<NodeParamList*>(node.parent);
//		// could be array initializer in function call
//		if(node_param_list->parent -> get_node_type() == NodeType::FunctionHeader) {
////			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
////			error.m_message = "Detected <array initializer> in function call. This is not yet allowed.";
////			error.exit();
//			return &node;
//		}
//		if(node_param_list->params.size() == 1) {
//			node_param_list->params.insert(
//				node_param_list->params.begin(),
//				std::make_move_iterator(node.params.begin()),
//				std::make_move_iterator(node.params.end())
//			);
//			node_param_list->params.pop_back();
//			node_param_list->set_child_parents();
//		}
//	}
	return node.desugar(m_program);
}

NodeAST* ASTDesugar::visit(NodeStruct& node) {
	node.members->accept(*this);
	for(auto & m: node.methods) {
		m->accept(*this);
	}
	return node.desugar(m_program);
}


