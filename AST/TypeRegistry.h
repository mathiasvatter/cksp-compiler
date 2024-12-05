//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "Types.h"
#include <unordered_map>

/**
 * @class TypeRegistry
 * @brief This class is used to manage and access different types in cksp.
 *
 * The TypeRegistry class provides methods to add and get object types, and also
 * provides predefined basic and composite types.
 */
class TypeRegistry {
public:
    /// Initialisierungsmethode
    static void initialize();

    /// returns the type from the annotation name (int, bool, string[], ...)
    static Type* get_type_from_annotation(const std::string& name);
    /// returns the annotation from the type (Integer -> int, Real -> real, ...)
    static std::string get_annotation_from_type(Type* ty);
    /// returns the type from the identifier ($, ~, @, ...)
    static Type* get_type_from_identifier(char identifier);
    /// returns the identifier from the type (Integer -> $, Real -> ~, ...)
    static char get_identifier_from_type(Type* ty);
    /// returns the neutral element from the type (Integer -> 0, Real -> 0.0, ...)
    static std::unique_ptr<struct NodeAST> get_neutral_element_from_type(Type* ty);
    /// adds a new object type to the registry, if object type already exists, the existing type is returned
    static ObjectType* add_object_type(const std::string& name);
    /// returns the object type from the name, if no object type with the name exists, nullptr is returned
    static ObjectType* get_object_type(const std::string& name);
    /// returns pointer to the composite type in registry by looking at the element type and dimensions
    /// returns nullptr if no composite type with the given element type and dimensions exists
    static CompositeType* get_composite_type(CompoundKind comp_type, Type* element_type, int dimensions=1);
    /// adds a new composite type to the registry, if composite type already exists, the existing type is returned
    static CompositeType* add_composite_type(CompoundKind comp_type, Type* element_type, int dimensions=1);

	static FunctionType* get_function_type(std::vector<Type*> params, Type* return_type);
	static FunctionType* add_function_type(const std::vector<Type*>& params, Type* return_type);

    // Deklaration der Standardtypen
    static inline std::unique_ptr<BasicType> IntegerType;
    static inline BasicType* Integer;
    static inline std::unique_ptr<BasicType> BooleanType;
    static inline BasicType* Boolean;
    static inline std::unique_ptr<BasicType> ComparisonType;
    static inline BasicType* Comparison;
    static inline std::unique_ptr<BasicType> StringType;
    static inline BasicType* String;
    static inline std::unique_ptr<BasicType> RealType;
    static inline BasicType* Real;
    static inline std::unique_ptr<BasicType> VoidType;
    static inline BasicType* Void;
    static inline std::unique_ptr<BasicType> AnyType;
    static inline BasicType* Any;
    static inline std::unique_ptr<BasicType> UnknownType;
    static inline BasicType* Unknown;
    static inline std::unique_ptr<BasicType> NumberType;
    static inline BasicType* Number;
	static inline std::unique_ptr<BasicType> PGSType;
	static inline BasicType* PGS;

    // Deklaration der Composite-Typen
    static inline CompositeType* ArrayOfInt;
    static inline CompositeType* ArrayOfReal;
    static inline CompositeType* ArrayOfBool;
    static inline CompositeType* ArrayOfString;
    static inline CompositeType* ArrayOfUnknown;

	static inline CompositeType* NDArrayOfInt;
	static inline CompositeType* NDArrayOfUnknown;

	static inline ObjectType* Nil;

private:
    static inline std::unordered_map<std::string, std::unique_ptr<ObjectType>> object_types;
    static inline std::unordered_map<std::string, std::unique_ptr<CompositeType>> composite_types;
	static inline std::unordered_map<std::string, std::unique_ptr<FunctionType>> function_types;

    static inline std::unordered_map<std::string, Type*> annotation_to_type;
    static inline std::unordered_map<Type*, std::string> type_to_annotation;
    static inline std::unordered_map<char, Type*> identifier_to_type;
    static inline std::unordered_map<Type*, char> type_to_identifier;
};