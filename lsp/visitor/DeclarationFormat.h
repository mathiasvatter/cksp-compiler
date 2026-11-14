//
// Shared rendering of declarations for completion details and hovers.
//

#pragma once

#include <string>
#include <vector>

#include "../../cksp/ASTNodes/ASTReferences.h"

/// How the editor shows a declaration: its type, parameter list and signature.
///
/// Names always come from the declaration token, never from NodeDataStructure::name: by the
/// time the LSP harvests run, UniqueParameterNamesProvider has uniquified parameter names and
/// struct desugaring has mangled member names, while the token keeps the source spelling.
namespace declaration_format {

inline bool is_unknown(const Type* type) {
	return !type || type->to_string() == "unknown";
}

/// The declared type, or empty when inference left it unknown.
inline std::string type_of(const NodeAST& node) {
	return is_unknown(node.ty) ? std::string{} : node.ty->to_string();
}

/// The declared name minus its qualifier path, i.e. what the user types after the dot.
inline std::string basename_of(const std::string& name) {
	const auto object_member = name.rfind(OBJ_DELIMITER);
	if (object_member != std::string::npos) {
		return name.substr(object_member + OBJ_DELIMITER.size());
	}
	const auto dot = name.rfind('.');
	return dot == std::string::npos ? name : name.substr(dot + 1);
}

/// "amount: int" per parameter, without the <self> receiver when `omit_receiver` is set.
///
/// Read tok.val rather than get_token_string(): the latter reconstructs a declaration from
/// the *name* for composite structures (NodeArray returns "name[size]"), so it carries the
/// uniquifying rename right back in.
inline std::vector<std::string> parameter_labels_of(
	const NodeFunctionHeader& header, const bool omit_receiver = false) {
	std::vector<std::string> labels;
	size_t first = 0;
	if (omit_receiver && !header.params.empty() && header.params.front()
		&& header.params.front()->variable
		&& header.params.front()->variable->tok.val == NodeStruct::SELF) {
		first = 1;
	}
	labels.reserve(header.params.size() - first);
	for (size_t i = first; i < header.params.size(); ++i) {
		const auto& parameter = header.params[i];
		if (!parameter || !parameter->variable) continue;
		std::string label = parameter->variable->tok.val;
		if (const auto type = type_of(*parameter->variable); !type.empty()) {
			label += ": " + type;
		}
		labels.push_back(std::move(label));
	}
	return labels;
}

/// "(amount: int, target: int)".
inline std::string parameters_of(const std::vector<std::string>& labels) {
	std::string parameters = "(";
	for (size_t i = 0; i < labels.size(); ++i) {
		if (i) parameters += ", ";
		parameters += labels[i];
	}
	return parameters + ")";
}

/// ": int", or empty for a function that returns nothing. The header's own type is a
/// FunctionType whose return type is Unknown then, which must not be printed.
inline std::string return_annotation_of(const NodeFunctionHeader& header) {
	const auto* function_type = header.ty ? header.ty->cast<FunctionType>() : nullptr;
	if (!function_type) return {};
	const auto* return_type = function_type->get_return_type();
	return is_unknown(return_type) ? std::string{} : ": " + return_type->to_string();
}

/// "function fade(amount: int, target: int): int".
///
/// The name comes from the token, which is how it was written: a function in a namespace is
/// declared as <reset>, while <function nks.update_labels()> carries the dots in its own name
/// and should show them.
inline std::string signature_of(
	const NodeFunctionHeader& header,
	const bool is_static,
	const bool omit_receiver = false) {
	std::string signature = is_static ? "static function " : "function ";
	signature += (header.tok.val.empty() ? basename_of(header.name) : header.tok.val)
		+ parameters_of(parameter_labels_of(header, omit_receiver))
		+ return_annotation_of(header);
	return signature;
}

}  // namespace declaration_format
