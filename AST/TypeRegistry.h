//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "Types.h"
#include "ASTNodes/AST.h"

/**
 * @class TypeRegistry
 * @brief This class is used to manage and access different types in cksp.
 *
 * The TypeRegistry class provides methods to add and get object types, and also
 * provides predefined basic and composite types.
 */
class TypeRegistry {
private:
    inline static std::unordered_map<std::string, std::unique_ptr<ObjectType>> object_types;
    inline static std::unordered_map<std::string, std::unique_ptr<CompositeType>> composite_types;

public:
    /// returns the type from the annotation name (int, bool, string[], ...)
    static Type* get_type_from_annotation(const std::string& name);
    /// returns the type from the identifier ($, ~, @, ...)
    static Type* get_type_from_identifier(char identifier);
    /// returns the identifier from the type (Integer -> $, Real -> ~, ...)
    static char get_identifier_from_type(Type* ty);
    /// returns the neutral element from the type (Integer -> 0, Real -> 0.0, ...)
    static std::unique_ptr<NodeAST> get_neutral_element_from_type(Type* ty);
    /// adds a new object type to the registry
    static ObjectType* add_object_type(const std::string& name);
    /// returns the object type from the name, if no object type with the name exists, nullptr is returned
    static ObjectType* get_object_type(const std::string& name);
    /// returns pointer to the composite type in registry by looking at the element type and dimensions
    /// returns nullptr if no composite type with the given element type and dimensions exists
    static CompositeType* get_composite_type(CompoundKind comp_type, Type* element_type, int dimensions=1);
    /// adds a new composite type to the registry
    static CompositeType* add_composite_type(CompoundKind comp_type, Type* element_type, int dimensions=1);

    // Standardtypen
	inline static std::unique_ptr<BasicType> IntegerType = std::make_unique<BasicType>(Kind::Integer);
	inline static BasicType* Integer = IntegerType.get();
	inline static std::unique_ptr<BasicType> BooleanType = std::make_unique<BasicType>(Kind::Boolean);
	inline static BasicType* Boolean = BooleanType.get();
    inline static std::unique_ptr<BasicType> ComparisonType = std::make_unique<BasicType>(Kind::Comparison);
    inline static BasicType* Comparison = ComparisonType.get();
	inline static std::unique_ptr<BasicType> StringType = std::make_unique<BasicType>(Kind::String);
	inline static BasicType* String = StringType.get();
	inline static std::unique_ptr<BasicType> RealType = std::make_unique<BasicType>(Kind::Real);
	inline static BasicType* Real = RealType.get();
	inline static std::unique_ptr<BasicType> VoidType = std::make_unique<BasicType>(Kind::Void);
	inline static BasicType* Void = VoidType.get();
	// allgemein typen
	inline static std::unique_ptr<BasicType> AnyType = std::make_unique<BasicType>(Kind::Any);
	inline static BasicType* Any = AnyType.get();
	inline static std::unique_ptr<BasicType> UnknownType = std::make_unique<BasicType>(Kind::Unknown);
	inline static BasicType* Unknown = UnknownType.get();
	inline static std::unique_ptr<BasicType> NumberType = std::make_unique<BasicType>(Kind::Number);
	inline static BasicType* Number = NumberType.get();

	// Composite-Typen
	inline static CompositeType* ArrayOfInt = add_composite_type(CompoundKind::Array, Integer, 1);
	inline static CompositeType* ArrayOfReal = add_composite_type(CompoundKind::Array, Real, 1);
	inline static CompositeType* ArrayOfBool = add_composite_type(CompoundKind::Array, Boolean, 1);
	inline static CompositeType* ArrayOfString = add_composite_type(CompoundKind::Array, String, 1);
	inline static CompositeType* ArrayOfUnknown = add_composite_type(CompoundKind::Array, Unknown, 1);

private:
	inline static std::unordered_map<std::string, Type*> annotation_to_type = {
		{"int", Integer},
		{"real", Real},
		{"string", String},
		{"bool", Boolean},
		{"void", Void},
		{"any", Any},
		{"int[]", ArrayOfInt},
		{"real[]", ArrayOfReal},
		{"string[]", ArrayOfString},
		{"bool[]", ArrayOfBool},
		{"[]", ArrayOfUnknown}
	};
	inline static std::unordered_map<char, Type*> identifier_to_type = {
		{'$', Integer},
		{'~', Real},
		{'@', String},
		{'%', ArrayOfInt},
		{'?', ArrayOfReal},
		{'!', ArrayOfString},
	};
    inline static std::unordered_map<Type*, char> type_to_identifier = invert_map(identifier_to_type);

};
