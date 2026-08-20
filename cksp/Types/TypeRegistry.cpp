//
// Created by Mathias Vatter on 27.05.24.
//

#include "../Types/TypeRegistry.h"
#include "../ASTNodes/ASTDataStructures.h"
#include "../ASTNodes/ASTReferences.h"

// Implementation of the initialization method
void TypeRegistry::initialize() {
    object_types.clear();
    composite_types.clear();
    function_types.clear();
    annotation_to_type.clear();
    type_to_annotation.clear();
    identifier_to_type.clear();
    type_to_identifier.clear();

    // Initialization of standard types
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
	PGSType = std::make_unique<BasicType>(Kind::PGS);
	PGS = PGSType.get();

    // Initialization of composite types
    ArrayOfInt = add_composite_type(CompoundKind::Array, Integer, 1);
    ArrayOfReal = add_composite_type(CompoundKind::Array, Real, 1);
    ArrayOfBool = add_composite_type(CompoundKind::Array, Boolean, 1);
    ArrayOfString = add_composite_type(CompoundKind::Array, String, 1);
    ArrayOfUnknown = add_composite_type(CompoundKind::Array, Unknown, 1);

	NDArrayOfInt = add_composite_type(CompoundKind::Array, Integer, 0);
	NDArrayOfUnknown = add_composite_type(CompoundKind::Array, Unknown, 0);

	Nil = add_object_type("nil");

    // Initialization of the maps
    annotation_to_type = {
            {"int", Integer},
            {"real", Real},
            {"string", String},
            {"bool", Boolean},
			{"number", Number},
            {"void", Void},
            {"any", Any},
			{"pgs", PGS},
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
    type_to_annotation = invert_map(annotation_to_type);
}

Type *TypeRegistry::get_type_from_annotation(const std::string &name) {
    auto it = annotation_to_type.find(name);
    if (it != annotation_to_type.end()) {
        return it->second;
    }
    return Unknown;
}

std::string TypeRegistry::get_annotation_from_type(Type* ty) {
	if (!ty) return "";
    auto it = type_to_annotation.find(ty);
    if (it != type_to_annotation.end()) {
        return it->second;
    }

	const auto is_registered = [ty] {
		for (const auto& [_, type] : object_types) {
			if (type.get() == ty) return true;
		}
		for (const auto& [_, type] : composite_types) {
			if (type.get() == ty) return true;
		}
		for (const auto& [_, type] : function_types) {
			if (type.get() == ty) return true;
		}
		return false;
	};
	if (!is_registered()) return "";
    return ty->to_string();
}


Type *TypeRegistry::get_type_from_identifier(const char identifier) {
    auto it = identifier_to_type.find(identifier);
    if (it != identifier_to_type.end()) {
        return it->second;
    }
    return Unknown;
}

char TypeRegistry::get_identifier_from_type(Type *ty) {
    const auto it = type_to_identifier.find(ty);
    if (it != type_to_identifier.end()) {
        return it->second;
    }
    return ' ';
}

std::unique_ptr<NodeAST> TypeRegistry::get_neutral_element_from_type(const Type* ty) {
	if (ty == Integer) {
		return std::make_unique<NodeInt>(0, Token());
	} else if (ty == Real) {
		return std::make_unique<NodeReal>(0.0, Token());
	} else if (ty == String) {
		return std::make_unique<NodeString>("\"\"", Token());
	} else if (ty == Boolean) {
		return std::make_unique<NodeString>("false", Token());
    } else if (ty->get_type_kind() == TypeKind::Object) {
		return std::make_unique<NodeNil>(Token());
	} else if (ty->cast<CompositeType>()) {
		// recursive if composite type
		return std::make_unique<NodeInitializerList>(Token(), get_neutral_element_from_type(ty->get_element_type()));
	}
	return nullptr;
}

bool TypeRegistry::is_type_neutral_element(const Type *ty, const std::unique_ptr<NodeAST> &value) {
	if (ty == Integer) {
		const auto node_int = value->cast<NodeInt>();
		return node_int and node_int->value == 0;
	} else if (ty == Real) {
		const auto node_real = value->cast<NodeReal>();
		return node_real and node_real->value == 0.0;
	} else if (ty == String) {
		const auto node_string = value->cast<NodeString>();
		return node_string and node_string->value == "\"\"";
	} else if (ty == Boolean) {
		const auto node_bool = value->cast<NodeBoolean>();
		return node_bool and node_bool->value == false;
	} else if (ty->get_type_kind() == TypeKind::Object) {
		return value->cast<NodeNil>() != nullptr;
	} else if (ty->cast<CompositeType>()) {
		if (const auto init_list = value->cast<NodeInitializerList>()) {
			if (init_list->size() != 1) {
				return false;
			}
			return is_type_neutral_element(ty->get_element_type(), init_list->elements[0]);
		}
	}
	return false;
}

ObjectType *TypeRegistry::add_object_type(const std::string &name) {
    if(auto obj_ty = get_object_type(name)) {
        return obj_ty;
    }
	auto object_type = std::make_unique<ObjectType>(name);
	auto key = object_type->registry_key();
	object_types[key] = std::move(object_type);
	return object_types[key].get();
}

ObjectType *TypeRegistry::add_object_type(
		const std::string &name, const std::vector<Type*>& arguments) {
	if (auto obj_ty = get_object_type(name, arguments)) {
		return obj_ty;
	}
	auto object_type = std::make_unique<ObjectType>(name, arguments);
	auto key = object_type->registry_key();
	object_types[key] = std::move(object_type);
	return object_types[key].get();
}

ObjectType *TypeRegistry::get_object_type(const std::string &name) {
	const ObjectType object_type(name);
	auto it = object_types.find(object_type.registry_key());
    if (it != object_types.end()) {
        return it->second.get();
    }
    return nullptr;
}

ObjectType *TypeRegistry::get_object_type(
		const std::string &name, const std::vector<Type*>& arguments) {
	const ObjectType object_type(name, arguments);
	auto it = object_types.find(object_type.registry_key());
	if (it != object_types.end()) {
		return it->second.get();
	}
	return nullptr;
}

CompositeType *TypeRegistry::get_composite_type(CompoundKind comp_type, Type *element_type, int dimensions) {
	const CompositeType composite_type(comp_type, element_type, dimensions);
	auto hash_val = composite_type.registry_key();
    auto it = composite_types.find(hash_val);
    if (it != composite_types.end()) {
        return it->second.get();
    }
    return nullptr;
}

CompositeType *TypeRegistry::add_composite_type(CompoundKind comp_type, Type *element_type, int dimensions) {
    if(auto comp_ty = get_composite_type(comp_type, element_type, dimensions)) {
        return comp_ty;
    }
	auto composite_type = std::make_unique<CompositeType>(comp_type, element_type, dimensions);
	auto hash_val = composite_type->registry_key();
	composite_types[hash_val] = std::move(composite_type);
    return composite_types[hash_val].get();
}

FunctionType *TypeRegistry::get_function_type(std::vector<Type *> params, Type *return_type) {
	const FunctionType function_type(std::move(params), return_type);
	auto hash_val = function_type.registry_key();
	auto it = function_types.find(hash_val);
	if (it != function_types.end()) {
		return it->second.get();
	}
	return nullptr;
}

FunctionType *TypeRegistry::add_function_type(const std::vector<Type *>& params, Type *return_type) {
	if(auto func_ty = get_function_type(params, return_type)) {
		return func_ty;
	}
	auto func_type = std::make_unique<FunctionType>(params, return_type);
	auto hash_val = func_type->registry_key();
	function_types[hash_val] = std::move(func_type);
	return function_types[hash_val].get();
}
