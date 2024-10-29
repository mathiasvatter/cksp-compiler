//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTSemanticAnalysis.h"

ASTSemanticAnalysis::ASTSemanticAnalysis(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

NodeAST * ASTSemanticAnalysis::visit(NodeProgram& node) {
    m_program = &node;

	m_program->global_declarations->accept(*this);
	for(auto & s : node.struct_definitions) {
		s->accept(*this);
	}
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
	// visit func defs that are not called. because of replacing incorrectly node params with array
    for(auto & func_def : node.function_definitions) {
		if(!func_def->visited) func_def->accept(*this);
	}
	node.reset_function_visited_flag();
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeWildcard& node) {
	if(!node.check_semantic()) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		error.m_message = "Wildcard is not allowed in this context";
		error.exit();
	}
	return &node;
}

/// check if declared constant variable ref gets new assignment -> throw error
NodeAST* ASTSemanticAnalysis::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
	if(node.l_value->get_node_type() == NodeType::VariableRef) {
		auto ref = static_cast<NodeVariableRef*>(node.l_value.get());
		if(ref->data_type == DataType::Const) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Cannot reassign value to constant variable.";
			error.exit();
		}
	}
	node.r_value->accept(*this);
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeCallback& node) {
	m_program->current_callback = &node;
	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);
	m_program->current_callback = nullptr;
	return &node;
}


NodeAST * ASTSemanticAnalysis::visit(NodeBlock &node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionDefinition &node) {
	m_program->function_call_stack.push(&node);
	node.visited = true;
    node.header ->accept(*this);
    if (node.return_variable.has_value())
        node.return_variable.value()->accept(*this);
    node.body->accept(*this);
	m_program->function_call_stack.pop();
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionVarRef& node) {
	if(node.header) node.header->accept(*this);
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->declaration);
//	if(new_node->declaration and new_node->declaration->get_node_type() == NodeType::FunctionHeader
//	and new_node->get_node_type() == NodeType::FunctionVarRef) {
//		auto func_var_ref = static_cast<NodeFunctionVarRef*>(new_node);
//		auto func_declaration = static_cast<NodeFunctionHeader*>(new_node->declaration);
//		if(func_var_ref->header->args->params.empty()) {
//			func_var_ref->header->args = clone_as<NodeParamList>(func_declaration->args.get());
//			func_var_ref->header->args->parent = func_var_ref->header.get();
//		} else if(func_declaration->args->params.empty()) {
//			func_declaration->args = clone_as<NodeParamList>(func_var_ref->header->args.get());
//			func_declaration->args->parent = func_declaration;
//		}
//	}
//	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionCall& node) {
	node.function->accept(*this);

	node.get_definition(m_program);

	if(node.kind == NodeFunctionCall::UserDefined and node.definition) {
		if(!node.definition->visited) node.definition->accept(*this);
	}

	// determine thread safety of currently visiting function definition
	if(node.definition) {
		if(!m_program->function_call_stack.empty()) {
			m_program->function_call_stack.top()->is_thread_safe &= node.definition->is_thread_safe;
		}
		if(m_program->current_callback) m_program->current_callback->is_thread_safe &= node.definition->is_thread_safe;
	}
	// determine if currently visiting function in stack is restricted
	if(node.definition) {
		if(!m_program->function_call_stack.empty()) {
			m_program->function_call_stack.top()->is_restricted &= node.definition->is_restricted;
		}
	}

	// if definition parameters of this function have different node types as the call site -> update
	update_func_call_node_types(&node);
	node.function->accept(*this);
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeArray &node) {
	if(node.size) node.size->accept(*this);
	return replace_incorrectly_detected_data_struct(&node);
}

NodeAST * ASTSemanticAnalysis::visit(NodeArrayRef &node) {
    if(node.index) node.index->accept(*this);
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->declaration);
}

NodeAST * ASTSemanticAnalysis::visit(NodeNDArray& node) {
	if(node.sizes) node.sizes->accept(*this);
	return replace_incorrectly_detected_data_struct(&node);
}

NodeAST * ASTSemanticAnalysis::visit(NodeNDArrayRef& node) {
    if(node.indexes) node.indexes->accept(*this);

	if (!node.determine_sizes()) {
		NodeReference *new_node = &node;
		if (auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
			new_node = repl;
			new_node->accept(*this);
		}
		return replace_incorrectly_detected_data_struct(new_node->declaration);
	}

	// check if indices have same size as dimensions of declaration
	if(node.indexes and node.indexes->params.size() != static_cast<NodeNDArray*>(node.declaration)->dimensions) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		error.m_message = "Number of indices does not match number of dimensions: " + node.name;
		error.exit();
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeVariable &node) {
	return replace_incorrectly_detected_data_struct(&node);
}

NodeAST * ASTSemanticAnalysis::visit(NodeVariableRef &node) {
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->declaration);
}

NodeAST * ASTSemanticAnalysis::visit(NodePointer &node) {
	return replace_incorrectly_detected_data_struct(&node);
}

NodeAST * ASTSemanticAnalysis::visit(NodePointerRef &node) {
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->declaration);
}

NodeAST * ASTSemanticAnalysis::visit(NodeList& node) {
	for(auto &params : node.body) {
		params->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(&node);
}

NodeAST * ASTSemanticAnalysis::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(m_def_provider, &node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->declaration);
}

void ASTSemanticAnalysis::update_func_call_node_types(NodeFunctionCall* func_call) {
	if(func_call->kind == NodeFunctionCall::Kind::UserDefined and func_call->definition) {
		for(int i=0; i<func_call->function->get_num_args(); i++) {
			auto & arg = func_call->function->get_arg(i);
			auto & param = func_call->definition->get_arg(i);
			if(arg->get_node_type() == NodeType::VariableRef and param->get_node_type() == NodeType::Array) {
				auto node_var_ref = static_cast<NodeVariableRef*>(arg.get());
				if(!node_var_ref->declaration) return;
				auto node_array_ref = std::make_unique<NodeArrayRef>(
					node_var_ref->name,
					nullptr,
					node_var_ref->tok);
				node_array_ref->match_data_structure(node_var_ref->declaration);
				m_def_provider->remove_reference(node_var_ref->declaration, node_var_ref);
				auto new_ref = static_cast<NodeReference*>(node_var_ref->replace_with(std::move(node_array_ref)));
				m_def_provider->add_reference(new_ref->declaration, new_ref);
			// problem when desugaring return stmts with multiple return variables -> all get to be variables
			} else if (arg->get_node_type() == NodeType::ArrayRef and param->get_node_type() == NodeType::Variable) {
				auto node_ret_var = static_cast<NodeVariable*>(param.get());
				// only permissible if return variable
				if(node_ret_var->data_type == DataType::Return) {
					auto node_array = node_ret_var->to_array(nullptr);
					auto new_declaration = static_cast<NodeDataStructure*>(node_ret_var->replace_with(std::move(node_array)));
					std::unordered_set<NodeReference*> new_references;
					for(auto & ref : m_def_provider->get_references(node_ret_var)) {
						auto new_ref = static_cast<NodeReference*>(ref->replace_with(ref->to_array_ref(nullptr)));
						new_ref->declaration = new_declaration;
						new_references.emplace(new_ref);
					}
					m_def_provider->set_references(new_declaration, new_references);
					m_def_provider->remove_references(node_ret_var);
				}
			} else if (arg->get_node_type() == NodeType::NDArrayRef and param->get_node_type() == NodeType::Variable) {
				auto node_ret_var = static_cast<NodeVariable*>(param.get());
				// only permissible if return variable
				if(node_ret_var->data_type == DataType::Return) {
					auto node_ndarray = node_ret_var->to_ndarray();
					auto new_declaration = static_cast<NodeDataStructure*>(node_ret_var->replace_with(std::move(node_ndarray)));
					std::unordered_set<NodeReference*> new_references;
					for(auto & ref : m_def_provider->get_references(node_ret_var)) {
						auto new_ref = static_cast<NodeReference*>(ref->replace_with(ref->to_array_ref(nullptr)));
						new_ref->declaration = new_declaration;
						new_references.emplace(new_ref);
					}
					m_def_provider->set_references(new_declaration, new_references);
					m_def_provider->remove_references(node_ret_var);
				}
			} else if (arg->get_node_type() == NodeType::VariableRef and param->get_node_type() == NodeType::FunctionHeader) {
				auto node_var_ref = static_cast<NodeVariableRef*>(arg.get());
				auto func_var_ref = std::make_unique<NodeFunctionVarRef>(
					node_var_ref->name,
					std::make_unique<NodeFunctionHeader>(node_var_ref->name, std::make_unique<NodeParamList>(node_var_ref->tok), node_var_ref->tok),
					    node_var_ref->tok
					);
				func_var_ref->ty = node_var_ref->ty;
				func_var_ref->declaration = node_var_ref->declaration;
				node_var_ref->replace_reference(std::move(func_var_ref), m_def_provider);
			}
		}
	}
}

NodeDataStructure* ASTSemanticAnalysis::replace_incorrectly_detected_data_struct(NodeDataStructure* data_struct) {
	// if data struct is provided from declaration pointer
	if(!data_struct or data_struct->is_engine) return data_struct;
	// only replace function parameters
	if(!data_struct->is_function_param()) return data_struct;
//	auto data_type = data_struct->data_type;
	// check if references of this data struct are of different node type
	std::unordered_set<NodeType> reference_node_types;
	const std::unordered_set<NodeReference*>& references = m_def_provider->get_references(data_struct);
	const auto& ref_node_type = data_struct->get_ref_node_type();
	for(const auto & ref : references) {
		if(ref_node_type != ref->get_node_type())
			reference_node_types.emplace(ref->get_node_type());
	}
	// if reference node types are same as data struct -> return
	if(reference_node_types.empty()) return data_struct;

	std::unique_ptr<NodeDataStructure> new_data_struct = nullptr;
	// if some of the references were detected as Array References -> change data_struct type
	if(reference_node_types.find(NodeType::ArrayRef) != reference_node_types.end()) {
		auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			nullptr,
			data_struct->tok);
//		new_data_struct = static_cast<NodeDataStructure *>(data_struct->replace_with(std::move(node_array)));
		new_data_struct = std::move(node_array);
	// if some references were detected as ndarray refs
	} else if(reference_node_types.find(NodeType::NDArrayRef) != reference_node_types.end()) {
		auto node_ndarray = std::make_unique<NodeNDArray>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			nullptr,
			data_struct->tok);
		// get dimensions from references
		for(auto & ref : references) {
			if(ref->get_node_type() == NodeType::NDArrayRef) {
				auto nd_array_ref = static_cast<NodeNDArrayRef*>(ref);
				if(nd_array_ref->indexes) {
					node_ndarray->dimensions = nd_array_ref->indexes->params.size();
					break;
				}
			}
		}
//		new_data_struct = static_cast<NodeDataStructure*>(data_struct->replace_with(std::move(node_ndarray)));
		new_data_struct = std::move(node_ndarray);
	// if some references were detected as pointer references -> change data_struct type
	} else if(reference_node_types.find(NodeType::PointerRef) != reference_node_types.end()) {
		auto node_pointer = std::make_unique<NodePointer>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			data_struct->tok);
//		new_data_struct = static_cast<NodeDataStructure*>(data_struct->replace_with(std::move(node_pointer)));
		new_data_struct = std::move(node_pointer);
	} else if(reference_node_types.find(NodeType::FunctionVarRef) != reference_node_types.end()) {
		auto node_var = std::make_unique<NodeFunctionHeader>(
			data_struct->name,
			std::make_unique<NodeParamList>(data_struct->tok),
			data_struct->tok);
		node_var->ty = data_struct->ty;
//		new_data_struct = static_cast<NodeDataStructure*>(data_struct->replace_with(std::move(node_var)));
		new_data_struct = std::move(node_var);
	}

	if(new_data_struct) {
		new_data_struct->ty = data_struct->ty;
		new_data_struct->match_metadata(data_struct);


//		m_def_provider->set_references(new_data_struct, references);
//		// update all reference declaration pointers
//		for(auto & ref : references) {
//			ref->declaration = new_data_struct;
//		}
//		m_def_provider->remove_references(data_struct);
		return data_struct->replace_datastruct(std::move(new_data_struct), m_def_provider);
	}
	return data_struct;
}

NodeReference* ASTSemanticAnalysis::replace_incorrectly_detected_reference(DefinitionProvider* def_provider, NodeReference* reference) {
	if(!reference->declaration) return nullptr;
	if(reference->get_node_type() == reference->declaration->get_ref_node_type()) return nullptr;

	std::unique_ptr<NodeReference> node_replacement = nullptr;
	// if reference is variable ref but declaration is detected as array -> change ref to array ref
	if(reference->get_node_type() == NodeType::VariableRef and reference->declaration->get_node_type() == NodeType::Array) {
		node_replacement = std::make_unique<NodeArrayRef>(
			reference->name,
			nullptr,
			reference->tok);
	} else if(reference->get_node_type() == NodeType::VariableRef and reference->declaration->get_node_type() == NodeType::NDArray) {
		// check if raw array is wanted -> then it is ArrayRef
		if(reference->is_raw_array()) {
			node_replacement = std::make_unique<NodeArrayRef>(
				reference->name,
				nullptr,
				reference->tok);
		} else {
			node_replacement = std::make_unique<NodeNDArrayRef>(
				reference->name,
				nullptr,
				reference->tok);
		}
		// check if it is NodeListRef
	} else if(reference->get_node_type() == NodeType::ArrayRef and reference->declaration->get_node_type() == NodeType::List) {
		auto node_array_ref = static_cast<NodeArrayRef*>(reference);
		node_replacement = std::make_unique<NodeListRef>(
			reference->name,
			std::make_unique<NodeParamList>(reference->tok, std::move(node_array_ref->index)),
			reference->tok);
	} else if(reference->get_node_type() == NodeType::NDArrayRef and reference->declaration->get_node_type() == NodeType::List) {
		auto node_nd_array_ref = static_cast<NodeNDArrayRef*>(reference);
		node_replacement = std::make_unique<NodeListRef>(
			reference->name,
			std::move(node_nd_array_ref->indexes),
			reference->tok);
	// reference detected as variable ref but declaration is pointer
	} else if (reference->get_node_type() == NodeType::VariableRef and reference->declaration->get_node_type() == NodeType::Pointer) {
		node_replacement = std::make_unique<NodePointerRef>(
			reference->name,
			reference->tok);
	// if function param was changed to NDArray in here -> size constants have been replaced by access chains!
	} else if(reference->get_node_type() == NodeType::AccessChain and (reference->declaration->get_node_type() == NodeType::NDArray
	|| reference->declaration->get_node_type() == NodeType::Array || reference->declaration->get_node_type() == NodeType::List)) {
		auto access_chain = static_cast<NodeAccessChain*>(reference);
		if(access_chain->chain.size() == 2) {
			auto node_var_ref = std::make_unique<NodeVariableRef>(
				access_chain->get_string(),
				access_chain->tok);
			node_var_ref->declaration = static_cast<NodeReference*>(access_chain->chain[0].get())->declaration;
			node_var_ref->data_type = DataType::Const;
			if(node_var_ref->is_ndarray_constant() || node_var_ref->is_array_constant()) {
				node_replacement = std::move(node_var_ref);
				node_replacement->declaration = nullptr;
				auto new_ref = static_cast<NodeReference*>(reference->replace_with(std::move(node_replacement)));
				return new_ref;
			}
		}
	} else if (reference->get_node_type() == NodeType::VariableRef and reference->declaration->get_node_type() == NodeType::FunctionHeader) {
		node_replacement = std::make_unique<NodeFunctionVarRef>(
			reference->name,
			std::make_unique<NodeFunctionHeader>(
				reference->name,
				std::make_unique<NodeParamList>(reference->tok),
				reference->tok),
			reference->tok);
		node_replacement->ty = reference->ty;
	}

	if(node_replacement) {
		node_replacement->match_data_structure(reference->declaration);
		def_provider->remove_reference(reference->declaration, reference);
		auto new_ref = static_cast<NodeReference*>(reference->replace_with(std::move(node_replacement)));
		def_provider->add_reference(new_ref->declaration, new_ref);
		return new_ref;
	}
	return nullptr;
}




