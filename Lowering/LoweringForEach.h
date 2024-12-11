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

    std::unordered_map<std::string, std::unique_ptr<NodeReference>> m_value_substitution;
    std::unique_ptr<NodeReference> get_value_substitute(const std::string& name) {
		auto it = m_value_substitution.find(name);
		if(it != m_value_substitution.end()) {
			return clone_as<NodeReference>(it->second.get());
		}
        return nullptr;
    }

	std::shared_ptr<NodeDataStructure> m_key = nullptr;
	std::shared_ptr<NodeDataStructure> m_value = nullptr;

public:

	inline NodeAST* visit(NodeForEach& node) override {
        // check if keys are variable references
		m_key = node.key ? node.key->variable : nullptr;
		m_value = node.value->variable;

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
		if(node.range->cast<NodePairs>()) {
			if (!m_key) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found no (key, value) pair in for-each syntax when using <pairs>.";
				error.m_expected = "2";
				error.m_got = "1";
				error.exit();
			}
			return node.replace_with(tackle_for_each_pairs(node));
		// normal for each loop over array with one key
		} else {
			if(m_key) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found incorrect amount of variables in for-each syntax. When a (key, value) pair is used, the range has to be a <pairs> function.";
				error.m_expected = "1";
				error.m_got = "2";
				error.exit();
			}
			if(node.range->cast<NodeRange>()) {

			}
			return node.replace_with(tackle_for_each(node));
		}


		return &node;
    }

	/// lowers for each loop with key, value pair and pairs function
	std::unique_ptr<NodeBlock> tackle_for_each_pairs(NodeForEach& node) {
		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		auto pairs = node.range->cast<NodePairs>();
		node.range->replace_with(std::move(pairs->range));
		if(auto comp_ref = cast_node<NodeCompositeRef>(node.range.get())) {
			// different approaches if node.range is ArrayRef or NDArrayRef:
			// if array, the key will be substituted
			// if ndarray, the key will be separate variable getting increased in innermost loop -> inc(key)
			if(auto array = comp_ref->cast<NodeArrayRef>()) {
				array->set_index(m_key->to_reference());
				array->ty = array->ty->get_element_type();
				node_scope->append_body(std::move(node.body));
				node_scope->wrap_in_loop(m_key, std::make_unique<NodeInt>(0, node.tok), array->get_size());
			} else {
				// if multidimensional array, the key will be a separate variable getting increased in innermost loop -> inc(key)
				auto inner_block = comp_ref->iterate_over(node_scope, m_program);
				inner_block->prepend_body(std::move(node.body));
				inner_block->add_as_stmt(DefinitionProvider::inc(m_key->to_reference()));
				// prepend key declaration
				node_scope->prepend_as_stmt(std::make_unique<NodeSingleDeclaration>(std::move(m_key), nullptr, node.key->tok));
			}

			auto node_array = unique_ptr_cast<NodeCompositeRef>(std::move(node.range));
			m_value_substitution.insert({m_value->name, std::move(node_array)});
			node_scope->accept(*this);
			m_value_substitution.clear();
			return std::move(node_scope);
		} else if(auto range = node.range->cast<NodeRange>()) {
			return lower_foreach_range(m_key, m_value, node);
		} else {
			throw_range_composite_error(node.range->tok).exit();
		}
		return nullptr;
	}

	/// lowers for each loop with one key reference
	std::unique_ptr<NodeBlock> tackle_for_each(NodeForEach& node) {
		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		// foreach loop without pairs function -> only one key
		if(auto comp_ref = cast_node<NodeCompositeRef>(node.range.get())) {
			auto inner_block = comp_ref->iterate_over(node_scope, m_program);
			inner_block->prepend_body(std::move(node.body));
			auto node_array = unique_ptr_cast<NodeCompositeRef>(std::move(node.range));
			m_value_substitution.insert({m_value->name, std::move(node_array)});
			node_scope->accept(*this);
			m_value_substitution.clear();
			return std::move(node_scope);
		} else if(auto range = node.range->cast<NodeRange>()) {
			return lower_foreach_range(m_key, m_value, node);
		} else {
			throw_range_composite_error(node.range->tok).exit();
		}
		return nullptr;
	}

	std::unique_ptr<NodeBlock> lower_foreach_range(std::shared_ptr<NodeDataStructure>& key, std::shared_ptr<NodeDataStructure>& value, NodeForEach& node) {
		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		auto range = node.range->cast<NodeRange>();
		value->ty = range->start->ty;
		if(key) {
			node_scope->add_as_stmt(std::make_unique<NodeSingleDeclaration>(key, nullptr, range->tok));
		}
		node_scope->add_as_stmt(std::make_unique<NodeSingleDeclaration>(value, std::move(range->start), range->tok));
		auto node_while = std::make_unique<NodeWhile>(
			get_range_condition(value, *range),
			std::move(node.body),
			node.tok
		);
		auto increase = std::make_unique<NodeSingleAssignment>(
			value->to_reference(),
			std::make_unique<NodeBinaryExpr>(
				token::ADD,
				value->to_reference(),
				range->step->clone(),
				range->tok
			),
			range->tok
		);
		node_while->body->add_as_stmt(std::move(increase));
		if(key) {
			node_while->body->add_as_stmt(DefinitionProvider::inc(key->to_reference()));
		}
		node_scope->add_as_stmt(std::move(node_while));
		node_scope->collect_references();
		return std::move(node_scope);
	}

	static CompileError throw_range_composite_error(Token tok) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Found incorrect range in for-each syntax. Range has to be of type <Composite> which"
						  " can be an array, a function or a multidimensional array";
		return error;
	}


	inline NodeAST* visit(NodeVariableRef& node) override {
		// range-based for-loop substitution
		return do_substitution(node);
	}

	inline NodeAST* visit(NodePointerRef& node) override {
		// range-based for-loop substitution
		return do_substitution(node);
	}

private:

	std::unique_ptr<NodeBinaryExpr> get_range_condition(std::shared_ptr<NodeDataStructure>& iterator, NodeRange& node) {
		// (step > 0 and _iter < stop) or (step < 0 and _iter > stop)
		return std::make_unique<NodeBinaryExpr>(
			token::BOOL_OR,
			std::make_unique<NodeBinaryExpr>(
				token::BOOL_AND,
				std::make_unique<NodeBinaryExpr>(
					token::LESS_THAN,
					iterator->to_reference(),
					node.stop->clone(),
					node.tok
				),
				std::make_unique<NodeBinaryExpr>(
					token::GREATER_THAN,
					node.step->clone(),
					std::make_unique<NodeInt>(0, node.tok),
					node.tok
				),
				node.tok
			),
			std::make_unique<NodeBinaryExpr>(
				token::BOOL_AND,
				std::make_unique<NodeBinaryExpr>(
					token::GREATER_THAN,
					iterator->to_reference(),
					node.stop->clone(),
					node.tok
				),
				std::make_unique<NodeBinaryExpr>(
					token::LESS_THAN,
					node.step->clone(),
					std::make_unique<NodeInt>(0, node.tok),
					node.tok
				),
				node.tok
			),
			node.tok
		);
	}

	NodeAST* do_substitution(NodeReference& node) {
		if(!m_value_substitution.empty()) {
			if(auto substitute = get_value_substitute(node.name)) {
				substitute->update_parents(node.parent);
				auto new_node = node.replace_with(std::move(substitute));
				new_node->collect_references();
				return new_node;
			}
		}

		return &node;
	}


};