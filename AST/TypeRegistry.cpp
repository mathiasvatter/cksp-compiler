//
// Created by Mathias Vatter on 27.05.24.
//
#include "TypeRegistry.h"

// Implementierung der Initialisierungsmethode
void TypeRegistry::initialize() {
    // Initialisierung der Standardtypen
    IntegerType = std::make_unique<BasicType>(Kind::Integer);
    Integer = IntegerType.get();
    BooleanType = std::make_unique<BasicType>(Kind::Boolean);
    Boolean = BooleanType.get();
    ComparisonType = std::make_unique<BasicType>(Kind::Comparison);
    Comparison = ComparisonType.get();
    StringType = std::make_unique<BasicType>(Kind::String);
    String = StringType.get();
    RealType = std::make_unique<BasicType>(Kind::Real);
    Real = RealType.get();
    VoidType = std::make_unique<BasicType>(Kind::Void);
    Void = VoidType.get();
    AnyType = std::make_unique<BasicType>(Kind::Any);
    Any = AnyType.get();
    UnknownType = std::make_unique<BasicType>(Kind::Unknown);
    Unknown = UnknownType.get();
    NumberType = std::make_unique<BasicType>(Kind::Number);
    Number = NumberType.get();

    // Initialisierung der Composite-Typen
    ArrayOfInt = add_composite_type(CompoundKind::Array, Integer, 1);
    ArrayOfReal = add_composite_type(CompoundKind::Array, Real, 1);
    ArrayOfBool = add_composite_type(CompoundKind::Array, Boolean, 1);
    ArrayOfString = add_composite_type(CompoundKind::Array, String, 1);
    ArrayOfUnknown = add_composite_type(CompoundKind::Array, Unknown, 1);

    // Initialisierung der Maps
    annotation_to_type = {
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

    identifier_to_type = {
            {'$', Integer},
            {'~', Real},
            {'@', String},
            {'%', ArrayOfInt},
            {'?', ArrayOfReal},
            {'!', ArrayOfString},
    };

    type_to_identifier = invert_map(identifier_to_type);
}

Type *TypeRegistry::get_type_from_annotation(const std::string &name) {
    auto it = annotation_to_type.find(name);
    if (it != annotation_to_type.end()) {
        return it->second;
    }
    return Unknown;
}

Type *TypeRegistry::get_type_from_identifier(char identifier) {
    auto it = identifier_to_type.find(identifier);
    if (it != identifier_to_type.end()) {
        return it->second;
    }
    return Unknown;
}

char TypeRegistry::get_identifier_from_type(Type *ty) {
    auto it = type_to_identifier.find(ty);
    if (it != type_to_identifier.end()) {
        return it->second;
    }
    return ' ';
}

std::unique_ptr<NodeAST> TypeRegistry::get_neutral_element_from_type(Type* ty) {
	if (ty == Integer) {
		return std::make_unique<NodeInt>(0, Token());
	} else if (ty == Real) {
		return std::make_unique<NodeReal>(0.0, Token());
	} else if (ty == String) {
		return std::make_unique<NodeString>("\"\"", Token());
	} else if (ty == Boolean) {
		return std::make_unique<NodeString>("false", Token());
	}
	return nullptr;
}


ObjectType *TypeRegistry::add_object_type(const std::string &name) {
    object_types[name] = std::make_unique<ObjectType>(name);
    return object_types[name].get();
}

ObjectType *TypeRegistry::get_object_type(const std::string &name) {
    auto it = object_types.find(name);
    if (it != object_types.end()) {
        return it->second.get();
    }
    return nullptr;
}

CompositeType *TypeRegistry::get_composite_type(CompoundKind comp_type, Type *element_type, int dimensions) {
    auto hash_val = std::to_string((int)comp_type)+element_type->to_string()+std::to_string(dimensions);
    auto it = composite_types.find(hash_val);
    if (it != composite_types.end()) {
        return it->second.get();
    }
    return nullptr;
}

CompositeType *TypeRegistry::add_composite_type(CompoundKind comp_type, Type *element_type, int dimensions) {
    auto hash_val = std::to_string((int)comp_type)+element_type->to_string()+std::to_string(dimensions);
    composite_types[hash_val] = std::make_unique<CompositeType>(comp_type, element_type, dimensions);
    return composite_types[hash_val].get();
}
