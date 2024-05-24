//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include <utility>

#include "../Tokenizer/Tokens.h"


inline static std::string type_kind_names[] = {"Basic", "Composite", "Object"};
enum class TypeKind {Basic, Composite, Object};
inline static std::string kind_names[] = {"Integer", "Boolean", "String", "Real", "Unknown", "Object", "Any", "Void", "Number"};
enum class Kind {Integer, Boolean, String, Real, Unknown, Object, Any, Void, Number};

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
	[[nodiscard]] virtual std::string get_type_kind_name() const {return type_kind_names[(int)get_type_kind()];}
	[[nodiscard]] virtual Type* get_element_type() const {return nullptr;}
    [[nodiscard]] virtual Kind get_kind() const { return m_kind; }
    [[nodiscard]] virtual bool is_compatible(const Type* other) const {
        return m_kind == other->get_kind() && get_type_kind() == other->get_type_kind();
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
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		// unknown is compatible with everything
		bool unknown = m_kind == Kind::Unknown || other->get_kind() == Kind::Unknown;
		if(unknown) return true;

		// number, integer, real are compatible with each other
		bool numbers = (m_kind == Kind::Number || m_kind == Kind::Integer || m_kind == Kind::Real) and
			(other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
		if(numbers) return true;

		// any is compatible with any other type
		bool any = m_kind == Kind::Any || other->get_kind() == Kind::Any;
		if(any) return true;

		// string is compatible with string
		bool strings = m_kind == Kind::String && other->get_kind() == Kind::String;
		if(strings) return true;

		// string is also compatible with float or int or number
		bool string_number = m_kind == Kind::String && (other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
		if(string_number) return true;

		// boolean is compatible with boolean
		bool booleans = m_kind == Kind::Boolean && other->get_kind() == Kind::Boolean;
		if(booleans) return true;
		return false;
	}
};

class CompositeType : public Type {
public:
    enum Kind {Array, List};
	inline static std::string kind_names[] = {"Array", "List"};
    CompositeType(CompositeType::Kind compound_kind, Type* element_type): Type(element_type->get_kind()), m_compound_kind(compound_kind), m_element_type(element_type) {}
    CompositeType(const CompositeType& other)
            : Type(other.get_kind()), m_compound_kind(other.m_compound_kind), m_element_type(other.m_element_type) {}

    [[nodiscard]] std::unique_ptr<Type> clone() const override {
        return std::make_unique<CompositeType>(m_compound_kind, m_element_type);
    }
    [[nodiscard]] std::string to_string() const override {
        return std::string(kind_names[(int)m_compound_kind]) + "[" + m_element_type->to_string() + "]";
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Composite;
    }
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		return get_type_kind() == other->get_type_kind() and m_element_type->is_compatible(other);
	}
	[[nodiscard]] Type* get_element_type() const override {return m_element_type;}
private:
    Kind m_compound_kind;
	Type* m_element_type;
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
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		return get_type_kind() == other->get_type_kind() && m_name == other->to_string();
	}
private:
    std::string m_name;
};


