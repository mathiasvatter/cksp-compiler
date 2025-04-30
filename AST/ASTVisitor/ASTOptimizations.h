//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../misc/CommandLineOptions.h"

/** @brief Class for AST Optimizations
 * Constant Propagation
 * Variable Pruning
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {
public:
    explicit ASTOptimizations(): ASTVisitor() {}

	static bool optimize(NodeProgram& node, OptimizationLevel optimize);

protected:

	static StringTypeKey get_hash_value(NodeAST& ref) {
		std::string hash_val = ref.get_string();
		if(const auto arr_ref = ref.cast<NodeArrayRef>()) {
			if(arr_ref->index) {
				hash_val += "[" + arr_ref->index->get_string() + "]";
			}
		}
		return {hash_val, ref.ty};
	}

	static bool is_destructive_func_arg(const NodeReference* node) {
		if(const auto header_ref = node->is_func_arg()) {
			const auto func_call = header_ref->parent->cast<NodeFunctionCall>();
			if(func_call->is_destructive_builtin_func()) {
				return true;
			} else if (func_call->kind == NodeFunctionCall::Kind::UserDefined) {
				return true;
			}
		}
		return false;
	}
};
