//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTVariableChecking final : public ASTVisitor {
public:
	explicit ASTVariableChecking(NodeProgram* main);

	NodeAST* do_complete_traversal(NodeProgram& node, const bool fail) {
		this->fail = fail;
		// update function lookup map because of altered param counts after lambda lifting
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// erase all previously saved scopes
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_data_vectors();
        node.accept(*this);
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		m_def_provider->refresh_scopes();
		return &node;
    }

	NodeAST* do_reachable_traversal(NodeProgram& node, const bool fail) {
		this->fail = fail;
		// update function lookup map because of altered param counts after lambda lifting
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// erase all previously saved scopes
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_data_vectors();
		node.accept(*this);
		node.reset_function_visited_flag();
		m_def_provider->refresh_scopes();
		return &node;
	}

	NodeAST * visit(NodeProgram& node) override;
	/// check if on init callback currently
	NodeAST * visit(NodeCallback& node) override;
	/// Check for correct variable types and parameter number
	NodeAST * visit(NodeUIControl& node) override;
	/// Scoping
	NodeAST * visit(NodeBlock& node) override;
    /// decide if declaration is local or global
	NodeAST * visit(NodeSingleDeclaration& node) override;
	/// check for reassignments of Function Parameters that are immutable
	NodeAST * visit(NodeSingleAssignment& node) override;
	/// Check if correctly declared and save declaration
	NodeAST * visit(NodeArray& node) override;
    /// get declaration
	NodeAST * visit(NodeArrayRef& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	NodeAST * visit(NodeVariable& node) override;
    /// get declaration
	NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	NodeAST * visit(NodeNDArray& node) override;
	NodeAST * visit(NodeNDArrayRef& node) override;
	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;
	NodeAST * visit(NodeList& node) override;
	NodeAST * visit(NodeListRef& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	NodeAST * visit(NodeFunctionCall& node) override;
    NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;

	NodeAST * visit(NodeAccessChain& node) override;

	NodeAST * visit(NodeForEach& node) override;
	NodeAST * visit(NodeConst& node) override;
	NodeAST * visit(NodeStruct& node) override;

private:
	// boolean to continue after not finding declaration or fail
	bool fail = false;
	NodeStruct* m_current_struct = nullptr;
    std::stack<NodeBlock*> m_current_block;
	DefinitionProvider* m_def_provider = nullptr;

	NodeBlock* get_current_block() const {
		if (m_current_block.empty()) return nullptr;
		return m_current_block.top();
	}

	/// track functions in use to search for recursive calls
	std::unordered_set<NodeFunctionDefinition*> m_functions_in_use;
	bool check_recursion(NodeFunctionDefinition* func) const {
		if(m_functions_in_use.contains(func)) {
			// recursive function call detected
			auto error = CompileError(ErrorType::SyntaxError, "", "", func->tok);
			error.m_message = "Recursive function call detected. Calling functions inside their definition is not allowed.";
			error.m_got = func->header->name;
			error.exit();
			return true;
		}
		return false;
	}

	/// node can be NodeFunctionCall or NodeReference
	/// transformation when first object is clearly a reference this_list.next.next()
	/// tries to get declaration of first object and if there is one, replaces it with method chain
	std::unique_ptr<NodeAccessChain> try_access_chain_transform(const std::string& name, NodeAST* node) const {
		// find object ptr name
		const size_t pos = name.find('.');
		if (pos == std::string::npos) {
			return nullptr;
		}
		const auto ptr_name = name.substr(0, pos);
		const auto node_declaration = m_def_provider->get_declared_data_structure(ptr_name);
		if(!node_declaration) return nullptr;

		// different scenarios for different node types
		// eq.lbl_param0 -> a reference originally recognized as a variable cannot have a variable or function declaration (eq)
		if(node->cast<NodeVariableRef>()) {
			if(node_declaration->cast<NodeFunctionHeader>()) {
				return nullptr;
			}
		}

		auto method_chain = node->to_method_chain();
		if(!method_chain) return nullptr;
		const auto object = static_cast<NodeReference*>(method_chain->chain[0].get());
		object->declaration = node_declaration;
		method_chain->declaration = node_declaration;
		return method_chain;
	}

	/// checks if given callback id is of type ui_control
	static bool check_callback_id_data_type(NodeAST* callback_id) {
		std::string id_node_type = "<Array>";
		if(callback_id->get_node_type() == NodeType::VariableRef) {
			id_node_type = "<Variable>";
		}
		const auto node_reference = static_cast<NodeReference*>(callback_id);
		// return prematurely if no declaration yet provided
		const auto declaration = node_reference->get_declaration();
		if(!declaration) return false;
		// check if callback id reference is ui_control
		auto error = CompileError(ErrorType::TypeError, "", "", callback_id->tok);
		if(node_reference->data_type != DataType::UIControl) {
			error.m_message = id_node_type+" needs to be of type <UI Control> to be referenced in <UI Callback>.";
			error.exit();
		} else {
			// var ref is ui control -> check if it is ui_label
			if(const auto ui_control = declaration->parent->cast<NodeUIControl>()) {
				if(ui_control->name == "ui_label") {
					error.m_message = "<UI Label> cannot be referenced in <UI Callback>.";
					error.exit();
				}
			}
		}
		return true;
	}
};