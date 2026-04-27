//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTSemanticAnalysis.h"

ASTSemanticAnalysis::ASTSemanticAnalysis(NodeProgram *main)
: m_def_provider(main->def_provider) {
	m_program = main;
}

NodeAST * ASTSemanticAnalysis::visit(NodeProgram& node) {
	m_program->current_callback = nullptr;
	m_program->global_declarations->accept(*this);
	visit_all(node.namespaces, *this);
	for(const auto & s : node.struct_definitions) {
		s->accept(*this);
	}
    for(const auto & callback : node.callbacks) {
        callback->accept(*this);
    }
	// visit func defs that are not called. because of replacing incorrectly node params with array
    for(const auto & func_def : node.function_definitions) {
		if(!func_def->visited) {
			m_program->function_call_stack.push(func_def->weak_from_this());
			func_def->accept(*this);
			m_program->function_call_stack.pop();
		}
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

NodeAST * ASTSemanticAnalysis::visit(NodeNumElements& node) {
	node.array->accept(*this);
	if(node.dimension) node.dimension->accept(*this);

	// transform var ref to composite type
	if(auto var = node.array->cast<NodeVariableRef>()) {
		auto node_array_ref = var->to_array_ref(nullptr);
		return var->replace_reference(std::move(node_array_ref));
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeSortSearch& node) {
	node.array->accept(*this);
	node.value->accept(*this);
	if(node.from) node.from->accept(*this);
	if(node.to) node.to->accept(*this);

	// transform var ref to composite type
	if(const auto var = node.array->cast<NodeVariableRef>()) {
		auto node_array_ref = var->to_array_ref(nullptr);
		return var->replace_reference(std::move(node_array_ref));
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeRange& node) {
	if(node.start) node.start->accept(*this);
	node.stop->accept(*this);
	if(node.step) node.step->accept(*this);

	node.check_environment();
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodePairs &node) {
	node.check_environment();
	return ASTVisitor::visit(node);
}

/// check if declared constant variable ref gets new assignment -> throw error
NodeAST* ASTSemanticAnalysis::visit(NodeSingleAssignment& node) {
	node.l_value->accept(*this);
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
	for(const auto & stmt : node.statements) {
		stmt->accept(*this);
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeSingleDeclaration &node) {
	node.variable->accept(*this);
	// check if static variable is class member
	if (node.variable->kind == NodeDataStructure::Kind::Static) {
		if (!node.variable->is_member()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<static> variables can only be declared as <struct> members in order for the fields to be used without instantiation.";
			error.exit();
		}
	}

	// due to old KSP code -> initializer lists in declarations are allowed with ()
	// could be misinterpreted by the parser as expressions and parsed without () -> hence initializer list
	// might have to be reinstantiated here when variable is array
	// declare array[] := (0)
	if (node.value) {
		if (node.variable->cast<NodeArray>() or node.variable->cast<NodeNDArray>()) {
			if (!node.value->cast<NodeInitializerList>() and !node.value->cast<NodeRange>()) {
				auto init_list = std::make_unique<NodeInitializerList>(node.tok, std::move(node.value));
				node.set_value(std::move(init_list));
			}
		}
		node.value->accept(*this);
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionDefinition &node) {
	m_functions_in_use.insert(&node);
	node.visited = true;
    node.header ->accept(*this);
    if (node.return_variable.has_value())
        node.return_variable.value()->accept(*this);
    node.body->accept(*this);
	m_functions_in_use.erase(&node);
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionHeaderRef& node) {
	if(node.args) node.args->accept(*this);
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(&node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->get_declaration());
}

NodeAST * ASTSemanticAnalysis::visit(NodeFunctionCall& node) {
	node.function->accept(*this);

	node.bind_definition(m_program);
	const auto definition = node.get_definition();
	// set has_exit_command of function definition node if we are in a function definition
	if (definition and node.is_builtin_kind() and !m_program->function_call_stack.empty()) {
		if (node.function->name == "exit") {
			auto func = m_program->function_call_stack.top().lock();
			func->has_exit_command = true;
		}
	}

	// visit the function definition
	if(definition and node.kind == NodeFunctionCall::UserDefined) {
		check_recursion(definition.get());
		if(!definition->visited) {
			m_program->function_call_stack.push(definition);
			// check all return paths
			definition->do_return_path_validation();
			definition->accept(*this);
			definition->sanitize_exit_commands();
			definition->visited = true;
			m_program->function_call_stack.pop();
		}
	}

	// checks for restricted functions and allowed callbacks -> throws error
	node.check_restricted_environment(m_program->current_callback);
	// if definition parameters of this function have different node types as the call site -> update
	update_func_call_node_types(&node);

	node.function->accept(*this);
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeArray &node) {
	if(node.size) node.size->accept(*this);
	return replace_incorrectly_detected_data_struct(node.get_shared());
}

NodeAST * ASTSemanticAnalysis::visit(NodeArrayRef &node) {
    if(node.index) node.index->accept(*this);
	NodeReference* new_node = &node;
	if(const auto repl = replace_incorrectly_detected_reference(&node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->get_declaration());
}

NodeAST * ASTSemanticAnalysis::visit(NodeString &node) {
	node.check_string_length();
	if (!node.is_valid_string()) {
		auto error = ASTVisitor::get_raw_compile_error(ErrorType::CompileError, node);
		error.add_message("Invalid string literal: " + node.value);
		error.add_message(". This might have been caused by faulty preprocessor macros.");
		error.exit();
	}
	// string values have to have " " on both sides -> ' ' is not permissible
	if (node.value[0] == '\'') {
		node.value = StringUtils::remove_quotes(node.value);
		node.value = StringUtils::add_double_quotes(node.value);
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeNDArray& node) {
	if(node.sizes) node.sizes->accept(*this);
	return replace_incorrectly_detected_data_struct(node.get_shared());
}

NodeAST * ASTSemanticAnalysis::visit(NodeNDArrayRef& node) {
    if(node.indexes) node.indexes->accept(*this);

	if (!node.determine_sizes()) {
		NodeReference *new_node = &node;
		if (const auto repl = replace_incorrectly_detected_reference(&node)) {
			new_node = repl;
			new_node->accept(*this);
		}
		return replace_incorrectly_detected_data_struct(new_node->get_declaration());
	}

	// check if indices have same size as dimensions of declaration
	if(node.indexes and node.indexes->params.size() != static_pointer_cast<NodeNDArray>(node.get_declaration())->dimensions) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		error.m_message = "Number of indices does not match number of dimensions: " + node.name;
		error.exit();
	}
	return &node;
}

NodeAST * ASTSemanticAnalysis::visit(NodeVariable &node) {
	return replace_incorrectly_detected_data_struct(node.get_shared());
}

NodeAST * ASTSemanticAnalysis::visit(NodeVariableRef &node) {
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(&node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	// checks for restricted functions and allowed callbacks -> throws error
	node.check_restricted_environment(m_program->current_callback);
	return replace_incorrectly_detected_data_struct(new_node->get_declaration());
}

NodeAST * ASTSemanticAnalysis::visit(NodePointer &node) {
	return replace_incorrectly_detected_data_struct(node.get_shared());
}

NodeAST * ASTSemanticAnalysis::visit(NodePointerRef &node) {
	NodeReference* new_node = &node;
	if(auto repl = replace_incorrectly_detected_reference(&node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->get_declaration());
}

NodeAST * ASTSemanticAnalysis::visit(NodeList& node) {
	for(auto &params : node.body) {
		params->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(node.get_shared());
}

NodeAST * ASTSemanticAnalysis::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	NodeReference* new_node = &node;
	if(const auto repl = replace_incorrectly_detected_reference(&node)) {
		new_node = repl;
		new_node->accept(*this);
	}
	return replace_incorrectly_detected_data_struct(new_node->get_declaration());
}

void ASTSemanticAnalysis::update_func_call_node_types(const NodeFunctionCall* func_call) {
	if(const auto definition = func_call->get_definition()) {
		for(int i=0; i<func_call->function->get_num_args(); i++) {
			const auto & arg = func_call->function->get_arg(i);
			const auto & param = definition->get_param(i);
			if(const auto node_var_ref = arg->cast<NodeVariableRef>(); node_var_ref and param->cast<NodeArray>()) {
//				if(!node_var_ref->get_declaration()) return;
				auto node_array_ref = std::make_unique<NodeArrayRef>(
					node_var_ref->name,
					nullptr,
					node_var_ref->tok);
				node_var_ref->replace_reference(std::move(node_array_ref));
			// problem when desugaring return stmts with multiple return variables -> all get to be variables
			} else if (arg->cast<NodeArrayRef>() and param->cast<NodeVariable>()) {
				// only permissible if return variable as function param
				if(param->data_type == DataType::Return) {
					auto node_array = param->to_array(nullptr);
					param->replace_datastruct(std::move(node_array));
				}
			} else if (arg->cast<NodeNDArrayRef>() and param->cast<NodeVariable>()) {
				// only permissible if return variable
				if(param->data_type == DataType::Return) {
					auto node_ndarray = param->to_ndarray();
					param->replace_datastruct(std::move(node_ndarray));
				}
			} else if (auto node_var_ref = arg->cast<NodeVariableRef>(); node_var_ref and param->cast<NodeFunctionHeader>()) {
				auto func_var_ref = std::make_unique<NodeFunctionHeaderRef>(
					node_var_ref->name,
					std::make_unique<NodeParamList>(node_var_ref->tok),
					node_var_ref->tok
					);
				func_var_ref->ty = node_var_ref->ty;
				node_var_ref->replace_reference(std::move(func_var_ref));
			}
		}
	}
}

NodeDataStructure* ASTSemanticAnalysis::replace_incorrectly_detected_data_struct(const std::shared_ptr<NodeDataStructure>& data_struct) {
	// if data struct is provided from declaration pointer
	if(!data_struct or data_struct->is_engine) return data_struct.get();
	// only replace function parameters
	if(!data_struct->is_function_param()) return data_struct.get();
	// check if references of this data struct are of different node type
	// if all reference nodeTypes are same as data struct -> return
	std::unordered_set<NodeType> ref_types;
	for(auto & ref : data_struct->references) {
		if(ref->get_node_type() != data_struct->get_ref_node_type())
			ref_types.insert(ref->get_node_type());
	}
	if(ref_types.empty()) return data_struct.get();

	// in the case of one wrong reference (being array type), all other references will be changed to array
	// check this when parameter is strongly typed as basic type
	// how to do that -> i dont know

	// check if ref_types has only two entries and one of them is VariableRef. Otherwise throw error
	// if(ref_types.size() > 1) {
	// 	// if (ref_types.size() == 2 and ref_types.contains(NodeType::ArrayRef) and ref_types.contains(NodeType::NDArrayRef))
	// 	static std::unordered_map<NodeType, std::string> ref_type_map = {
	// 		{NodeType::VariableRef, "VariableRef"},
	// 		{NodeType::ArrayRef, "ArrayRef"},
	// 		{NodeType::NDArrayRef, "NDArrayRef"},
	// 		{NodeType::PointerRef, "PointerRef"},
	// 		{NodeType::FunctionHeaderRef, "FunctionHeaderRef"}
	// 	};
	// 	std::unordered_set<NodeReference*> refs;
	// 	std::unordered_set<std::string> ref_names;
	// 	for(auto & ref : data_struct->references) {
	// 		if (ref->get_node_type() != NodeType::VariableRef) {
	// 			refs.insert(ref);
	// 			auto it = ref_type_map.find(ref->get_node_type());
	// 			if (it != ref_type_map.end()) {
	// 				ref_names.insert(it->second);
	// 			}
	// 		}
	// 	}
	// 	auto error = CompileError(ErrorType::SyntaxError, "", "", data_struct->tok);
	// 	error.m_message = "Found different types of reference for the same variable: " + data_struct->tok.val + ".";
	// 	for(const auto & ref : ref_names) {
	// 		error.m_got += " as <" + ref + ">, ";
	// 	}
	// 	error.m_got.erase(error.m_got.size() - 2);
	// 	error.exit();
	// }

	std::unique_ptr<NodeDataStructure> new_data_struct = nullptr;
	// if some of the references were detected as Array References -> change data_struct type
	if(ref_types.contains(NodeType::ArrayRef)) {
		auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			nullptr,
			data_struct->tok);
		new_data_struct = std::move(node_array);
	// if some references were detected as ndarray refs
	} else if(ref_types.contains(NodeType::NDArrayRef)) {
		auto node_ndarray = std::make_unique<NodeNDArray>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			nullptr,
			data_struct->tok);
		// get dimensions from references
		for(auto & ref : data_struct->references) {
			if(auto nd_array_ref = ref->cast<NodeNDArrayRef>()) {
				if(nd_array_ref->indexes) {
					node_ndarray->dimensions = nd_array_ref->indexes->params.size();
					break;
				}
			}
		}
		new_data_struct = std::move(node_ndarray);
	// if some references were detected as pointer references -> change data_struct type
	} else if(ref_types.contains(NodeType::PointerRef)) {
		auto node_pointer = std::make_unique<NodePointer>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			data_struct->tok);
		new_data_struct = std::move(node_pointer);
	} else if(ref_types.contains(NodeType::FunctionHeaderRef)) {
		auto node_var = std::make_unique<NodeFunctionHeader>(
			data_struct->name,
			data_struct->tok);
		node_var->ty = data_struct->ty;
		new_data_struct = std::move(node_var);
//	} else if(!ref_types.empty()) {
//		auto error = CompileError(ErrorType::SyntaxError, "", "", data_struct->tok);
//		error.m_message = "Reference type does not match declaration type: " + data_struct->name;
//		error.exit();
	}

	if(new_data_struct) {
		new_data_struct->ty = data_struct->ty;
		return data_struct->replace_datastruct(std::move(new_data_struct));
	}
	return data_struct.get();
}

NodeReference* ASTSemanticAnalysis::replace_incorrectly_detected_reference(NodeReference* reference) {
	const auto declaration = reference->get_declaration();
	if(!declaration) return nullptr;
	if(reference->get_node_type() == declaration->get_ref_node_type()) return nullptr;

	std::unique_ptr<NodeReference> node_replacement = nullptr;
	// if reference is variable ref but declaration is detected as array -> change ref to array ref
	if(reference->cast<NodeVariableRef>() and declaration->cast<NodeArray>()) {
		node_replacement = std::make_unique<NodeArrayRef>(
			reference->name,
			nullptr,
			reference->tok);
	} else if(reference->cast<NodeVariableRef>() and declaration->cast<NodeNDArray>()) {
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
	} else if(auto node_array_ref = reference->cast<NodeArrayRef>(); node_array_ref and declaration->cast<NodeList>()) {
		node_replacement = std::make_unique<NodeListRef>(
			reference->name,
			std::make_unique<NodeParamList>(reference->tok, std::move(node_array_ref->index)),
			reference->tok);
	} else if(auto node_nd_array_ref = reference->cast<NodeNDArrayRef>(); node_nd_array_ref and declaration->cast<NodeList>()) {
		node_replacement = std::make_unique<NodeListRef>(
			reference->name,
			std::move(node_nd_array_ref->indexes),
			reference->tok);
	// reference detected as variable ref but declaration is pointer
	} else if (reference->cast<NodeVariableRef>() and declaration->cast<NodePointer>()) {
		node_replacement = std::make_unique<NodePointerRef>(
			reference->name,
			reference->tok);
	} else if (reference->cast<NodeVariableRef>() and declaration->cast<NodeFunctionHeader>()) {
		node_replacement = std::make_unique<NodeFunctionHeaderRef>(
			reference->name,
			std::make_unique<NodeParamList>(reference->tok),
			reference->tok);
		node_replacement->ty = reference->ty;
	} else if(auto node_arr_ref = reference->cast<NodeArrayRef>(); node_array_ref and declaration->cast<NodeNDArray>()) {
		if (!node_arr_ref->is_raw_array()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", reference->tok);
			error.m_message =
				"<ArrayRef> was declared as <NDArray> but is no raw array. To reference a raw <NDArray> use '_' as prefix.";
			error.exit();
		}

	}

	if(node_replacement) {
		node_replacement->match_data_structure(declaration);
		const auto new_ref = reference->replace_reference(std::move(node_replacement));
		return new_ref;
	}
	return nullptr;
}




