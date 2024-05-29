//
// Created by Mathias Vatter on 27.05.24.
//
#include "TypeRegistry.h"



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
    auto it = type_to_neutral_element.find(ty);
    if (it != type_to_neutral_element.end()) {
        return it->second->clone();
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

Type *TypeRegistry::set_element_type(Type *ty, Type *element_type) {
    if(ty->get_type_kind() == TypeKind::Composite and element_type->get_type_kind() == TypeKind::Basic) {
        ty = get_composite_type(static_cast<CompositeType*>(ty)->get_compound_type(), element_type, ty->get_dimensions());
        return ty;
    }
    if(ty->get_type_kind() == TypeKind::Basic and element_type->get_type_kind() == TypeKind::Basic) {
        ty = element_type;
        return ty;
    }
    return nullptr;
}

Type *TypeRegistry::infer_type(NodeDataStructure *node) {
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