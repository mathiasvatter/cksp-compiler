//
// Created by Mathias Vatter on 07.08.26.
//

#pragma once

#include "ASTLowering.h"

/**
 * Lowers object-array queries to generated, typed helper functions.
 * pre:
 *     search_by(zones, .id, value)
 * post lowering:
 *     Zone::search_by(zones, Zone::id, value)
 *
 * The member heaps are explicit arguments of the generated helper. This makes
 * one helper reusable for all paths with the same type signature and avoids
 * encoding member names in the helper name. For a nested path, one heap is
 * passed per segment.
 */
class LoweringArrayQuery final : public ASTLowering {
public:
	explicit LoweringArrayQuery(NodeProgram* program) : ASTLowering(program) {}

	/**
	 * A nested selector contributes one heap argument per path segment:
	 *
	 *     search_by(items, .child.id, value)
	 *     Item::search_by(items, Item::child, Child::id, value)
	 *
	 * The helper is shared by calls with the same object, path and value types.
	 * The concrete member heaps remain call arguments, so two equally typed
	 * fields such as <.id> and <.start> can use the same helper definition.
	 */
	NodeAST* visit(NodeArrayQuery& node) override {
		if (node.query_kind != NodeArrayQuery::QueryKind::SearchBy) {
			auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
			error.message = "Unsupported object-array query kind during lowering.";
			error.exit();
		}

		// Type inference resolved the selector once and recorded the deterministic
		// heap name and type of every segment. The concrete heap declarations do not
		// need to be alive yet; post-lowering variable checking binds the references
		// constructed below.
		const auto array_type = node.array->ty->cast<CompositeType>();
		Type* object_type = array_type ? array_type->get_element_type() : node.ty;
		if (!node.member_path->is_resolved()) {
			auto error = Diagnostic(ErrorType::InternalError, "", "", node.member_path->tok);
			error.message = "Array-query member path was not resolved before lowering.";
			error.exit();
		}

		// Every path segment must produce a one-dimensional scalar heap. For
		// <.child.id>, the stored descriptors name <Item::child> and <Child::id>.
		std::vector<Type*> heap_types;
		heap_types.reserve(node.member_path->segments.size());
		for (size_t i = 0; i < node.member_path->segments.size(); ++i) {
			const auto& member = node.member_path->resolved_member(i);
			if (!member.has_scalar_heap) {
				auto error = Diagnostic(ErrorType::TypeError, "", "", node.member_path->segment(i));
				error.message = "Member <" + node.member_path->segment(i).val
					+ "> cannot be used in <search_by> because it has no scalar object heap.";
				error.actual = member.value_type->to_string();
				error.exit();
			}
			heap_types.push_back(member.heap_type);
		}

		// The hidden heap parameters are part of the function type. Consequently,
		// helpers of different path depth or element type overload naturally.
		const std::string function_name = object_type->to_string() + OBJ_DELIMITER + node.query_name();
		std::vector<Type*> param_types{node.array->ty};
		param_types.insert(param_types.end(), heap_types.begin(), heap_types.end());
		param_types.push_back(node.value->ty);
		Type* function_type = TypeRegistry::add_function_type(param_types, object_type);

		// check if function signature is already present
		auto definition = m_program->look_up_exact(
			{function_name, static_cast<int>(param_types.size())},
			function_type
		);
		if (!definition) {
			definition = add_function(
				function_name,
				object_type,
				node.array->ty,
				heap_types,
				node.value->ty,
				node.tok
			);
		}

		// Materialize the normal FunctionCall that replaces NodeArrayQuery. From
		// here on all generic function passes can handle the query.
		auto args = std::make_unique<NodeParamList>(node.tok);
		args->add_param(std::move(node.array));
		for (size_t i = 0; i < node.member_path->segments.size(); ++i) {
			const auto& member = node.member_path->resolved_member(i);
			args->add_param(make_heap_ref(member, node.member_path->segment(i)));
		}
		args->add_param(std::move(node.value));

		auto header_ref = std::make_unique<NodeFunctionHeaderRef>(
			function_name,
			std::move(args),
			node.tok
		);
		header_ref->ty = definition->header->ty;
		auto call = std::make_unique<NodeFunctionCall>(false, std::move(header_ref), node.tok);
		call->kind = NodeFunctionCall::Kind::UserDefined;
		call->definition = definition;
		call->ty = object_type;

		return node.replace_with(std::move(call));
	}

private:
	/**
	 * Creates an unsized array parameter for one of the generated helpers array views.
	 *     function Zone::search_by(objects: Zone[], id_heap: int[], value: int)
	 */
	static std::unique_ptr<NodeArray> make_array_param(const std::string& name, Type* type, const Token& tok) {
		auto param = std::make_unique<NodeArray>(
			std::nullopt,
			name,
			type,
			nullptr,
			tok,
			DataType::Param
		);
		param->is_local = true;
		return param;
	}

	/**
	 * Creates the unresolved reference to a generated struct-member heap.
	 *
	 *     .id  ->  Zone::id
	 *
	 * No declaration is attached here deliberately. Struct lowering has already
	 * created an array with this deterministic name, and the post-lowering
	 * ASTVariableChecking pass links this reference to that declaration.
	 */
	static std::unique_ptr<NodeArrayRef> make_heap_ref(
		const NodeMemberPath::ResolvedMemberSegment& member,
		const Token& tok
	) {
		auto heap_ref = std::make_unique<NodeArrayRef>(member.heap_name, nullptr, tok);
		heap_ref->ty = member.heap_type;
		return heap_ref;
	}

	/**
	 * Builds a search call using NodeSortSearch
	 * Without a start position:
	 *     search(heap, value)
	 *
	 * With a start position, the explicit upper bound selects the matching
	 * four-argument KSP builtin and prevents searching past the heap:
	 *
	 *     search(heap, value, from, num_elements(heap) - 1)
	 */
	static std::unique_ptr<NodeSortSearch> make_search(const std::shared_ptr<NodeDataStructure>& array,
		std::unique_ptr<NodeAST> value,
		const Token& tok,
		std::unique_ptr<NodeAST> from = nullptr
	) {
		auto array_ref = array->to_reference();
		auto search = std::make_unique<NodeSortSearch>(
			"search",
			std::move(array_ref),
			std::move(value),
			tok
		);
		search->ty = TypeRegistry::Integer;
		if (from) {
			search->set_from(std::move(from));
			auto num_elements = std::make_unique<NodeNumElements>(array->to_reference(), nullptr, tok);
			num_elements->ty = TypeRegistry::Integer;
			search->set_to(std::make_unique<NodeBinaryExpr>(
				token::SUB,
				std::move(num_elements),
				std::make_unique<NodeInt>(1, tok),
				tok
			));
		}
		return search;
	}

	/**
	 * Builds one level of the reverse heap walk used by <search_by>.
	 * For the direct path <Zone.id>, level 0 models:
	 *     candidate0 := search(Zone::id, value)
	 *     while (candidate0 # -1)
	 *         if (search(zones, candidate0) # -1)
	 *             return candidate0
	 *         end if
	 *         candidate0 := search(
	 *             Zone::id,
	 *             value,
	 *             candidate0 + 1,
	 *             num_elements(Zone::id) - 1
	 *         )
	 *     end while
	 * A nested path is searched from its leaf back to the queried object. For
	 * <Item.child.id>, level 1 finds matching Child indices in <Child::id> and
	 * recursively creates level 0 to find Item indices in <Item::child>:
	 *     child_candidate := search(Child::id, value)
	 *     while (child_candidate # -1)
	 *         item_candidate := search(Item::child, child_candidate)
	 *         while (item_candidate # -1)
	 *             if (search(items, item_candidate) # -1)
	 *                 return item_candidate
	 *             end if
	 *             item_candidate := search(Item::child, child_candidate, ...)
	 *         end while
	 *         child_candidate := search(Child::id, value, ...)
	 *     end while
	 * Continuing every search from <candidate + 1> is the backtracking step:
	 * it considers duplicate field values and multiple parents referencing the
	 * same child instead of returning only the first heap match.
	 */
	static std::unique_ptr<NodeBlock> make_search_level(
		const size_t level,
		const std::vector<std::shared_ptr<NodeDataStructure>>& heap_params,
		const std::vector<std::shared_ptr<NodeVariable>>& candidates,
		const std::shared_ptr<NodeDataStructure>& array_param,
		std::unique_ptr<NodeAST> needle,
		const std::shared_ptr<NodeFunctionDefinition>& definition,
		const Token& tok
	) {
		auto block = std::make_unique<NodeBlock>(tok);
		block->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			candidates[level]->to_reference(),
			make_search(heap_params[level], needle->clone(), tok),
			tok
		));

		auto loop_body = std::make_unique<NodeBlock>(tok, true);
		if (level == 0) {
			auto node_return = std::make_unique<NodeReturn>(tok, candidates[0]->to_reference());
			node_return->definition = definition;

			auto found_in_array = std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				make_search(array_param, candidates[0]->to_reference(), tok),
				std::make_unique<NodeInt>(-1, tok),
				tok
			);
			auto found = std::make_unique<NodeIf>(tok);
			found->set_condition(std::move(found_in_array));
			found->if_body->add_as_stmt(std::move(node_return));
			loop_body->add_as_stmt(std::move(found));
		} else {
			loop_body->append_body(make_search_level(
				level - 1,
				heap_params,
				candidates,
				array_param,
				candidates[level]->to_reference(),
				definition,
				tok
			));
		}

		loop_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			candidates[level]->to_reference(),
			make_search(
				heap_params[level],
				std::move(needle),
				tok,
				std::make_unique<NodeBinaryExpr>(
					token::ADD,
					candidates[level]->to_reference(),
					std::make_unique<NodeInt>(1, tok),
					tok
				)
			),
			tok
		));

		auto has_candidate = std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			candidates[level]->to_reference(),
			std::make_unique<NodeInt>(-1, tok),
			tok
		);
		block->add_as_stmt(std::make_unique<NodeWhile>(
			std::move(has_candidate),
			std::move(loop_body),
			tok
		));
		return block;
	}

	/**
	 * Constructs and registers the complete generated function definition.
	 * For <search_by(zones, .id, value)> it models approximately:
	 *     function Zone::search_by(zones: Zone[], id_heap: int[], value: int)
	 *         declare candidate0: int
	 *         candidate0 := search(id_heap, value)
	 *         while (candidate0 # -1)
	 *             if (search(zones, candidate0) # -1)
	 *                 return candidate0
	 *             end if
	 *             candidate0 := search(
	 *                 id_heap,
	 *                 value,
	 *                 candidate0 + 1,
	 *                 num_elements(id_heap) - 1
	 *             )
	 *         end while
	 *
	 *         return nil
	 *     end function
	 * For nested paths the header receives additional heap parameters and
	 * <make_search_level()> inserts the corresponding nested search loops.
	 */
	std::shared_ptr<NodeFunctionDefinition> add_function(
		const std::string& function_name,
		Type* return_type,
		Type* array_type,
		const std::vector<Type*>& heap_types,
		Type* value_type,
		const Token& tok
	) {
		auto header = std::make_unique<NodeFunctionHeader>(function_name, tok);

		auto array_param = make_array_param(
			m_def_provider->get_fresh_name("search_by_array"),
			array_type,
			tok
		);
		auto array_param_shared = std::shared_ptr<NodeDataStructure>(std::move(array_param));
		header->add_param(array_param_shared);

		std::vector<std::shared_ptr<NodeDataStructure>> heap_params;
		heap_params.reserve(heap_types.size());
		for (Type* heap_type : heap_types) {
			auto heap_param = make_array_param(
				m_def_provider->get_fresh_name("search_by_heap"),
				heap_type,
				tok
			);
			auto heap_param_shared = std::shared_ptr<NodeDataStructure>(std::move(heap_param));
			header->add_param(heap_param_shared);
			heap_params.push_back(std::move(heap_param_shared));
		}

		auto value_param = std::make_shared<NodeVariable>(
			std::nullopt,
			m_def_provider->get_fresh_name("search_by_value"),
			value_type,
			tok,
			DataType::Param
		);
		value_param->is_local = true;
		header->add_param(value_param);
		header->create_function_type(return_type);

		auto definition = std::make_shared<NodeFunctionDefinition>(
			std::move(header),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		definition->ty = return_type;
		definition->num_return_params = 1;
		definition->num_return_stmts = 2;

		std::vector<std::shared_ptr<NodeVariable>> candidates;
		candidates.reserve(heap_types.size());
		for (size_t i = 0; i < heap_types.size(); ++i) {
			auto candidate = std::make_shared<NodeVariable>(
				std::nullopt,
				m_def_provider->get_fresh_name("search_by_candidate"),
				TypeRegistry::Integer,
				tok,
				DataType::Mutable
			);
			candidate->is_local = true;
			definition->body->add_as_stmt(std::make_unique<NodeSingleDeclaration>(candidate, nullptr, tok));
			candidates.push_back(std::move(candidate));
		}

		definition->body->append_body(make_search_level(
			heap_types.size() - 1,
			heap_params,
			candidates,
			array_param_shared,
			value_param->to_reference(),
			definition,
			tok
		));

		auto nil_return = std::make_unique<NodeReturn>(tok, std::make_unique<NodeNil>(tok));
		nil_return->definition = definition;
		definition->body->add_as_stmt(std::move(nil_return));
		definition->collect_declarations(m_program);
		definition->collect_references();
		m_program->add_function_definition(definition);
		return definition;
	}


};
