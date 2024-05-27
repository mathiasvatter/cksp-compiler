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
 * The TypeRegistry class provides methods to add and get object types, and also provides predefined basic and composite types.
 */
class TypeRegistry {
public:

	/// tries to infer the type by specializing given type from Number to Integer
	static inline Type* infer_type(NodeDataStructure* node) {
		Type* ty = node->ty;
		if(ty == Number || ty == Unknown || ty == Any) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
			error.m_message = "Failed to infer <"+ty->get_type_kind_name()+"> type.";
			error.m_message += " Automatically casted "+node->name+" as <Integer>. Consider using a variable identifier.";
			error.m_got = ty->to_string();
			error.print();
			node->ty = Integer;
			return Integer;
		}
		return ty;
	}

	inline static Type* get_type_from_annotation(const std::string& name) {
		auto it = type_annotations.find(name);
		if (it != type_annotations.end()) {
			return it->second;
		}
		return Unknown;
	}

	inline static Type* get_type_from_identifier(char identifier) {
		auto it = type_identifiers.find(identifier);
		if (it != type_identifiers.end()) {
			return it->second;
		}
		return Unknown;
	}

	inline static ObjectType* add_object_type(const std::string& name) {
		object_types[name] = std::make_unique<ObjectType>(name);
        return object_types[name].get();
	}

	inline static ObjectType* get_object_type(const std::string& name) {
		auto it = object_types.find(name);
		if (it != object_types.end()) {
			return it->second.get();
		}
		return nullptr;
	}

    inline static CompositeType* get_composite_type(CompositeType::Kind comp_type, Type* element_type, int dimensions=1) {
        auto hash_val = std::to_string(comp_type)+element_type->to_string()+std::to_string(dimensions);
        auto it = composite_types.find(hash_val);
        if (it != composite_types.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    inline static CompositeType* add_composite_type(CompositeType::Kind comp_type, Type* element_type, int dimensions=1) {
        auto hash_val = std::to_string(comp_type)+element_type->to_string()+std::to_string(dimensions);
        composite_types[hash_val] = std::make_unique<CompositeType>(comp_type, element_type, dimensions);
        return composite_types[hash_val].get();
    }


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
	inline static std::unique_ptr<CompositeType> ArrayOfIntType = std::make_unique<CompositeType>(CompositeType::Array, Integer);
	inline static CompositeType* ArrayOfInt = ArrayOfIntType.get();
	inline static std::unique_ptr<CompositeType> ArrayOfRealType = std::make_unique<CompositeType>(CompositeType::Array, Real);
	inline static CompositeType* ArrayOfReal = ArrayOfRealType.get();
	inline static std::unique_ptr<CompositeType> ArrayOfBoolType = std::make_unique<CompositeType>(CompositeType::Array, Boolean);
	inline static CompositeType* ArrayOfBool = ArrayOfBoolType.get();
	inline static std::unique_ptr<CompositeType> ArrayOfStringType = std::make_unique<CompositeType>(CompositeType::Array, String);
	inline static CompositeType* ArrayOfString = ArrayOfStringType.get();
	inline static std::unique_ptr<CompositeType> ArrayOfUnknownType = std::make_unique<CompositeType>(CompositeType::Array, Unknown);
	inline static CompositeType* ArrayOfUnknown = ArrayOfUnknownType.get();

	inline static std::unique_ptr<CompositeType> ListOfIntType = std::make_unique<CompositeType>(CompositeType::List, IntegerType.get());
	inline static std::unique_ptr<CompositeType> ListOfRealType = std::make_unique<CompositeType>(CompositeType::List, RealType.get());
	inline static std::unique_ptr<CompositeType> ListOfBoolType = std::make_unique<CompositeType>(CompositeType::List, BooleanType.get());
	inline static std::unique_ptr<CompositeType> ListOfStringType = std::make_unique<CompositeType>(CompositeType::List, StringType.get());

private:
	inline static std::unordered_map<std::string, std::unique_ptr<ObjectType>> object_types;
    inline static std::unordered_map<std::string, std::unique_ptr<CompositeType>> composite_types;

	inline static std::unordered_map<std::string, Type*> type_annotations = {
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
	inline static std::unordered_map<char, Type*> type_identifiers = {
		{'$', Integer},
		{'~', Real},
		{'@', String},
		{'%', ArrayOfInt},
		{'?', ArrayOfReal},
		{'!', ArrayOfString},
	};

};
