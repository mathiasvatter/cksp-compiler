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
		m_def_provider->refresh_scopes();
		m_program = &node;
		m_init_callback = node.callbacks[0].get();
		for(auto & def : node.function_definitions) {
			m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

	}

	void inline visit(NodeCallback& node) {
		if(&node == m_init_callback) m_is_init_callback = true;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_is_init_callback = false;
	}

	void inline visit(NodeBody &node) {
		m_current_body = &node;
		if(node.scope) m_def_provider->add_scope();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) {
			auto passive_vars = m_def_provider->remove_scope();
            // add free memory vars which dynamic extent has ended to passive_vars vector
			add_passive_vars(passive_vars);
            // set back passive_var index since scope has ended
            m_passive_var_idx = 0;
		}
	}

	void inline visit(NodeFunctionCall& node) override {
		if(!node.definition and !node.function->is_builtin) {
			if (auto function_def = get_function_definition(node.function.get())) {
				node.definition = function_def;
				m_current_function = function_def;
			} else if (auto builtin_func = m_def_provider->get_builtin_function(node.function.get())) {
				node.function->type = builtin_func->type;
				node.function->has_forced_parenth = builtin_func->has_forced_parenth;
				node.function->arg_ast_types = builtin_func->arg_ast_types;
				node.function->arg_var_types = builtin_func->arg_var_types;
				node.function->is_builtin = builtin_func->is_builtin;
			} else {
				CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
			}
		}
//		node.definition->accept(*this);
	}

	void inline visit(NodeSingleDeclareStatement& node) {
		if(m_current_body->scope and m_current_body->parent != m_init_callback) {
			node.to_be_declared->is_local = true;
		}

		node.to_be_declared->accept(*this);
		if(node.assignee) node.assignee->accept(*this);

	}

	void inline visit(NodeArrayRef& node) {
		if(node.index) node.index->accept(*this);

		auto node_declaration = m_def_provider->get_declaration(&node);
		if(!node_declaration) throw_declaration_error(&node).exit();

		node.match_data_structure(node_declaration);
		node.name = node_declaration->name;
	}

	void inline visit(NodeArray& node) {
		if(node.size) node.size->accept(*this);

		m_def_provider->set_declaration(&node, m_is_init_callback || !node.is_local);
	}

	void inline visit(NodeVariableRef& node) {
		auto node_declaration = m_def_provider->get_declaration(&node);
		if(!node_declaration) throw_declaration_error(&node).exit();

		node.match_data_structure(node_declaration);
		node.name = node_declaration->name;

		// replace variable with array if incorrectly recognized by parser
		if(node_declaration->get_node_type() == NodeType::Array) {
			auto node_array = std::make_unique<NodeArrayRef>(node.name, nullptr, node.tok);
			node_array->accept(*this);
			node.replace_with(std::move(node_array));
			return;
		}
	}

	void inline visit(NodeVariable& node) {
		m_def_provider->set_declaration(&node, m_is_init_callback || !node.is_local);

        if(node.is_local) {
            if(auto free_passive_var = get_free_passive_var()) {
                m_passive_vars_replace.insert({&node, free_passive_var});
                m_def_provider->remove_from_current_scope(node.name);
                node.name = free_passive_var->name;
                return;
            }
        }
	}

private:
	std::string loc_var_prefix = "loc_";
	Gensym m_gensym;
	DefinitionProvider* m_def_provider = nullptr;
	NodeProgram* m_program = nullptr;
	NodeCallback* m_init_callback = nullptr;
	NodeBody* m_current_body = nullptr;
	bool m_is_init_callback = false;
	NodeFunctionDefinition* m_current_function = nullptr;

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
    inline NodeDataStructure* get_new_declaration(NodeReference *node) {
        if(!node->declaration) throw_declaration_error(node).exit();
        auto it = m_passive_vars_replace.find(node->declaration);
        if(it != m_passive_vars_replace.end()) {
            return it->second;
        }
        return nullptr;
    }
    /// map for old datastructures (as keys) that get replaced by new datastructures (passive_vars) (as values)
    std::unordered_map<NodeDataStructure*, NodeDataStructure*> m_passive_vars_replace;
	std::vector<NodeDataStructure*> m_all_data_structures;
	std::vector<NodeReference*> m_all_references;

    /// method for replacing local variable declarations with passive_var references in assignment
    void inline replace_declaration_with_passive_var(NodeSingleDeclareStatement* node, NodeDataStructure* free_passive_var) {
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
        node->replace_with(std::move(node_assign_statement));
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
