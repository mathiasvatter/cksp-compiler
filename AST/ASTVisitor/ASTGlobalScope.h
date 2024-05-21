//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../misc/Gensym.h"

class ASTGlobalScope : public ASTVisitor {
public:
	explicit ASTGlobalScope(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}

	void inline visit(NodeProgram& node) override {
		m_program = &node;
		m_def_provider->refresh_scopes();
		m_passive_vars.clear();
		for(auto & def : node.function_definitions) {
//			def->visited = false;
			m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
		}

		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			if(!function_definition->visited) {
				m_passive_vars.clear();
				function_definition->accept(*this);
			}
		}

        // rename local passive_vars with gensym and add to global scope
        for(auto & local_var : m_all_local_vars) {
            local_var->name = m_gensym.fresh(loc_var_prefix);
            m_def_provider->set_declaration(local_var, false);
        }
        // rename all local references with their new passive_var names
        for(auto & local_ref : m_all_local_references) {
            local_ref->name = local_ref->declaration->name;
        }
        // move all passive_vars declarations to global scope
        auto local_declare_statement = std::make_unique<NodeBody>(node.tok);
        for(auto & local_var : m_all_local_callback_vars) {
            auto node_declare_statement = local_var->parent;
            auto node_assign_statement = assign_statement_from_declaration(static_cast<NodeSingleDeclareStatement*>(node_declare_statement), local_var);
            local_declare_statement->statements.push_back(std::make_unique<NodeStatement>(
                    clone_as<NodeSingleDeclareStatement>(node_declare_statement), node.tok)
                    );
            node_declare_statement->replace_with(std::move(node_assign_statement));
        }
        node.init_callback->statements->prepend_body(std::move(local_declare_statement));
	}

	void inline visit(NodeCallback& node) override {
        m_current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_current_callback = nullptr;
	}

	void inline visit(NodeBody &node) override {
		m_current_body = &node;
		if(node.scope) m_def_provider->add_scope();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) {
			auto passive_vars = m_def_provider->remove_scope();
			// only add new passive vars on thread_safe callbacks and functions not using 'wait'
			if(is_thread_safe_env()) {
				// add free memory vars which dynamic extent has ended to passive_vars vector
				add_passive_vars(passive_vars);
			}
			// set back passive_var index since scope has ended
			m_passive_var_idx = 0;
			// clear passive_var replace map
			m_passive_vars_replace.clear();
		}
	}

	void inline visit(NodeFunctionCall& node) override {
        node.function->accept(*this);


		if(!node.function->is_builtin or !node.definition) {
			if (auto function_def = get_function_definition(node.function.get())) {
				node.definition = function_def;
			} else if (auto builtin_func = m_def_provider->get_builtin_function(node.function.get())) {
				node.function->type = builtin_func->type;
				node.function->has_forced_parenth = builtin_func->has_forced_parenth;
				node.function->arg_ast_types = builtin_func->arg_ast_types;
				node.function->arg_var_types = builtin_func->arg_var_types;
				node.function->is_builtin = builtin_func->is_builtin;
				node.function->is_thread_safe = builtin_func->is_thread_safe;
			} else {
				CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
			}
		}

		// only in second pass -> visit function definition
		if(node.definition) {
			node.definition->call_sites.emplace(&node);
			node.definition->callback_sites.emplace(m_current_callback);
			if(!node.definition->visited and !m_function_call_stack.empty())
				node.definition->accept(*this);
		}

	}

	void inline visit(NodeFunctionDefinition& node) override {
		m_function_call_stack.push(&node);
		m_current_function = m_function_call_stack.top();
		node.body->accept(*this);
		node.visited = true;
		m_function_call_stack.pop();
		m_current_function = nullptr;
		if(!m_function_call_stack.empty()) m_current_function = m_function_call_stack.top();
	}

	void inline visit(NodeSingleDeclareStatement& node) override {
		if(is_local_var(node.to_be_declared.get(), m_current_body, m_current_callback, m_program->init_callback)) {
			node.to_be_declared->is_local = true;
		}

		if(node.assignee) node.assignee->accept(*this);

		if(node.to_be_declared->is_local) {
			// do lambda lifting when in function and add var to arguments
//			if(m_current_function) {
////				m_all_local_function_vars.push_back(node.to_be_declared.get());
//				m_current_function->header->args->params.push_back(node.to_be_declared->clone());
//				m_passive_vars_replace.insert({node.to_be_declared->name, static_cast<NodeDataStructure*>(m_current_function->header->args->params.back().get())});
//				auto node_assign_statement = assign_statement_from_declaration(&node, node.to_be_declared.get());
//
//			}

			if(is_thread_safe_env()) {
				if (auto free_passive_var = get_free_passive_var()) {
					m_passive_vars_replace.insert({node.to_be_declared->name, free_passive_var});
					auto node_assign_statement = assign_statement_from_declaration(&node, free_passive_var);
					m_all_local_references.push_back(static_cast<NodeReference *>(node_assign_statement->array_variable.get()));
					node.replace_with(std::move(node_assign_statement));
					return;
				}
			}
		}
		// only add var to local scope if it is not replaced by passive_var
		node.to_be_declared->accept(*this);

	}

	void inline visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);

		auto node_declaration = m_def_provider->get_declaration(&node);
		if(!node_declaration) throw_declaration_error(&node).exit();

		node.match_data_structure(node_declaration);
	}

	void inline visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);

		m_def_provider->set_declaration(&node, !node.is_local);
		if(node.is_local) {
			if(m_current_callback) m_all_local_callback_vars.push_back(&node);
			m_all_local_vars.push_back(&node);
		}
	}

	void inline visit(NodeVariableRef& node) override {
		// add all references in local scope to vector for later passive_var replacement
		m_all_local_references.push_back(&node);

		if(!m_passive_vars_replace.empty()) {
			// search if declaration was local var and has been replaced by passive_var -> replace declaration and reference name
			if(auto new_declaration = get_new_declaration(node.name)) {
				node.match_data_structure(new_declaration);
				node.name = new_declaration->name;
				return;
			}
		}

		auto node_declaration = m_def_provider->get_declaration(&node);
		if(!node_declaration) throw_declaration_error(&node).exit();

		node.match_data_structure(node_declaration);

		// replace variable with array if incorrectly recognized by parser
		if(node_declaration->get_node_type() == NodeType::Array) {
			auto node_array = std::make_unique<NodeArrayRef>(node.name, nullptr, node.tok);
			node_array->accept(*this);
			node.replace_with(std::move(node_array));
			return;
		}
	}

	void inline visit(NodeVariable& node) override {
		m_def_provider->set_declaration(&node, !node.is_local);
		if(node.is_local) {
			if(m_current_callback) m_all_local_callback_vars.push_back(&node);
			m_all_local_vars.push_back(&node);
		}
	}

private:
	std::string loc_var_prefix = "loc_";
	Gensym m_gensym;
	DefinitionProvider* m_def_provider = nullptr;
	NodeProgram* m_program = nullptr;
	NodeBody* m_current_body = nullptr;
	NodeCallback* m_current_callback = nullptr;
	NodeFunctionDefinition* m_current_function = nullptr;
	/// holds the current function definition that is being visited on top
	std::stack<NodeFunctionDefinition*> m_function_call_stack;


	static bool inline is_local_var(NodeDataStructure* node, NodeBody* current_body, NodeCallback* current_callback, NodeCallback* init_callback) {
		return current_body->scope and current_callback != init_callback and !node->is_global and node->get_node_type() != NodeType::UIControl;
	};

	bool inline is_thread_safe_env() {
		return (m_current_callback and m_current_callback->is_thread_safe) or (m_current_function and m_current_function->header->is_thread_safe);
	};

	/// vector for all local vars in callbacks
	std::vector<NodeDataStructure*> m_all_local_callback_vars;
	/// vector for all local vars in functions -> do not get moved into on init
	std::vector<NodeDataStructure*> m_all_local_vars;
	/// vector for all variables which dynamic extend has ended
	std::vector<NodeDataStructure*> m_passive_vars;
	inline void add_passive_vars(const std::unordered_map<std::string, NodeDataStructure*, StringHash, StringEqual>& map2) {
		for(auto & var : map2) {
            m_passive_vars.push_back(var.second);
		}
	};
	int m_passive_var_idx = 0;
    /// get next free passive_var
    inline NodeDataStructure* get_free_passive_var() {
        if(m_passive_var_idx < m_passive_vars.size()) {
            return m_passive_vars[m_passive_var_idx++];
        }
        return nullptr;
    }
    /// search for new declaration to reference if declaration is replaced by passive_var
    inline NodeDataStructure* get_new_declaration(const std::string& ref_name) {
        auto it = m_passive_vars_replace.find(ref_name);
        if(it != m_passive_vars_replace.end()) {
            return it->second;
        }
        return nullptr;
    }
    /// map for old datastructure name (as keys) that get replaced by new datastructures (passive_vars) (as values)
    std::unordered_map<std::string, NodeDataStructure*> m_passive_vars_replace;
    /// vector for all local references that have been replaced by passive_var references
	std::vector<NodeReference*> m_all_local_references;

    /// method for replacing local variable declarations with passive_var references in assignment
    std::unique_ptr<NodeSingleAssignStatement> inline assign_statement_from_declaration(NodeSingleDeclareStatement* node, NodeDataStructure* free_passive_var) {
        // change declare statement to assign statement and replace declaration with reference to passive_var
        auto passive_var_ref = free_passive_var->to_reference();
        std::unique_ptr<NodeAST> node_assignee = nullptr;
        if(!node->assignee) {
            node_assignee = std::make_unique<NodeInt>(0, node->tok);
        } else {
            node_assignee = std::move(node->assignee);
        }
        auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(
                std::move(passive_var_ref),
                std::move(node_assignee),
                node->tok
        );
        return node_assign_statement;
    };

	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> m_function_lookup;
	inline NodeFunctionDefinition* get_function_definition(NodeFunctionHeader *function_header) {
		auto it = m_function_lookup.find({function_header->name, (int)function_header->args->params.size()});
		if(it != m_function_lookup.end()) {
			it->second->is_used = true;
			return it->second;
		}
		return nullptr;
	};

	static inline CompileError throw_declaration_error(NodeReference* node) {
		auto compile_error = CompileError(ErrorType::Variable, "","", node->tok);
		std::string type = "<Variable>";
		if(node->get_node_type() == NodeType::Array) type = "<Array>";
		compile_error.m_message = type+" has not been declared: " + node->name+".";
		compile_error.m_expected = "Valid declaration";
		compile_error.m_got = node->name;
		return compile_error;
	};

	static inline CompileError throw_declaration_type_error(NodeReference* node) {
		auto compile_error = CompileError(ErrorType::Variable, "","", node->tok);
		if(!node->declaration) throw_declaration_error(node).exit();
		if(node->declaration->get_node_type() == NodeType::Array && node->get_node_type() == NodeType::Variable) {
			compile_error.m_message = "Incorrect Reference type. Reference was declared as <Array>: " + node->name+".";
			compile_error.m_expected = "<Array>";
			compile_error.m_got = "<Variable>";
			compile_error.exit();
		}
		if(node->declaration->get_node_type() == NodeType::Variable && node->get_node_type() == NodeType::Array) {
			compile_error.m_message = "Incorrect Reference type. Reference was declared as <Variable>: " + node->name+".";
			compile_error.m_expected = "<Variable>";
			compile_error.m_got = "<Array>";
			compile_error.exit();
		}
		return compile_error;
	}

};

struct PassiveVar {
    std::string name;
    NodeDataStructure* declaration;
    NodeType node_type;
    ASTType type;
};
