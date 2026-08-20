//
// Created by Mathias Vatter on 13.08.26.
//

#pragma once

#include "ASTLowering.h"

/**
 * Lowers a struct storage access to a generated accessor function.
 * pre:
 *     Note.storage(.pitch)
 * post lowering:
 *     Note::storage::pitch()
 *
 *     function Note::storage::pitch(): int[]
 *         return Note::pitch
 *     end function
 *
 * Struct lowering keeps one array per member that holds the value of that member for every
 * instance back to back, so the <pitch> of every <Note> lives in <Note::pitch>. That array has no
 * name in the source language and <storage> is the only way to reach it.
 *
 * Type inference resolved the selector once and recorded the deterministic heap name and type of
 * the member. It contributes the receiver type and nothing else, so the chain is still intact
 * here. The heap declaration does not need to be alive either: post-lowering variable checking
 * binds the reference below, the same way it binds the ones LoweringArrayQuery hands its helpers.
 *
 * Handing out an accessor rather than substituting the heap reference keeps the result inside the
 * regular function machinery, so a discarded or reassigned result is treated like that of any
 * other function. Since the accessor is a return-only function it is inlined as an expression
 * later on: the call site ends up holding the heap array itself and not a copy of it.
 *
 * One accessor per struct and member. Further accesses to the same member find it in the function
 * lookup and share it.
 */
class LoweringStorageAccess final : public ASTLowering {
public:
	explicit LoweringStorageAccess(NodeProgram* program) : ASTLowering(program) {}

	/// A type-qualified chain whose only method is the compiler-provided <storage>. Type inference
	/// has already rejected every malformed spelling, so this only has to recognize the shape.
	static bool is_storage_access(NodeAccessChain& node) {
		if (node.chain.size() != 2) return false;
		const auto object = node.chain[0]->is_reference();
		if (!object or object->kind != NodeReference::Kind::TypeQualifier) return false;
		const auto call = node.chain[1]->cast<NodeFunctionCall>();
		if (!call or call->function->name != NodeStruct::STORAGE) return false;
		return call->function->get_num_args() == 1
			and call->function->get_arg(0)->cast<NodeMemberPath>() != nullptr;
	}

	NodeAST* visit(NodeAccessChain& node) override {
		const auto call = node.chain[1]->cast<NodeFunctionCall>();
		const auto member_path = call->function->get_arg(0)->cast<NodeMemberPath>();
		if (!member_path->is_resolved()) {
			auto error = Diagnostic(ErrorType::InternalError, "", "", member_path->tok);
			error.message = "Storage selector was not resolved before lowering.";
			error.exit();
		}
		const auto& member = member_path->resolved_member(0);

		// The accessor takes no arguments, so the member has to be part of its name. Two members of
		// the same type would otherwise share one accessor and hand out the same heap.
		const std::string function_name = node.chain[0]->ty->ksp_encoded_string() + OBJ_DELIMITER
			+ NodeStruct::STORAGE + OBJ_DELIMITER + member_path->leaf_name();
		Type* function_type = TypeRegistry::add_function_type({}, member.heap_type);

		// check if function signature is already present
		auto definition = m_program->look_up_exact({function_name, 0}, function_type);
		if (!definition) definition = add_function(function_name, member, node.tok);

		auto header_ref = std::make_unique<NodeFunctionHeaderRef>(
			function_name,
			std::make_unique<NodeParamList>(node.tok),
			node.tok
		);
		header_ref->ty = definition->header->ty;
		auto storage_call = std::make_unique<NodeFunctionCall>(false, std::move(header_ref), node.tok);
		storage_call->kind = NodeFunctionCall::Kind::UserDefined;
		storage_call->definition = definition;
		storage_call->ty = member.heap_type;
		return node.replace_with(std::move(storage_call));
	}

private:
	/**
	 * Constructs and registers the accessor:
	 *     function Note::storage::pitch(): int[]
	 *         return Note::pitch
	 *     end function
	 */
	std::shared_ptr<NodeFunctionDefinition> add_function(
		const std::string& function_name,
		const NodeMemberPath::ResolvedMemberSegment& member,
		const Token& tok
	) const {
		auto header = std::make_unique<NodeFunctionHeader>(function_name, tok);
		header->create_function_type(member.heap_type);

		auto definition = std::make_shared<NodeFunctionDefinition>(
			std::move(header),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		definition->ty = member.heap_type;
		definition->num_return_params = 1;
		// the single return keeps this an expression function, which is what makes the call site end
		// up with the heap itself instead of a copy of it
		definition->num_return_stmts = 1;

		// No declaration is attached here deliberately. Struct lowering creates an array with this
		// deterministic name, and the post-lowering ASTVariableChecking pass links this reference
		// to that declaration.
		auto heap_ref = std::make_unique<NodeArrayRef>(member.heap_name, nullptr, tok);
		heap_ref->ty = member.heap_type;
		auto heap_return = std::make_unique<NodeReturn>(tok, std::move(heap_ref));
		heap_return->definition = definition;
		definition->body->add_as_stmt(std::move(heap_return));

		definition->collect_declarations(m_program);
		definition->collect_references();
		m_program->add_function_definition(definition);
		return definition;
	}
};
