//
// Created by Mathias Vatter on 05.04.24.
//

#include "DefinitionProvider.h"

#include <utility>

#include "../Migration/TCMMigration.h"

DefinitionProvider::DefinitionProvider(
		std::unordered_map<std::string, std::shared_ptr<NodeVariable>> m_builtin_variables,
		std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_builtin_functions,
		std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_boolean_functions,
		std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> m_property_functions,
		std::unordered_map<std::string, std::shared_ptr<NodeArray>> m_builtin_arrays,
		std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> m_builtin_widgets,
		std::vector<std::shared_ptr<NodeDataStructure>> m_external_variables)
        : external_variables(std::move(m_external_variables)),
		builtin_variables(std::move(m_builtin_variables)),
		builtin_arrays(std::move(m_builtin_arrays)), // property functions
		builtin_widgets(std::move(m_builtin_widgets)),
		builtin_functions(std::move(m_builtin_functions)),
		boolean_functions(std::move(m_boolean_functions)),
		property_functions(std::move(m_property_functions)) {
	// add default scope to work as global scope
	this->add_scope();
    add_external_variables_to_global_scope();
}

DefinitionProvider::DefinitionProvider() {
	// add default scope to work as global scope
	this->add_scope();
	add_external_variables_to_global_scope();
}

bool DefinitionProvider::add_scope() {
    m_declared_data_structures.emplace_back();
    return true;
}

bool DefinitionProvider::copy_last_scope() {
	if(m_declared_data_structures.size() < 2) {
		auto diagnostic = Diagnostic(ErrorType::InternalError, "",-1, "","","");
		diagnostic.message = "Tried to copy last scope, but there is no last scope to copy.";
		diagnostic.exit();
		return false;
	}
	const auto& last_scope = m_declared_data_structures[m_declared_data_structures.size() - 2];
	auto& current_scope = m_declared_data_structures.back();
	// Optional: Reserve Platz im aktuellen Scope, um unnötige Allokationen zu vermeiden
	current_scope.reserve(current_scope.size() + last_scope.size());
	for (const auto& data_struct : last_scope) {
		current_scope.emplace(data_struct);
	}
	return true;
}

std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual>
    DefinitionProvider::remove_scope() {
    auto diagnostic = Diagnostic(ErrorType::InternalError, "",-1, "","","");
    if(m_declared_data_structures.empty()) {
        diagnostic.message = "Tried to remove global scope.";
        diagnostic.exit();
    }
	auto passive_scope = std::move(m_declared_data_structures.back());
	m_declared_data_structures.pop_back();
	return passive_scope;
}

bool DefinitionProvider::refresh_scopes() {
    m_declared_data_structures.clear();
    // add global scope
    add_scope();
    add_external_variables_to_global_scope();
    return true;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::remove_from_current_scope(const std::string& name) {
    const auto it = m_declared_data_structures.back().find(name);
    if(it != m_declared_data_structures.back().end()) {
        auto var = it->second;
        m_declared_data_structures.back().erase(it);
        return var;
    }
    return nullptr;
}


std::shared_ptr<NodeDataStructure> DefinitionProvider::get_declaration(NodeReference& var) {
	// if reference is compiler, return dummy declaration pointer
	if(const auto &dummy_decl = get_compiler_declaration(var)) {
		var.kind = NodeReference::Kind::Compiler;
		return dummy_decl;
	}
	if(const auto &pgs_decl = get_pgs_declaration(var)) {
		var.kind = NodeReference::Kind::User;
		return pgs_decl;
	}
	if(const auto &throwaway = get_throwaway_declaration(var)) {
		var.kind = NodeReference::Kind::Throwaway;
		return throwaway;
	}

	// get builtin declaration if it exists
	std::shared_ptr<NodeDataStructure> node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var.name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var.name);

	if (node_builtin_declaration) {
		var.kind = NodeReference::Kind::Builtin;
		return node_builtin_declaration;
	}

	// try not sanitized name first
	auto node_declaration = get_declared_data_structure(var.name);
	if(!node_declaration) {
		// sanitize name if array
		const std::string sanitized = var.sanitize_name();
		node_declaration = get_declared_data_structure(sanitized);
	}
	if (node_declaration) {
		var.kind = NodeReference::Kind::User;
		return node_declaration;
	}
	return nullptr;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::set_declaration(const std::shared_ptr<NodeDataStructure>& var, bool global_scope) {
	// handle_throwaway_var(*var);
	reserve_name(var->name);
	// get builtin declaration if it exists
	std::shared_ptr<NodeDataStructure> node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);

	// is declaration and is builtin -> compile error
	if (node_builtin_declaration) {
		auto diagnostic = Diagnostic(ErrorType::VariableError, "", "", var->tok);
		diagnostic.message = "Variable shadows builtin variable. Try renaming the variable.";
		diagnostic.exit();
	}

	// input var is declaration
	if (auto data_struct = get_scoped_data_structure(var->name, global_scope)) {
		auto diagnostic = Diagnostic(ErrorType::VariableError, "", "", var->tok);
		diagnostic.message = "Data Structure has already been declared in this scope. Variables with different types but same names are not allowed. ";
		if (data_struct->kind == NodeDataStructure::Compiler or var->kind == NodeDataStructure::Compiler) {
			diagnostic.message = "A variable of this name is needed internally by cksp. ";
		} else {
			diagnostic.message += var->name + " is a redeclaration of " + data_struct->tok.val + " in line " + std::to_string(data_struct->tok.line) +". ";
		}
		if(data_struct->is_function_param()) {
			diagnostic.message += "The original declaration is a function parameter. Function parameters cannot be shadowed.";
		} else {
			diagnostic.message += "Try renaming the variable to avoid shadowing.";
		}
		if(global_scope and !data_struct->is_engine) diagnostic.message += "\nVariables declared in the <init> callback are always considered global, no local scopes are created.";
		diagnostic.exit();
	} else {
		if(global_scope) {
			m_declared_data_structures.at(0).insert({var->name, var});
		} else {
			m_declared_data_structures.back().insert({var->name, var});
		}
		m_declaration_candidates.push_back(var);
	}
	return nullptr;
}


// ******************* getter and setter *******************


std::shared_ptr<NodeDataStructure> DefinitionProvider::get_declared_data_structure(const std::string& data) {
	// then search in all other scopes
	for (auto rit = m_declared_data_structures.rbegin(); rit != m_declared_data_structures.rend(); ++rit) {
		if (auto it = rit->find(data); it != rit->end()) {
			return it->second;
		}
	}
	return nullptr;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::get_scoped_data_structure(const std::string& data, const bool global_scope) const {
	const auto& map = global_scope ? m_declared_data_structures.at(0) : m_declared_data_structures.back();
	if (const auto it = map.find(data); it != map.end()) {
		return it->second;
	}
	return nullptr;
}

std::vector<std::shared_ptr<NodeDataStructure>> DefinitionProvider::get_data_structure_candidates(
	const bool include_collected) const {
	std::vector<std::shared_ptr<NodeDataStructure>> candidates;
	std::unordered_set<const NodeDataStructure*> seen;

	auto add = [&](const std::shared_ptr<NodeDataStructure>& candidate) {
		if (candidate && seen.insert(candidate.get()).second) {
			candidates.push_back(candidate);
		}
	};
	auto add_function_definitions = [&](const auto& definitions) {
		for (const auto& [_, definition] : definitions) {
			if (definition) add(definition->header);
		}
	};

	for (const auto& scope : std::ranges::reverse_view(m_declared_data_structures)) {
		for (const auto& declaration : scope | std::views::values) {
			add(declaration);
		}
	}
	for (const auto& weak_header : m_function_headers) {
		add(weak_header.lock());
	}
	for (const auto& weak_declaration : m_declaration_candidates) {
		add(weak_declaration.lock());
	}

	if (include_collected) {
		for (const auto& weak_declaration : m_all_data_structures) {
			add(weak_declaration.lock());
		}
	}

	for (const auto& declaration : external_variables) add(declaration);
	for (const auto& declaration : builtin_variables | std::views::values) add(declaration);
	for (const auto& declaration : builtin_arrays | std::views::values) add(declaration);
	for (const auto& declaration : builtin_widgets | std::views::values) add(declaration);
	add_function_definitions(builtin_functions);
	add_function_definitions(boolean_functions);
	add_function_definitions(property_functions);

	return candidates;
}

std::vector<std::shared_ptr<NodeDataStructure>> DefinitionProvider::find_data_structures(
	const std::string& name,
	const bool include_collected) const {
	auto candidates = get_data_structure_candidates(include_collected);
	std::erase_if(candidates, [&](const auto& candidate) {
		return candidate->name != name;
	});
	return candidates;
}

std::vector<std::shared_ptr<NodeDataStructure>> DefinitionProvider::misspelled_data_structures(
	const std::string& name,
	const std::optional<int> num_args,
	const size_t max_results,
	const bool include_collected) const {
	const auto lower_name = StringUtils::to_lower(name);
	const size_t name_length = name.size();
	const size_t max_distance =
		std::clamp<size_t>(name_length <= 4 ? 1 : (name_length <= 8 ? 2 : 3), 1, 4);

	struct RankedDeclaration {
		std::shared_ptr<NodeDataStructure> declaration;
		int score;
	};
	std::vector<RankedDeclaration> ranked;

	for (const auto& declaration : get_data_structure_candidates(include_collected)) {
		const auto& candidate_name = declaration->name;
		if (candidate_name.empty() || candidate_name == name
			|| StringUtils::starts_with(candidate_name, "CKSP" + OBJ_DELIMITER)) {
			continue;
		}
		if (candidate_name.size() + max_distance + 1 < name_length
			|| name_length + max_distance + 1 < candidate_name.size()) {
			continue;
		}

		const auto lower_candidate = StringUtils::to_lower(candidate_name);
		const size_t distance = StringUtils::get_levenshtein_distance(lower_name, lower_candidate);
		if (distance > max_distance) continue;

		int score = suggestion_score(lower_name, lower_candidate, distance);
		if (num_args) {
			if (const auto header = declaration->cast<NodeFunctionHeader>()) {
				score += std::abs(*num_args - static_cast<int>(header->get_num_params())) * 3;
			} else {
				// Keep non-functions in the result: the parentheses themselves may
				// be the typo, but prefer a similarly named callable declaration.
				score += 3;
			}
		}
		ranked.push_back({declaration, score});
	}

	std::ranges::sort(ranked, [](const auto& left, const auto& right) {
		if (left.score != right.score) return left.score < right.score;
		return left.declaration->name < right.declaration->name;
	});

	std::vector<std::shared_ptr<NodeDataStructure>> suggestions;
	std::unordered_set<std::string> seen_names;
	for (const auto& candidate : ranked) {
		if (suggestions.size() >= max_results) break;
		if (seen_names.insert(candidate.declaration->name).second) {
			suggestions.push_back(candidate.declaration);
		}
	}
	return suggestions;
}

std::vector<std::string> DefinitionProvider::misspelled_suggestions(
	const std::string& name,
	const size_t max_results) const {
	const bool scopes_are_empty = m_declared_data_structures.size() == 1
		&& m_declared_data_structures.front().empty();
	const auto declarations = misspelled_data_structures(
		name,
		std::nullopt,
		max_results,
		scopes_are_empty
	);

	std::vector<std::string> suggestions;
	suggestions.reserve(declarations.size());
	for (const auto& declaration : declarations) {
		suggestions.push_back(declaration->name);
	}
	return suggestions;
}

Diagnostic DefinitionProvider::make_missing_function_definition_error(
	const NodeFunctionCall& node) const {
	const auto* function = node.function.get();
	const std::string function_name = function ? function->name : node.tok.val;
	const int num_args = function ? function->get_num_args() : 0;

	// SublimeKSP's TCM has no declaration to find because CKSP needs none; say that instead
	// of listing near-miss overloads for a name that was never going to resolve.
	if (auto tcm = tcm_migration::make_diagnostic(node, function_name)) return *tcm;

	auto declarations = find_data_structures(function_name, true);
	if (function) {
		if (const auto declaration = function->get_declaration();
			declaration && std::ranges::find(declarations, declaration) == declarations.end()) {
			declarations.insert(declarations.begin(), declaration);
		}
	}

	std::vector<std::shared_ptr<NodeFunctionHeader>> same_name;
	std::vector<std::shared_ptr<NodeFunctionHeader>> same_arity;
	std::shared_ptr<NodeDataStructure> non_callable;
	for (const auto& declaration : declarations) {
		if (auto header = std::dynamic_pointer_cast<NodeFunctionHeader>(declaration)) {
			same_name.push_back(header);
			if (static_cast<int>(header->get_num_params()) == num_args) {
				same_arity.push_back(std::move(header));
			}
		} else if (!non_callable) {
			non_callable = declaration;
		}
	}

	const auto diagnostic_type = !same_arity.empty()
		? ErrorType::TypeError
		: ErrorType::SyntaxError;
	auto diagnostic = Diagnostic(
		diagnostic_type,
		"",
		"",
		function ? function->tok : node.tok
	);

	if (function) {
		std::vector<Type*> argument_types;
		if (function->args) {
			argument_types.reserve(function->args->params.size());
			for (const auto& argument : function->args->params) {
				argument_types.push_back(argument->ty ? argument->ty : TypeRegistry::Unknown);
			}
		}
		Type* return_type = TypeRegistry::Unknown;
		if (function->ty) {
			if (const auto function_type = function->ty->cast<FunctionType>()) {
				return_type = function_type->get_return_type();
			}
		}
		const FunctionType call_type(std::move(argument_types), return_type);
		diagnostic.actual = function_name + call_type.to_string();
	} else {
		diagnostic.actual = function_name;
	}

	if (!same_arity.empty()) {
		diagnostic.message = "No compatible overload of <" + function_name
			+ "> was found for the given argument types.";
	} else if (!same_name.empty()) {
		diagnostic.message = "No overload of <" + function_name + "> accepts "
			+ std::to_string(num_args) + (num_args == 1 ? " argument." : " arguments.");
	} else if (non_callable) {
		diagnostic.message = "A declaration named <" + function_name
			+ "> exists, but its type is not callable.";
		diagnostic.expected = "Function";
		diagnostic.actual = non_callable->name + ": "
			+ (non_callable->ty ? non_callable->ty->to_string() : "unknown");
	} else {
		diagnostic.message = "Function <" + function_name + "> has not been declared.";
		const auto suggestions = misspelled_data_structures(
			function_name,
			num_args,
			4,
			true
		);
		if (!suggestions.empty()) {
			std::vector<std::string> names;
			names.reserve(suggestions.size());
			for (const auto& suggestion : suggestions) {
				names.push_back(suggestion->name);
			}
			diagnostic.message += " Did you mean: " + StringUtils::join(names, ", ") + "?";
			diagnostic.fix = make_name_case_fix(
				function_name, names, function ? function->tok : node.tok);
		}
	}

	const auto& available = same_arity.empty() ? same_name : same_arity;
	if (!available.empty()) {
		std::vector<std::string> signatures;
		std::unordered_set<std::string> seen_signatures;
		for (const auto& header : available) {
			const std::string signature = header->name
				+ (header->ty ? header->ty->to_string() : "(unknown): unknown");
			if (seen_signatures.insert(signature).second) {
				signatures.push_back(signature);
			}
		}
		diagnostic.add_message(
			"Available " + std::string(signatures.size() == 1 ? "signature:\n  " : "overloads:\n  ")
			+ StringUtils::join(signatures, "\n  ")
		);
	}

	return diagnostic;
}

void DefinitionProvider::set_external_variables(std::vector<std::shared_ptr<NodeDataStructure>> external_variables) {
	DefinitionProvider::external_variables = std::move(external_variables);
}

std::shared_ptr<NodeVariable> DefinitionProvider::get_builtin_variable(const std::string& var) {
	if(const auto it = builtin_variables.find(var); it != builtin_variables.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_builtin_variables(std::unordered_map<std::string, std::shared_ptr<NodeVariable>> builtin_variables) {
	DefinitionProvider::builtin_variables = std::move(builtin_variables);
}

std::shared_ptr<NodeArray> DefinitionProvider::get_builtin_array(const std::string& arr) {
	if(const auto it = builtin_arrays.find(arr); it != builtin_arrays.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_builtin_arrays(std::unordered_map<std::string, std::shared_ptr<NodeArray>> builtin_arrays) {
	DefinitionProvider::builtin_arrays = std::move(builtin_arrays);
}

std::shared_ptr<NodeUIControl> DefinitionProvider::get_builtin_widget(const std::string &ui_control) {
	if(const auto it = builtin_widgets.find(ui_control); it != builtin_widgets.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_builtin_widgets(std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> builtin_widgets) {
	DefinitionProvider::builtin_widgets = std::move(builtin_widgets);
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_builtin_function(const NodeFunctionHeaderRef *function) {
	if (!function || !function->args) {
		return nullptr;
	}
	const auto it = builtin_functions.find({function->name, (int)function->args->size()});
	if(it != builtin_functions.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_builtin_function(const std::string &name,
	const int num_params) {
	const auto it = builtin_functions.find({name, num_params});
	if(it != builtin_functions.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_builtin_functions(std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> builtin_functions) {
	DefinitionProvider::builtin_functions = std::move(builtin_functions);
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_boolean_function(const std::string &name, const int arg_count) {
	const auto it = boolean_functions.find({name, arg_count});
	if(it != boolean_functions.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_boolean_functions(
	std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> boolean_functions) {
	DefinitionProvider::boolean_functions = std::move(boolean_functions);
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_property_function(const NodeFunctionHeaderRef *function) {
	if (!function || !function->args) {
		return nullptr;
	}
	if(auto it = property_functions.find(function->name); it != property_functions.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_property_functions(std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> property_functions) {
	DefinitionProvider::property_functions = std::move(property_functions);
}
