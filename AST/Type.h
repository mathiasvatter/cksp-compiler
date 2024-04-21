//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include "../Tokenizer/Tokens.h"


inline static std::string TypeKindNames[] = {"Basic", "Composite", "Object"};
enum class TypeKind {Basic, Composite, Object};
inline static std::string KindNames[] = {"Integer", "Boolean", "String", "Real", "Unknown", "Object", "Any", "Void"};
enum class Kind {Integer, Boolean, String, Real, Unknown, Object, Any, Void};


class Type {
public:
    explicit Type(Kind kind) : m_kind(kind) {}
    ~Type() = default;
    virtual std::string to_string() const = 0;
    [[nodiscard]] virtual TypeKind get_type_kind() const = 0;
    virtual Kind get_kind() const { return m_kind; }
    virtual bool is_compatible(const Type& other) const {
        return m_kind == other.get_kind() && get_type_kind() == other.get_type_kind();
    }
protected:
    Kind m_kind = Kind::Unknown;
};

class BasicType: public Type {
public:
    explicit BasicType(Kind kind): Type(kind) {}
    [[nodiscard]] std::string to_string() const override {
        return KindNames[(int)m_kind];
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Basic;
    }
};

class CompositeType : public Type {
public:
    enum Kind {Array, List};
    CompositeType(CompositeType::Kind compound_kind, Type* element_type): Type(element_type->get_kind()), m_compound_kind(compound_kind), m_element_type(element_type) {}
    [[nodiscard]] std::string to_string() const override {
        return std::string(TypeKindNames[(int)m_compound_kind]) + "[" + m_element_type->to_string() + "]";
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Composite;
    }
private:
    Kind m_compound_kind;
    std::unique_ptr<Type> m_element_type;
};

class ObjectType : public Type {
public:
    explicit ObjectType(const std::string& name): Type(Kind::Object), m_name(name) {}
    [[nodiscard]] std::string to_string() const override {
        return m_name;
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Object;
    }
private:
    std::string m_name;
};


