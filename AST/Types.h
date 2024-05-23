//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include <utility>

#include "../Tokenizer/Tokens.h"


inline static std::string type_kind_names[] = {"Basic", "Composite", "Object"};
enum class TypeKind {Basic, Composite, Object};
inline static std::string kind_names[] = {"Integer", "Boolean", "String", "Real", "Unknown", "Object", "Any", "Void"};
enum class Kind {Integer, Boolean, String, Real, Unknown, Object, Any, Void};

/**
 * @class Type
 * @brief This class represents a type in the language.
 *
 * A Type can be of different kinds (Integer, Boolean, String, Real, Unknown, Object, Any, Void).
 * Each type has a kind and a type kind (Basic, Composite, Object).
 * The Type class is an abstract base class for different types of types.
 */
class Type {
public:
    explicit Type(Kind kind=Kind::Unknown) : m_kind(kind) {}
    virtual ~Type() = default;
    Type(const Type& other) = default;
    [[nodiscard]] virtual std::unique_ptr<Type> clone() const = 0;
    [[nodiscard]] virtual std::string to_string() const = 0;
    [[nodiscard]] virtual TypeKind get_type_kind() const = 0;
    [[nodiscard]] virtual Kind get_kind() const { return m_kind; }
    virtual bool is_compatible(const Type& other) const {
        return m_kind == other.get_kind() && get_type_kind() == other.get_type_kind();
    }
protected:
    Kind m_kind = Kind::Unknown;
};

class BasicType: public Type {
public:
    explicit BasicType(Kind kind=Kind::Unknown): Type(kind) {}
    BasicType(const BasicType& other) = default;
    [[nodiscard]] std::unique_ptr<Type> clone() const override {
        return std::make_unique<BasicType>(*this);
    }
    [[nodiscard]] std::string to_string() const override {
        return kind_names[(int)m_kind];
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Basic;
    }
};

class CompositeType : public Type {
public:
    enum Kind {Array, List};
    CompositeType(CompositeType::Kind compound_kind, const Type* element_type): Type(element_type->get_kind()), m_compound_kind(compound_kind), m_element_type(element_type) {}
    CompositeType(const CompositeType& other)
            : Type(other.get_kind()), m_compound_kind(other.m_compound_kind), m_element_type(other.m_element_type) {}

    [[nodiscard]] std::unique_ptr<Type> clone() const override {
        return std::make_unique<CompositeType>(m_compound_kind, m_element_type);
    }
    [[nodiscard]] std::string to_string() const override {
        return std::string(type_kind_names[(int)m_compound_kind]) + "[" + m_element_type->to_string() + "]";
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Composite;
    }
private:
    Kind m_compound_kind;
    const Type* m_element_type;
};

class ObjectType : public Type {
public:
    explicit ObjectType(std::string name): Type(Kind::Object), m_name(std::move(name)) {}
    ObjectType(const ObjectType& other) = default;
    [[nodiscard]] std::unique_ptr<Type> clone() const override {
        return std::make_unique<ObjectType>(*this);
    }
    [[nodiscard]] std::string to_string() const override {
        return m_name;
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Object;
    }
	[[nodiscard]] bool is_compatible(const Type& other) const override {
		return m_kind == other.get_kind() && get_type_kind() == other.get_type_kind() && m_name == other.to_string();
	}
private:
    std::string m_name;
};


