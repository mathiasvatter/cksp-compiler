//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "Types.h"

/**
 * @class TypeRegistry
 * @brief This class is used to manage and access different types in cksp.
 *
 * The TypeRegistry class provides methods to add and get object types, and also provides predefined basic and composite types.
 */
class TypeRegistry {
public:

	inline static void add_object_type(const std::string& name) {
		object_types[name] = std::make_unique<ObjectType>(name);
	}

	inline static ObjectType* get_object_type(const std::string& name) {
		auto it = object_types.find(name);
		if (it != object_types.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	// Standardtypen
	inline static std::unique_ptr<BasicType> Integer = std::make_unique<BasicType>(Kind::Integer);
	inline static std::unique_ptr<BasicType> Boolean = std::make_unique<BasicType>(Kind::Boolean);
	inline static std::unique_ptr<BasicType> String = std::make_unique<BasicType>(Kind::String);
	inline static std::unique_ptr<BasicType> Real = std::make_unique<BasicType>(Kind::Real);
	inline static std::unique_ptr<BasicType> Void = std::make_unique<BasicType>(Kind::Void);
	inline static std::unique_ptr<BasicType> Any = std::make_unique<BasicType>(Kind::Any);

	// Composite-Typen
	inline static std::unique_ptr<CompositeType> ArrayOfInt = std::make_unique<CompositeType>(CompositeType::Array, Integer.get());
	inline static std::unique_ptr<CompositeType> ArrayOfReal = std::make_unique<CompositeType>(CompositeType::Array, Real.get());
	inline static std::unique_ptr<CompositeType> ArrayOfBool = std::make_unique<CompositeType>(CompositeType::Array, Boolean.get());
	inline static std::unique_ptr<CompositeType> ArrayOfString = std::make_unique<CompositeType>(CompositeType::Array, String.get());

	inline static std::unique_ptr<CompositeType> ListOfInt = std::make_unique<CompositeType>(CompositeType::List, Integer.get());
	inline static std::unique_ptr<CompositeType> ListOfReal = std::make_unique<CompositeType>(CompositeType::List, Real.get());
	inline static std::unique_ptr<CompositeType> ListOfBool = std::make_unique<CompositeType>(CompositeType::List, Boolean.get());
	inline static std::unique_ptr<CompositeType> ListOfString = std::make_unique<CompositeType>(CompositeType::List, String.get());

private:
	static std::unordered_map<std::string, std::unique_ptr<ObjectType>> object_types;
};