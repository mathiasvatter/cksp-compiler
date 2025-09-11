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
 *
 *
 * IMPORTANT:
 * Also lowers continue statements in for loops to
 * inc(incrementer)
 * continue
 * Otherwise the loop would become infinite!
 * This is only done for range loops because all others are lowered to for-loops which tackle this already.
 *
 */
class LoweringForEach final : public ASTLowering {
public:
	explicit LoweringForEach(NodeProgram* program) : ASTLowering(program) {
		m_def_provider = program->def_provider;
		m_program = program;
	};

private:
	DefinitionProvider* m_def_provider;

    std::unordered_map<std::string, std::unique_ptr<NodeAST>> m_value_substitution;
    std::unique_ptr<NodeAST> get_value_substitute(const std::string& name) {
		auto it = m_value_substitution.find(name);
		if(it != m_value_substitution.end()) {
			return clone_as<NodeAST>(it->second.get());
		}
        return nullptr;
    }

	std::shared_ptr<NodeDataStructure> m_key = nullptr;
	std::shared_ptr<NodeDataStructure> m_value = nullptr;

	std::vector<NodeLoop*> m_loop_stack; // to determine whether we are in a while loop of a for loop
	std::unique_ptr<NodeAST> m_compound_assignment;

public:

	NodeAST* visit(NodeForEach& node) override {
		m_loop_stack.push_back(&node);
		node.remove_references();
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
			auto block = tackle_for_each_pairs(node);
			block->collect_references();
			m_loop_stack.pop_back();
			return node.replace_with(std::move(block));
		// normal for each loop over array with one key
		} else {
			if(m_key) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found incorrect amount of variables in for-each syntax. When a (key, value) pair is used, the range has to be a <pairs> function.";
				error.m_expected = "1";
				error.m_got = "2";
				error.exit();
			}
			auto block = tackle_for_each(node);
			block->collect_references();
			m_loop_stack.pop_back();
			return node.replace_with(std::move(block));
		}

		m_loop_stack.pop_back();
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
		} else if(node.range->cast<NodeInitializerList>()) {
			return lower_foreach_initlist(m_key, m_value, node);
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
		} else if(node.range->cast<NodeInitializerList>()) {
			return lower_foreach_initlist(m_key, m_value, node);
		} else {
			throw_range_composite_error(node.range->tok).exit();
		}
		return nullptr;
	}

	// declare local value := start
	// declare local _step = step
	// declare local _stop = stop
	// while (_step > 0 and value < _stop) or (_step < 0 and value > _stop)
	// 	message(value)
	// 	value += _step
	// end while
	std::unique_ptr<NodeBlock> lower_foreach_range(std::shared_ptr<NodeDataStructure>& key, std::shared_ptr<NodeDataStructure>& value, NodeForEach& node) {
		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		auto range = node.range->cast<NodeRange>();
		value->ty = range->start->ty;
		if(key) {
			node_scope->add_as_stmt(std::make_unique<NodeSingleDeclaration>(key, nullptr, range->tok));
		}
		node_scope->add_as_stmt(
			std::make_unique<NodeSingleDeclaration>(
				value, std::move(range->start), range->tok
			)
		);
		// declare _step and _stop
		auto _step_var = m_program->get_tmp_var(range->step->ty);
		_step_var->name = m_def_provider->get_fresh_name("_step");
		auto _stop_var = m_program->get_tmp_var(range->stop->ty);
		_stop_var->name = m_def_provider->get_fresh_name("_stop");
		auto _step = std::make_unique<NodeSingleDeclaration>(
			std::move(_step_var),
			std::move(range->step),
			range->tok
		);
		auto _stop = std::make_unique<NodeSingleDeclaration>(
			std::move(_stop_var),
			std::move(range->stop),
			range->tok
		);
		// i += step
		auto compound_assignment = std::make_unique<NodeCompoundAssignment>(
			value->to_reference(),
			_step->variable->to_reference(),
			token::ADD,
			range->tok
		);

		m_compound_assignment = compound_assignment->clone();
		node.body->accept(*this);
		m_compound_assignment = nullptr;

		auto node_while = std::make_unique<NodeWhile>(
			get_range_condition(value, _step->variable, _stop->variable, *range),
			std::move(node.body),
			node.tok
		);
		node_scope->add_as_stmt(std::move(_step));
		node_scope->add_as_stmt(std::move(_stop));
		node_while->body->add_as_stmt(std::move(compound_assignment));
		node_while->body->get_last_statement()->desugar(m_program);
		if(key) {
			node_while->body->add_as_stmt(DefinitionProvider::inc(key->to_reference()));
		}
		node_scope->add_as_stmt(std::move(node_while));


		node_scope->collect_references();
		return std::move(node_scope);
	}

	std::unique_ptr<NodeBlock> lower_foreach_initlist(const std::shared_ptr<NodeDataStructure>& key, const std::shared_ptr<NodeDataStructure>& value, NodeForEach& node) {
		auto node_scope = std::make_unique<NodeBlock>(node.tok, true);
		auto init_list = node.range->cast<NodeInitializerList>();
		init_list->flatten();
		// inline loop -> for each element in initializer list
		for(int i = 0; i < init_list->size(); i++) {
			auto &val = init_list->elements[i];
			// substitute val with value
			m_value_substitution.insert({value->name, std::move(val)});
			if(key) {
				m_value_substitution.insert({key->name, std::make_unique<NodeInt>(i, node.tok)});
			}
			auto element_body = clone_as<NodeBlock>(node.body.get());
			element_body->accept(*this);
			m_value_substitution.clear();
			node_scope->add_as_stmt(std::move(element_body));
		}
		return std::move(node_scope);
	}

	static CompileError throw_range_composite_error(const Token &tok) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Found incorrect range in for-each syntax. Range has to be of type <Composite> which"
						  " can be an array, a function or a multidimensional array. This range parameter seems"
						  " not to be recognized as valid.";
		return error;
	}


	NodeAST* visit(NodeVariableRef& node) override {
		// range-based for-loop substitution
		return do_substitution(node);
	}

	NodeAST* visit(NodePointerRef& node) override {
		// range-based for-loop substitution
		return do_substitution(node);
	}

private:

	static std::unique_ptr<NodeBinaryExpr> get_range_condition(
		const std::shared_ptr<NodeDataStructure>& iterator,
		const std::shared_ptr<NodeDataStructure>& step,
		const std::shared_ptr<NodeDataStructure>& stop,
		NodeRange& node
	) {
		// (step > 0 and _iter < stop) or (step < 0 and _iter > stop)
		return std::make_unique<NodeBinaryExpr>(
			token::BOOL_OR,
			std::make_unique<NodeBinaryExpr>(
				token::BOOL_AND,
				std::make_unique<NodeBinaryExpr>(
					token::LESS_THAN,
					iterator->to_reference(),
					stop->to_reference(),
					node.tok
				),
				std::make_unique<NodeBinaryExpr>(
					token::GREATER_THAN,
					step->to_reference(),
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
					stop->to_reference(),
					node.tok
				),
				std::make_unique<NodeBinaryExpr>(
					token::LESS_THAN,
					step->to_reference(),
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

	// inside while loops no lowering of continue statements
	NodeAST * visit(NodeWhile& node) override {
		m_loop_stack.push_back(&node);
		ASTVisitor::visit(node);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST * visit(NodeFor& node) override {
		m_loop_stack.push_back(&node);
		ASTVisitor::visit(node);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);

		if (node.kind != NodeFunctionCall::Kind::Builtin) return &node;
		if (m_loop_stack.back() != m_loop_stack.front()) return &node;
		if(node.function->name == "continue" and m_compound_assignment) {
			auto block = std::make_unique<NodeBlock>(node.tok, false);
			block->add_as_stmt(m_compound_assignment->clone());
			block->get_last_statement()->desugar(m_program);
			block->add_as_stmt(DefinitionProvider::continu(node.tok));
			return node.replace_with(std::move(block));
		}
		return &node;
	}


};