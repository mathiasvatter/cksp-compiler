//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * @brief This class is responsible for desugaring for each loops into for loops in the AST.
 * In this case, a for each loop is rewritten as a for loop.
 *
 * The transformation is as follows:
 *
 * Pre-desugaring:
 * declare array[10] := (3,4,6,8)
 * for _, val in array
 *     message(val)
 * end for
 *
 * Post-desugaring:
 * declare array[10] := (3,4,6,8)
 * for _ := 0 to num_elements(array) - 1
 *     message(array[_])
 * end for
 */
class LoweringForEach : public ASTLowering {
public:
	explicit LoweringForEach(NodeProgram* program) : ASTLowering(program) {
		m_def_provider = program->def_provider;
	};

private:
	DefinitionProvider* m_def_provider;
    std::unordered_map<std::string, std::unique_ptr<NodeReference>> m_key_value_scope_stack;
    std::unique_ptr<NodeReference> get_key_value_substitute(const std::string& name) {
		auto it = m_key_value_scope_stack.find(name);
		if(it != m_key_value_scope_stack.end()) {
			return clone_as<NodeReference>(it->second.get());
		}
        return nullptr;
    }

public:

	inline NodeAST* visit(NodeForEach& node) override {
        // check if keys are variable references
        for(auto &var : node.keys) {
            if(!var->cast<NodeVariableRef>() and !var->cast<NodePointerRef>()) {
        		auto error = CompileError(ErrorType::SyntaxError, "", "", var->tok);
                error.m_message = "Found incorrect key in ranged-for-loop syntax.";
                error.m_expected = "<Variable>";
                error.m_got = var->get_string();
                error.exit();
            }
        }

        // range has to be an array that was parsed as variable ref
        if(!node.range->ty->cast<CompositeType>()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.range->tok);
			error.m_message = "Found incorrect range in for-each syntax. Range has to be of type <Composite> which"
							  " can be an array, a function or a multidimensional array";
            error.m_expected = "<CompositeType>";
            error.m_got = node.range->ty->to_string();
            error.exit();
        }

		// foreach loop with pairs function -> two keys
		std::unique_ptr<NodeReference> node_key;
		std::unique_ptr<NodeReference> node_value;
		if(auto pairs = node.range->cast<NodePairs>()) {
			if(node.keys.size() != 2) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found incorrect amount of keys in for-each syntax when using <pairs>.";
				error.m_expected = "2";
				error.m_got = std::to_string(node.keys.size());
				error.exit();
			}
			node.range->replace_with(std::move(pairs->range));
		} else {
			if(node.keys.size() != 1) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found incorrect amount of keys in for-each syntax. When two keys are used, the range has to be a <pairs> function.";
				error.m_expected = "1";
				error.m_got = std::to_string(node.keys.size());
				error.exit();
			}
		}

		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		// foreach loop without pairs function -> only one key
		if(auto comp_ref = cast_node<NodeCompositeRef>(node.range.get())) {

			auto inner_block = comp_ref->iterate_over(node_scope);
			inner_block->prepend_body(std::move(node.body));
			auto node_array = unique_ptr_cast<NodeCompositeRef>(std::move(node.range));
			m_key_value_scope_stack.insert({node.keys[0]->name, std::move(node_array)});
			node_scope->accept(*this);
			m_key_value_scope_stack.clear();
			return node.replace_with(std::move(node_scope));

		}





//        auto node_value_array = std::make_unique<NodeArrayRef>(node.range->get_string(), std::move(node_key_ref), value_tok);
//        m_key_value_scope_stack.insert({node_value_ref->name, std::move(node_value_array)});
//
//        auto node_for_statement = std::make_unique<NodeFor>(
//                std::move(node_key_iterator),
//                token::TO,
//                std::move(node_end_range),
//                std::move(node.body), node.tok);
//        node_for_statement->body->accept(*this);
//
//        node_scope->add_as_stmt(std::move(node_for_statement));
//		node_scope->get_last_statement()->lower(m_program);
//        m_key_value_scope_stack.pop_back();
//        return node.replace_with(std::move(node_scope));
		return &node;
    }


	inline NodeAST* visit(NodeVariableRef& node) override {
		// range-based for-loop substitution
		if(!m_key_value_scope_stack.empty()) {
			if(auto substitute = get_key_value_substitute(node.name)) {
				substitute->update_parents(node.parent);
				auto new_node = node.replace_with(std::move(substitute));
				new_node->collect_references();
				return new_node;
			}
		}
		return &node;
	}

	inline NodeAST* visit(NodePointerRef& node) override {
		// range-based for-loop substitution
		if(!m_key_value_scope_stack.empty()) {
			if(auto substitute = get_key_value_substitute(node.name)) {
				substitute->update_parents(node.parent);
				auto new_node = node.replace_with(std::move(substitute));
				new_node->collect_references();
				return new_node;
			}
		}
		return &node;
	}

};