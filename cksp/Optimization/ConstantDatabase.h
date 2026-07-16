//
// Created by Mathias Vatter on 03.07.26.
//

#pragma once
#include <limits>
#include <optional>
#include <shared_mutex>

#include "../ASTVisitor/ASTOptimizations.h"
#include "../Lowering/LoweringNumElements.h"

class ConstantDatabase;

/**
 * Folds a detached value expression with the help of the database:
 * substitutes references to already known constants (scalars and const array
 * elements with a constant index) and constant folds the result.
 * Only ever runs on clones owned by the database, never on the program AST.
 */
class ConstantValueFolder final : public ASTOptimizations {
	const ConstantDatabase& m_db;

public:
	explicit ConstantValueFolder(const ConstantDatabase& db) : m_db(db) {}

	/// returns a folded clone of the given expression, the expression itself stays untouched
	std::unique_ptr<NodeAST> fold_clone(const NodeAST& expr) {
		// wrap the clone in a statement so replace_with has a parent to replace into
		auto stmt = std::make_unique<NodeStatement>(expr.clone(), expr.tok);
		stmt->do_constant_folding();
		stmt->accept(*this);
		stmt->do_constant_folding();
		auto folded = std::move(stmt->statement);
		folded->parent = nullptr;
		return folded;
	}

private:
	NodeAST* visit(NodeVariableRef& node) override;
	NodeAST* visit(NodeArrayRef& node) override;
	NodeAST* visit(NodeNumElements& node) override;

	static NodeAST* replace_with_int(NodeAST& node, const int32_t value) {
		auto new_node = std::make_unique<NodeInt>(value, node.tok);
		new_node->ty = TypeRegistry::Integer;
		return node.replace_with(std::move(new_node));
	}
};

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
	void add_constant(NodeDataStructure* decl, const NodeAST& value) {
		ConstantValueFolder folder(*this);
		auto folded = folder.fold_clone(value);
		std::unique_lock lock(m_mutex);
		m_values[decl] = std::move(folded);
	}

	void add_array_sizes(NodeDataStructure* decl, const std::vector<NodeAST*>& sizes) {
		ConstantValueFolder folder(*this);
		std::vector<std::unique_ptr<NodeAST>> folded_sizes;
		folded_sizes.reserve(sizes.size());
		for (const auto size : sizes) {
			folded_sizes.push_back(folder.fold_clone(*size));
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


inline NodeAST* ConstantValueFolder::visit(NodeVariableRef& node) {
	// builtin engine constants have no compile-time value
	if (node.is_engine) return &node;
	if (node.data_type != DataType::Const) return &node;
	if (auto value = m_db.clone_value(node.get_declaration().get())) {
		if (value->ty and node.ty and !value->ty->is_compatible(node.ty)) return &node;
		return node.replace_with(std::move(value));
	}
	return &node;
}

inline NodeAST* ConstantValueFolder::visit(NodeArrayRef& node) {
	// fold the index first, so constant indices become literals
	if (node.index) {
		node.index->accept(*this);
		node.index->do_constant_folding();
	}
	if (node.is_engine) return &node;
	if (node.data_type != DataType::Const) return &node;
	const auto index = node.index ? node.index->cast<NodeInt>() : nullptr;
	if (!index) return &node;
	if (auto element = m_db.clone_element(node.get_declaration().get(), index->value)) {
		return node.replace_with(std::move(element));
	}
	return &node;
}

inline NodeAST * ConstantValueFolder::visit(NodeNumElements &node) {
	if (!node.array) return &node;
	// fold a possible constant dimension expression first
	if (node.dimension) {
		node.dimension->accept(*this);
		node.dimension->do_constant_folding();
	}
	const auto decl = node.array->get_declaration();
	if (!decl) return &node;

	// plain arrays: no dimension parameter allowed (diagnosed later in lowering)
	if (decl->cast<NodeArray>()) {
		if (node.dimension) return &node;
		if (const auto size = m_db.get_array_size(decl.get())) {
			return replace_with_int(node, *size);
		}
		return &node;
	}

	// ndarrays: dimension 0 (or none) = total element count, 1..n = size of that dimension
	if (const auto nd_array = decl->cast<NodeNDArray>()) {
		// resolve wildcard index notation on the clone, exactly like the later lowering does
		if (const auto nd_ref = node.array->cast<NodeNDArrayRef>()) {
			if (nd_ref->indexes and nd_ref->num_wildcards()) {
				LoweringNumElements::handle_wildcard_notation(*nd_ref, *nd_array, node);
			}
		}
		int32_t dim = 0;
		if (node.dimension) {
			const auto dim_node = node.dimension->cast<NodeInt>();
			if (!dim_node) return &node;
			dim = dim_node->value;
		}
		const auto num_dims = m_db.get_num_dimensions(decl.get());
		if (num_dims == 0) return &node;
		// invalid dimensions are diagnosed later in lowering
		if (dim < 0 or dim > static_cast<int32_t>(num_dims)) return &node;
		if (dim > 0) {
			if (const auto size = m_db.get_array_size(decl.get(), dim - 1)) {
				return replace_with_int(node, *size);
			}
			return &node;
		}
		// dimension 0 -> product of all dimension sizes
		int64_t total = 1;
		for (size_t d = 0; d < num_dims; ++d) {
			const auto size = m_db.get_array_size(decl.get(), d);
			if (!size) return &node;
			total *= *size;
		}
		if (total > std::numeric_limits<int32_t>::max()) return &node;
		return replace_with_int(node, static_cast<int32_t>(total));
	}
	return &node;
}

