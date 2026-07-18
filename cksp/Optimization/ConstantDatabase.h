//
// Created by Mathias Vatter on 03.07.26.
//

#pragma once
#include <optional>
#include <shared_mutex>

#include "../ASTVisitor/ASTOptimizations.h"

/**
 * Early pass that collects every constant in the program and eagerly folds its
 * value, so later passes can look up constant values at any time.
 * Because declarations are visited in source order, a constant that refers to
 * previously declared constants is resolved to a literal immediately.
 * Additionally records folded array size expressions (they must be
 * compile-time constants in KSP).
 * The pass is purely observing: all folding happens on clones owned by the
 * database, the program AST itself stays completely unchanged.
 */
class ConstantDatabase final : public ASTOptimizations {
	/// declaration -> eagerly folded value (literal or initializer list if resolvable)
	std::unordered_map<NodeDataStructure*, std::unique_ptr<NodeAST>> m_values;
	/// array declaration -> folded size per dimension (one entry for NodeArray)
	std::unordered_map<NodeDataStructure*, std::vector<std::unique_ptr<NodeAST>>> m_array_sizes;
	mutable std::shared_mutex m_mutex;

public:
	NodeAST* build(NodeProgram& node) {
		m_program = &node;
		{
			std::unique_lock lock(m_mutex);
			m_values.clear();
			m_array_sizes.clear();
		}
		// globals and init callback first, so callbacks/functions can rely on them
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		parallel_for_each(node.callbacks.begin(), node.callbacks.end(),
			[this](const auto& callback) {
				if (callback.get() == m_program->init_callback) return;
				callback->accept(*this);
			}
		);
		parallel_for_each(node.function_definitions.begin(), node.function_definitions.end(),
			[this](const auto& func) {
				func->accept(*this);
			}
		);
		node.reset_function_visited_flag();
		// debug_print();
		return &node;
	}

	[[nodiscard]] bool contains(NodeDataStructure* decl) const {
		if (!decl) return false;
		std::shared_lock lock(m_mutex);
		return m_values.contains(decl);
	}

	/// returns a clone of the folded value of a constant, nullptr if unknown
	[[nodiscard]] std::unique_ptr<NodeAST> clone_value(NodeDataStructure* decl) const {
		if (!decl) return nullptr;
		std::shared_lock lock(m_mutex);
		const auto it = m_values.find(decl);
		if (it == m_values.end()) return nullptr;
		return it->second->clone();
	}

	/// returns a clone of the folded element of a constant array, nullptr if unknown
	[[nodiscard]] std::unique_ptr<NodeAST> clone_element(NodeDataStructure* decl, const int idx) const {
		if (!decl) return nullptr;
		std::shared_lock lock(m_mutex);
		const auto it = m_values.find(decl);
		if (it == m_values.end()) return nullptr;
		const auto init_list = it->second->cast<NodeInitializerList>();
		if (!init_list or init_list->elements.empty() or idx < 0) return nullptr;
		// elem() clamps to the last element, mirroring KSP's shorthand fill
		return init_list->elem(idx)->clone();
	}

	/// interpreted integer value of a constant, if it folded down to a literal
	[[nodiscard]] std::optional<int32_t> get_int(NodeDataStructure* decl) const {
		if (!decl) return std::nullopt;
		std::shared_lock lock(m_mutex);
		const auto it = m_values.find(decl);
		if (it == m_values.end()) return std::nullopt;
		if (const auto int_node = it->second->cast<NodeInt>()) return int_node->value;
		return std::nullopt;
	}

	/// interpreted real value of a constant, if it folded down to a literal
	[[nodiscard]] std::optional<double> get_real(NodeDataStructure* decl) const {
		if (!decl) return std::nullopt;
		std::shared_lock lock(m_mutex);
		const auto it = m_values.find(decl);
		if (it == m_values.end()) return std::nullopt;
		if (const auto real_node = it->second->cast<NodeReal>()) return real_node->value;
		return std::nullopt;
	}

	/// interpreted size of an array declaration, if its size expression folded
	/// down to a literal. Works for every array, not only constant ones
	[[nodiscard]] std::optional<int32_t> get_array_size(NodeDataStructure* decl, const size_t dimension = 0) const {
		if (!decl) return std::nullopt;
		std::shared_lock lock(m_mutex);
		const auto it = m_array_sizes.find(decl);
		if (it == m_array_sizes.end() or dimension >= it->second.size()) return std::nullopt;
		if (const auto size = it->second[dimension]->cast<NodeInt>()) return size->value;
		return std::nullopt;
	}

	/// number of recorded dimensions of an array declaration, 0 if unknown
	[[nodiscard]] size_t get_num_dimensions(NodeDataStructure* decl) const {
		if (!decl) return 0;
		std::shared_lock lock(m_mutex);
		const auto it = m_array_sizes.find(decl);
		if (it == m_array_sizes.end()) return 0;
		return it->second.size();
	}

	[[nodiscard]] size_t size() const {
		std::shared_lock lock(m_mutex);
		return m_values.size();
	}

	void debug_print() const {
		std::shared_lock lock(m_mutex);
		for (const auto& [decl, value] : m_values) {
			std::cout << decl->name << " := " << value->get_string() << std::endl;
		}
		for (const auto& [decl, sizes] : m_array_sizes) {
			std::cout << decl->name << ".SIZE := " << StringUtils::join_apply(
				sizes, [](auto& size) { return size->get_string(); }) << std::endl;
		}
	}

private:
	/// Fold a detached clone against constants already recorded in this database.
	[[nodiscard]] std::unique_ptr<NodeAST> fold_clone(const NodeAST& expression) const {
		auto statement = std::make_unique<NodeStatement>(expression.clone(), expression.tok);
		statement->do_constant_folding(this);
		auto folded = std::move(statement->statement);
		folded->parent = nullptr;
		return folded;
	}

	void add_constant(NodeDataStructure* decl, const NodeAST& value) {
		auto folded = fold_clone(value);
		std::unique_lock lock(m_mutex);
		m_values[decl] = std::move(folded);
	}

	void add_array_sizes(NodeDataStructure* decl, const std::vector<NodeAST*>& sizes) {
		std::vector<std::unique_ptr<NodeAST>> folded_sizes;
		folded_sizes.reserve(sizes.size());
		for (const auto size : sizes) {
			folded_sizes.push_back(fold_clone(*size));
		}
		std::unique_lock lock(m_mutex);
		m_array_sizes[decl] = std::move(folded_sizes);
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		// record array sizes, they must be compile-time constants in KSP
		if (const auto array = node.variable->cast<NodeArray>()) {
			if (array->size) {
				add_array_sizes(node.variable.get(), {array->size.get()});
			} else if (node.value) {
				// declared without explicit size -> size is the initializer list length
				if (const auto init_list = node.value->cast<NodeInitializerList>()) {
					auto size = std::make_unique<NodeInt>(static_cast<int32_t>(init_list->size()), node.tok);
					size->ty = TypeRegistry::Integer;
					std::vector<std::unique_ptr<NodeAST>> sizes;
					sizes.push_back(std::move(size));
					std::unique_lock lock(m_mutex);
					m_array_sizes[node.variable.get()] = std::move(sizes);
				}
			}
		} else if (const auto nd_array = node.variable->cast<NodeNDArray>()) {
			if (nd_array->sizes) {
				std::vector<NodeAST*> sizes;
				sizes.reserve(nd_array->sizes->params.size());
				for (const auto& size : nd_array->sizes->params) sizes.push_back(size.get());
				add_array_sizes(node.variable.get(), sizes);
			}
		}
		if (node.value and node.variable->data_type == DataType::Const) {
			add_constant(node.variable.get(), *node.value);
		}
		return &node;
	}
};
