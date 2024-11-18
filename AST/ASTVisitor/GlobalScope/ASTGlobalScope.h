//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "../../../misc/Gensym.h"

/**
 * @class ASTGlobalScope
 * @brief Optimizes the AST by handling the dynamic extension of variables, renaming local variables,
 * promoting parameters, and optimizing callbacks.
 *
 * This class performs the transformation from local scope to global scope by utilizing register reuse and parameter
 * promotion. The main steps are:
 *
 * 1. Analyzing the dynamic extend of variables/arrays within different scopes in functions and replacing them with
 *    variables whose dynamic extend has expired. Declarations are replaced by assignments with neutral elements.
 *     - Creates a map of "passive variables" with hash values based on variable types (and array sizes). Once a scope is exited, variables whose dynamic extension has expired are added to the map.
 *     - For each new declaration, it checks if a variable with the same type already exists in the map. If so, the variable is replaced by a reference to the map.
 *     - Replaced declarations are substituted by assignments with neutral elements.
 * 2. Renaming all local variables using Gensym to prevent variable capturing.
 *     - Tracks all variables and references in lists and renames them using Gensym to avoid variable capturing when a free "passive variable" has the same name as a variable in the scope.
 * 3. Promoting parameters (Lambda Lifting) for all functions until all variables are in callbacks.
 *     - Initially performs the first three steps only with function definitions.
 *     - Visits each function call up to the last nested call, adds local declarations as new parameters, and maps pointers to the next higher function definitions.
 *     - Repeats this process until the function call stack is empty and inserts the declarations into the callback.
 * 4. Executing steps 1 and 2 for all callbacks without visiting functions.
 *     - Replaces the declarations in the callbacks with passive variables.
 *     - Adds all declarations to the init callback.
 *     - Renames with Gensym.
 *
 * @note The first class called by this process is ASTRegisterReuse, followed by ASTParameterPromotion.
 *
 * @section Challenges and solutions:
 * - Array sizes are not always known, and array reuse currently depends only on type and dimension.
 *     * Solution: Add not only the type but also size and other factors affecting array reuse (persistence, const) to the hash value in the map of passive variables.
 *     * Problem: Size can also be an expression or a constant (consider constant propagation beforehand).
 * - TODO: Lambda Lifting increases the overhead of function parameters, which may require previously callable functions (without parameters) to be inlined, leading to more overhead and longer code.
 *     * Solution?: A global stack that passes all function parameters beforehand and retrieves them afterward.
 * - Arrays need to be reinitialized when reused, which is not directly supported in KSP.
 *     * Solution: Perform Array Assignment Lowering. Create a function (already parameter promoted) used where arrays are initialized, with the array type in the name. Perform this process before converting to the global scope, so the iterator of the while loop can be directly replaced by passive variables.
 *
 */
class ASTGlobalScope : public ASTVisitor {
protected:
	DefinitionProvider* m_def_provider;
	/// removes array declarations
	static inline std::unique_ptr<NodeAST> to_assign_statement(NodeSingleDeclaration& node) {
		if(node.is_promoted) {
			return std::make_unique<NodeDeadCode>(node.tok);
		}
		auto node_assignment = node.to_assign_stmt();
		if (node_assignment->l_value->get_node_type() == NodeType::ArrayRef) {
			auto node_array_ref = static_cast<NodeArrayRef *>(node_assignment->l_value.get());
			if (!node_array_ref->index and node_assignment->r_value->get_node_type() == NodeType::ParamList) {
				return std::make_unique<NodeDeadCode>(node.tok);
			}
		}
		return std::move(node_assignment);
	};

	bool inline is_thread_safe_env() {
		return (m_program->current_callback and m_program->current_callback->is_thread_safe) or
			(m_program->get_current_function() and m_program->get_current_function()->is_thread_safe);
	};

	static inline std::string get_passive_var_hash(NodeDataStructure& data) {
		auto hash = data.ty->to_string();
		// add size if it is array
		if(data.get_node_type() == NodeType::Array) {
			auto array = static_cast<NodeArray*>(&data);
			if(array->size) hash += array->size->get_string();
		}
		if(data.persistence.has_value()) hash += data.persistence.value().val;
		return hash;
	}

public:
	explicit ASTGlobalScope(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	NodeAST * visit(NodeProgram& node) override;

};
