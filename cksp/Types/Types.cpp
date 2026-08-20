//
// Created by Mathias Vatter on 20.04.24.
//

#include "Types.h"
#include "TypeRegistry.h"

Type * CompositeType::substitute_type_parameters(const TypeSubstitutions &substitutions) const {
	auto* element_type = this->m_element_type->substitute_type_parameters(substitutions);
	// nothing substituted
	if (element_type == m_element_type) {
		return const_cast<CompositeType*>(this);
	}
	return TypeRegistry::add_composite_type(
		m_compound_kind,
		element_type,
		m_dimensions
	);
}

Type * ObjectType::substitute_type_parameters(const TypeSubstitutions &substitutions) const {
	std::vector<Type*> arguments;
	bool changed = false;
	for (const auto* argument : m_arguments) {
		auto new_argument = argument->substitute_type_parameters(substitutions);
		if (new_argument != argument) changed = true;
		arguments.push_back(new_argument);
	}
	if (!changed) {
		return const_cast<ObjectType*>(this);
	}
	return TypeRegistry::add_object_type(
		m_name,
		arguments
	);
}

Type * TypeParameterType::substitute_type_parameters(const TypeSubstitutions &substitutions) const {
	const auto it = substitutions.find(this);
	if (it == substitutions.end()) {
		return const_cast<TypeParameterType*>(this);
	}
	return it->second;
}

Type * FunctionType::substitute_type_parameters(const TypeSubstitutions &substitutions) const {
	std::vector<Type*> params;
	bool changed = false;
	for (const auto* param : m_params) {
		auto new_param = param->substitute_type_parameters(substitutions);
		if (new_param != param) {
			changed = true;
		}
		params.push_back(new_param);
	}
	auto new_return_type = m_return_type->substitute_type_parameters(substitutions);
	if (new_return_type == m_return_type and !changed) {
		return const_cast<FunctionType*>(this);
	}
	return TypeRegistry::add_function_type(
		params,
		new_return_type
	);
}
