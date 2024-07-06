//
// Created by Mathias Vatter on 23.04.24.
//

#include "ASTSemanticAnalysis.h"

ASTSemanticAnalysis::ASTSemanticAnalysis(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

void ASTSemanticAnalysis::visit(NodeProgram& node) {
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
		// reset visited flag
		func_def->visited = false;
	}

}

void ASTSemanticAnalysis::visit(NodeWildcard& node) {
	if(!node.check_semantic()) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		error.m_message = "Wildcard is not allowed in this context";
		error.exit();
	}
}


void ASTSemanticAnalysis::visit(NodeCallback& node) {
	m_program->current_callback = &node;
	if(node.callback_id) node.callback_id->accept(*this);
	node.statements->accept(*this);
	m_program->current_callback = nullptr;
}


void ASTSemanticAnalysis::visit(NodeBlock &node) {
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
	}
}

void ASTSemanticAnalysis::visit(NodeFunctionDefinition &node) {
	node.visited = true;
    node.header ->accept(*this);
    if (node.return_variable.has_value())
        node.return_variable.value()->accept(*this);
    node.body->accept(*this);
}

void ASTSemanticAnalysis::visit(NodeFunctionCall& node) {
	node.function->accept(*this);

	node.get_definition(m_program);

	if(node.kind == NodeFunctionCall::UserDefined and node.definition) {
		if(!node.definition->visited) node.definition->accept(*this);
	}
	if(!m_program->function_call_stack.empty()) {
		m_program->function_call_stack.top()->header->is_thread_safe &= node.function->is_thread_safe;
	}
	if(m_program->current_callback) m_program->current_callback->is_thread_safe &= node.function->is_thread_safe;
	if(m_program->current_callback) node.function->is_thread_safe &= m_program->current_callback->is_thread_safe;

	// if definition parameters of this function have different node types as the call site -> update
	update_func_call_node_types(&node);
	node.function->accept(*this);
}

void ASTSemanticAnalysis::visit(NodeArray &node) {
	if(node.size) node.size->accept(*this);
	replace_incorrectly_detected_data_struct(&node);
}

void ASTSemanticAnalysis::visit(NodeArrayRef &node) {
    if(node.index) node.index->accept(*this);
	replace_incorrectly_detected_reference(&node);
	replace_incorrectly_detected_data_struct(node.declaration);
}

void ASTSemanticAnalysis::visit(NodeNDArray& node) {
	if(node.sizes) node.sizes->accept(*this);
	replace_incorrectly_detected_data_struct(&node);
}

void ASTSemanticAnalysis::visit(NodeNDArrayRef& node) {
    if(node.indexes) node.indexes->accept(*this);

	if(!node.determine_sizes()) {
		replace_incorrectly_detected_reference(&node);
		replace_incorrectly_detected_data_struct(node.declaration);
//		CompileError(ErrorType::Variable, "Incorrectly recognized as <ndarray>: "+node.name, node.tok.line, "", node.name, node.tok.file).exit();
	}

	// check if indices have same size as dimensions of declaration
	if(node.indexes and node.indexes->params.size() != static_cast<NodeNDArray*>(node.declaration)->dimensions) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		error.m_message = "Number of indices does not match number of dimensions: " + node.name;
		error.exit();
	}
}

void ASTSemanticAnalysis::visit(NodeVariable &node) {
	replace_incorrectly_detected_data_struct(&node);
}

void ASTSemanticAnalysis::visit(NodeVariableRef &node) {
	replace_incorrectly_detected_reference(&node);
	replace_incorrectly_detected_data_struct(node.declaration);
}

void ASTSemanticAnalysis::visit(NodePointer &node) {
	replace_incorrectly_detected_data_struct(&node);
}

void ASTSemanticAnalysis::visit(NodePointerRef &node) {
	node.update_ptr_chain();
	replace_incorrectly_detected_reference(&node);
	replace_incorrectly_detected_data_struct(node.declaration);
}

void ASTSemanticAnalysis::visit(NodeList& node) {
	for(auto &params : node.body) {
		params->accept(*this);
	}
	replace_incorrectly_detected_data_struct(&node);
}

void ASTSemanticAnalysis::visit(NodeListRef& node) {
	node.indexes->accept(*this);
	replace_incorrectly_detected_reference(&node);
	replace_incorrectly_detected_data_struct(node.declaration);
}

void ASTSemanticAnalysis::update_func_call_node_types(NodeFunctionCall* func_call) {
	if(func_call->kind == NodeFunctionCall::Kind::UserDefined and func_call->definition) {
		for(int i=0; i<func_call->function->args->params.size(); i++) {
			auto & arg = func_call->function->args->params.at(i);
			auto & param = func_call->definition->header->args->params.at(i);
			if(arg->get_node_type() == NodeType::VariableRef and param->get_node_type() == NodeType::Array) {
				auto node_var_ref = static_cast<NodeVariableRef*>(arg.get());
				if(!node_var_ref->declaration) return;
				auto node_array_ref = std::make_unique<NodeArrayRef>(
					node_var_ref->name,
					nullptr,
					node_var_ref->tok);
				node_array_ref->match_data_structure(node_var_ref->declaration);
				auto new_ref = static_cast<NodeReference*>(node_var_ref->replace_with(std::move(node_array_ref)));
				m_def_provider->add_reference(new_ref->declaration, new_ref);
			// problem when desugaring return stmts with multiple return variables -> all get to be variables
			} else if (arg->get_node_type() == NodeType::ArrayRef and param->get_node_type() == NodeType::Variable) {
				auto node_ret_var = static_cast<NodeVariable*>(param.get());
				// only permissible if return variable
				if(node_ret_var->data_type == DataType::Return) {
					auto node_array = node_ret_var->to_array(nullptr);
					auto new_declaration = static_cast<NodeDataStructure*>(node_ret_var->replace_with(std::move(node_array)));
					std::set<NodeReference*> new_references;
					for(auto & ref : m_def_provider->get_references(node_ret_var)) {
						auto new_ref = static_cast<NodeReference*>(ref->replace_with(ref->to_array_ref(nullptr)));
						new_ref->declaration = new_declaration;
						new_references.emplace(new_ref);
					}
					m_def_provider->set_references(new_declaration, new_references);
				}
			} else if (arg->get_node_type() == NodeType::NDArrayRef and param->get_node_type() == NodeType::Variable) {
				auto node_ret_var = static_cast<NodeVariable*>(param.get());
				// only permissible if return variable
				if(node_ret_var->data_type == DataType::Return) {
					auto node_ndarray = node_ret_var->to_ndarray();
					auto new_declaration = static_cast<NodeDataStructure*>(node_ret_var->replace_with(std::move(node_ndarray)));
					std::set<NodeReference*> new_references;
					for(auto & ref : m_def_provider->get_references(node_ret_var)) {
						auto new_ref = static_cast<NodeReference*>(ref->replace_with(ref->to_array_ref(nullptr)));
						new_ref->declaration = new_declaration;
						new_references.emplace(new_ref);
					}
					m_def_provider->set_references(new_declaration, new_references);
				}
			}
		}
	}
}

void ASTSemanticAnalysis::replace_incorrectly_detected_data_struct(NodeDataStructure* data_struct) {
	// if data struct is provided from declaration pointer
	if(!data_struct or data_struct->is_engine) return;
	// only replace function parameters
	if(!data_struct->is_function_param()) return;
	// check if references of this data struct are of different node type
	std::set<NodeType> reference_node_types;
	std::set<NodeReference*> references = m_def_provider->get_references(data_struct);
	for(auto & ref : references) {
		if(data_struct->get_ref_node_type() != ref->get_node_type())
			reference_node_types.emplace(ref->get_node_type());
	}
	// if reference node types are same as data struct -> return
	if(reference_node_types.empty()) return;

	// if some of the references were detected as Array References -> change data_struct type
	if(reference_node_types.find(NodeType::ArrayRef) != reference_node_types.end()) {
		auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
            nullptr,
			data_struct->tok);
		auto new_data_struct = static_cast<NodeDataStructure*>(data_struct->replace_with(std::move(node_array)));
		m_def_provider->set_references(new_data_struct, references);
		// update all reference declaration pointers
		for(auto & ref : references) {
			ref->declaration = new_data_struct;
		}
	// if some references were detected as pointer references -> change data_struct type
	} else if(reference_node_types.find(NodeType::PointerRef) != reference_node_types.end()) {
		auto node_pointer = std::make_unique<NodePointer>(
			std::nullopt,
			data_struct->name,
			data_struct->ty,
			data_struct->tok);
		auto new_data_struct = static_cast<NodeDataStructure*>(data_struct->replace_with(std::move(node_pointer)));
		m_def_provider->set_references(new_data_struct, references);
		// update all reference declaration pointers
		for(auto & ref : references) {
			ref->declaration = new_data_struct;
		}
	}
}

void ASTSemanticAnalysis::replace_incorrectly_detected_reference(NodeReference* reference) {
	if(!reference->declaration) return;
	if(reference->get_node_type() == reference->declaration->get_ref_node_type()) return;

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
	}

	if(node_replacement) {
		node_replacement->match_data_structure(reference->declaration);
		auto new_ref = static_cast<NodeReference*>(reference->replace_with(std::move(node_replacement)));
		new_ref->accept(*this);
		m_def_provider->add_reference(new_ref->declaration, new_ref);
	}
}




