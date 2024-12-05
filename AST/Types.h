//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include <utility>
#include <memory>
#include <typeindex>
#include "../Tokenizer/Tokens.h"

inline static std::string type_kind_names[] = {"Basic", "Composite", "Object", "Function"};
enum class TypeKind {Basic, Composite, Object, Function};
inline static std::string kind_names[] = {"int", "bool", "string", "real", "unknown", "object", "any", "void", "number", "comparison", "pgs"};
enum class Kind {Integer, Boolean, String, Real, Unknown, Object, Any, Void, Number, Comparison, PGS};
inline static std::string compound_kind_names[] = {"Array", "List"};
enum class CompoundKind {Array, List};

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
    [[nodiscard]] virtual int get_dimensions() const {return 0;}
	[[nodiscard]] virtual Kind get_kind() const { return m_kind; }
    [[nodiscard]] virtual bool is_compatible(const Type* other) const {
        return m_kind == other->get_kind() && get_type_kind() == other->get_type_kind();
    }
	virtual bool is_assignable_from(const Type* other) const {
		return is_compatible(other);
	}
	virtual bool is_same_type(const Type* other) const {
		return get_type_kind() == other->get_type_kind();
	}
	// Template-Methode für den Cast
	template <typename TargetType>
	const TargetType* cast() const {
		// Überprüfen, ob der Typ des Objekts mit `TargetType` übereinstimmt
		if (std::type_index(typeid(*this)) == std::type_index(typeid(TargetType))) {
			return static_cast<const TargetType*>(this);
		}
		return nullptr;
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
	[[nodiscard]] Type* get_element_type() const override {
		return const_cast<BasicType*>(this);
	}
	bool is_assignable_from(const Type* other) const override {
		// string is also compatible with float or int or number
		bool string_number = m_kind == Kind::String && (other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
		if(string_number) return true;
		return is_compatible(other);
	}
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		// unknown is compatible with everything
		bool unknown = m_kind == Kind::Unknown || other->get_kind() == Kind::Unknown;
		if(unknown) return true;

		// any is compatible with any other type
		bool any = (m_kind == Kind::Any || other->get_kind() == Kind::Any); //and (other->get_type_kind() == TypeKind::Basic or other->get_type_kind() == TypeKind::Object);
		if(any) return true;

		bool voids = m_kind == Kind::Void && other->get_kind() == Kind::Void;
		if(voids) return true;

		bool type_kind = this->get_type_kind() == other->get_type_kind();
		if(!type_kind) return false;

		// number, integer, real are compatible with each other
		bool numbers = (m_kind == Kind::Number || m_kind == Kind::Integer || m_kind == Kind::Real) and
			(other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
		if(numbers) return true;

		// string is compatible with string
		bool strings = m_kind == Kind::String && other->get_kind() == Kind::String;
		if(strings) return true;

		// string is also compatible with float or int or number
		bool string_number = m_kind == Kind::String && (other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
		if(string_number) return true;

		// boolean is compatible with boolean
		bool booleans = m_kind == Kind::Boolean && other->get_kind() == Kind::Boolean;
		if(booleans) return true;

        // comparison is compatible with boolean and vice versa
        bool comparison = m_kind == Kind::Comparison && other->get_kind() == Kind::Boolean ||
                m_kind == Kind::Boolean && other->get_kind() == Kind::Comparison;
        if(comparison) return true;

        // comparison is compatible with int or real and vice versa
        bool compare_number = m_kind == Kind::Comparison && (other->get_kind() == Kind::Number || other->get_kind() == Kind::Integer || other->get_kind() == Kind::Real);
        compare_number |= other->get_kind() == Kind::Comparison && (m_kind == Kind::Number || m_kind == Kind::Integer || m_kind == Kind::Real);
        if(compare_number) return true;

		bool pgs = m_kind == Kind::PGS && other->get_kind() == Kind::PGS;
		if(pgs) return true;

		return false;
	}
};

class CompositeType : public Type {
public:
    CompositeType(CompoundKind compound_kind, Type* element_type, int dimensions=1)
        : Type(element_type->get_kind()), m_compound_kind(compound_kind),
        m_element_type(element_type), m_dimensions(dimensions) {}
    CompositeType(const CompositeType& other)
            : Type(other.get_kind()), m_compound_kind(other.m_compound_kind), m_element_type(other.m_element_type),
			m_dimensions(other.m_dimensions) {}

    [[nodiscard]] std::unique_ptr<Type> clone() const override {
        return std::make_unique<CompositeType>(m_compound_kind, m_element_type);
    }
    [[nodiscard]] std::string to_string() const override {
		std::string output = m_element_type->to_string();
		for(int i = 0; i < m_dimensions; i++) {
			output += "[]";
		}
		return output;
    };
    [[nodiscard]] TypeKind get_type_kind() const override {
        return TypeKind::Composite;
    }
    [[nodiscard]] int get_dimensions() const override {return m_dimensions;}
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		bool is_unknown = other->get_kind() == Kind::Unknown and other->get_type_kind() == TypeKind::Basic;
		bool is_any = other->get_kind() == Kind::Any and other->get_type_kind() == TypeKind::Basic;
		bool is_unknown_dim = m_dimensions == 0 or (other->get_type_kind() == TypeKind::Composite and other->get_dimensions() == 0);
		return is_unknown or is_any or is_unknown_dim or (get_type_kind() == other->get_type_kind() and m_dimensions == other->get_dimensions() and m_element_type->is_compatible(other->get_element_type()));
	}
	[[nodiscard]] Type* get_element_type() const override {return m_element_type;}
	[[nodiscard]] CompoundKind get_compound_type() const {return m_compound_kind;}
	bool is_same_type(const Type* other) const override {
		return get_type_kind() == other->get_type_kind() && m_compound_kind == static_cast<const CompositeType*>(other)->get_compound_type() && m_dimensions == other->get_dimensions();
	}
	void set_element_type(Type* element_type) {
		m_element_type = element_type;
	}
private:
	CompoundKind m_compound_kind;
	Type* m_element_type;
    int m_dimensions;
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
	[[nodiscard]] Type* get_element_type() const override {return (Type *) this;}
	[[nodiscard]] bool is_compatible(const Type* other) const override {
		bool is_object_type = get_type_kind() == other->get_type_kind() && (m_name == other->to_string() or other->to_string() == "nil" or m_name == "nil");
		return is_object_type or other->get_kind() == Kind::Unknown or other->get_kind() == Kind::Any;
	}
	bool is_same_type(const Type* other) const override {
		return get_type_kind() == other->get_type_kind() or ((other->get_kind() == Kind::Unknown or other->get_kind() == Kind::Any) and other->get_type_kind() == TypeKind::Basic);
	}
private:
    std::string m_name;
};

class FunctionType : public Type {
public:
	explicit FunctionType(std::vector<Type*> params, Type* return_type)
		: Type(return_type->get_kind()), m_params(std::move(params)), m_return_type(return_type) {}
	FunctionType(const FunctionType& other) = default;
	[[nodiscard]] std::unique_ptr<Type> clone() const override {
		return std::make_unique<FunctionType>(*this);
	}
	[[nodiscard]] std::string to_string() const override {
		std::string result = "(";
		for (size_t i = 0; i < m_params.size(); ++i) {
			result += m_params[i]->to_string();
			if (i < m_params.size() - 1) {
				result += ", ";
			}
		}
		result += "): " + m_return_type->to_string();
		return result;
	}

	[[nodiscard]] TypeKind get_type_kind() const override {
		return TypeKind::Function;
	}
	[[nodiscard]] Type* get_element_type() const override {return (Type *) this;}
	[[nodiscard]] const std::vector<Type*>& get_params() const { return m_params; }
	[[nodiscard]] Type* get_return_type() const { return m_return_type; }

	[[nodiscard]] bool is_compatible(const Type* other) const override {
		// Zunächst prüfen, ob der andere Typ ebenfalls ein Funktionstyp ist
		if (other->get_type_kind() != TypeKind::Function and other->get_kind() != Kind::Unknown) {
			return false;
		}

		// Cast auf den Funktionstyp
		const auto* other_function = dynamic_cast<const FunctionType*>(other);

		// Parameteranzahl prüfen
		if (m_params.size() != other_function->get_params().size()) {
			return false;
		}

		// Jeder Parameter muss kompatibel sein
		for (size_t i = 0; i < m_params.size(); ++i) {
			if (!m_params[i]->is_compatible(other_function->get_params()[i])) {
				return false;
			}
		}

		// Der Rückgabewert muss ebenfalls kompatibel sein
		if (!m_return_type->is_compatible(other_function->get_return_type())) {
			return false;
		}

		// Wenn alle Überprüfungen erfolgreich waren, sind die Funktionstypen kompatibel
		return true;
	}

	std::vector<Type*> m_params;
	Type* m_return_type;
private:
};


