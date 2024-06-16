//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class TypeInference : public ASTVisitor {
public:
	explicit TypeInference(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {};

	void visit(NodeProgram& node) override;

	/// Type casting with Base Types
	void visit(NodeInt& node) override;
	void visit(NodeString& node) override;
	void visit(NodeReal& node) override;
	void visit(NodeNil& node) override;

    void visit(NodeCallback& node) override;

	void visit(NodeSingleDeclaration& node) override;
	void visit(NodeSingleAssignment& node) override;
	void visit(NodeUIControl& node) override;

    /// check if every member has same type only if in assign or declare statement
	void visit(NodeParamList& node) override;
	void visit(NodeVariableRef& node) override;
	void visit(NodeVariable& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeArrayRef& node) override;

    void visit(NodeNDArray& node) override;
    void visit(NodeNDArrayRef& node) override;

	void visit(NodePointer& node) override;
	void visit(NodePointerRef& node) override;

    void visit(NodeList& node) override;
    void visit(NodeListRef& node) override;

	void visit(NodeBinaryExpr& node) override;
	void visit(NodeUnaryExpr& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;

	void visit(NodeConstStatement& node) override;

    /// iterates through all references and declarations and tries to match the types
    /// with cast set to true -> will cast types of data structures if no type could be infered
    static void cast_data_structure_types(DefinitionProvider* def_provider, bool cast= false);

private:
	DefinitionProvider* m_def_provider;

    /// error if composite type was not added to the type registry
    static inline CompileError throw_composite_error(NodeReference* node) {
        auto error = CompileError(ErrorType::TypeError,"", "", node->tok);
        error.m_message = "Composite Type is referenced but does not exist: "+node->name;
        return error;
    }

    static inline CompileError throw_type_error(NodeAST* node1, NodeAST* node2) {
        auto error = CompileError(ErrorType::TypeError,"", "", node1->tok);
        error.m_message = "Type mismatch: " + node1->ty->to_string() + " and " + node2->ty->to_string()+". ";
        return error;
    }

    /// check types of initializations and try to infer overall element type
    static Type* infer_initialization_types(std::vector<Type*> &type_list, NodeAST* node) {
        std::set<Type*> types(type_list.begin(), type_list.end());
        auto error = CompileError(ErrorType::TypeError, "", "", node->tok);
        if(types.empty()) CompileError(ErrorType::InternalError, "Found no types in list", "", node->tok).exit();
        // all types in param list are the same -> set new param list node type
        if(types.size() == 1) {
            auto ty = *types.begin();
            if(ty->get_type_kind() == TypeKind::Composite) {
                error.m_message = "Composite types are not allowed in an initialization.";
                error.exit();
            }
            return ty;
        // two types in param list -> only allowed if types are int|real and string
        } else if (types.size() == 2) {
            auto it = types.find(TypeRegistry::String);
            if(it != types.end()) {
                if(types.find(TypeRegistry::Integer) != types.end() or types.find(TypeRegistry::Real) != types.end()) {
                    return TypeRegistry::String;
                } else {
                    error.m_message = "Only <String> and <Integer> or <Real> types are allowed in an initialization.";
                    error.exit();
                }
            }
        }
		error.m_message = "Unable to infer type from initialization.";
		error.exit();
        return nullptr;
    }

	/// tries to match the type of node 1 to the type of node2 after checking type compatibility
	static inline Type* match_type(NodeAST* node1, NodeAST* node2) {
		if(!node1->ty->is_compatible(node2->ty)) {
            throw_type_error(node1, node2).exit();
		}

		// specialize types:
        node1->set_element_type(specialize_type(node1->ty, node2->ty));
        return node1->ty;
	}

	/// tries to match the types of l_value and r_value after checking type compatibility, skipping
	/// compatibility check of r_value to l_value because of string and integer
	static inline Type* match_assignment_types(NodeAST* l_value, NodeAST* r_value) {
		auto l_value_ty = match_type(l_value, r_value);
		if(l_value_ty == TypeRegistry::String) return l_value_ty;
		// specialize types:
		r_value->set_element_type(specialize_type(r_value->ty, l_value->ty));
		return r_value->ty;
	}

    /// tries to match the types of reference and declaration by also checking for element typ
    /// in case declaration is a composite type
    static inline Type* match_reference_declaration(NodeReference* node) {
		// before lowering, the declaration is not necessarily set, check before:
		if(!node->declaration) return node->ty;

        Type* declaration_type = node->declaration->ty->get_element_type();
        Type* reference_type = node->ty->get_element_type();
        if(!reference_type->is_compatible(declaration_type)) {
            throw_type_error(node, node->declaration).exit();
        }

        // match type from declaration to reference
		node->set_element_type(specialize_type(reference_type, declaration_type));

        declaration_type = node->declaration->ty->get_element_type();
        reference_type = node->ty->get_element_type();
        if(!declaration_type->is_compatible(reference_type)) {
            throw_type_error(node->declaration, node).exit();
        }
        // match type from reference to declaration
        node->declaration->set_element_type(specialize_type(declaration_type, reference_type));
        return node->ty;
    }

    /// tries to find the most specialized type for type1 by looking at type2
    static inline Type* specialize_type(Type* type1, Type* type2) {
        // get element types if composite type (element typ not nullptr):
        auto node_1 = type1->get_element_type();
        auto node_2 = type2->get_element_type();

        // specialize types:
		// comparison is never the most specialized type
		if(node_2 == TypeRegistry::Comparison) return node_1;
		if(node_1 == TypeRegistry::Comparison) return node_2;

        // if node 2 is unknown, return type of node1
        if(node_2 == TypeRegistry::Unknown) return node_1;
        // if node 1 is unknown, return type of node2
        if(node_1 == TypeRegistry::Unknown) return node_2;

		// if node 2 is unknown, return type of node1
		if(node_2 == TypeRegistry::Nil) return node_1;
		// if node 1 is unknown, return type of node2
		if(node_1 == TypeRegistry::Nil) return node_2;

        // if node 1 is any, node2 has to be more specialized, return node_2
        if(node_1 == TypeRegistry::Any) return node_2;
        // if node 2 is any, node1 has to be more specialized, return node_1
        if(node_2 == TypeRegistry::Any) return node_1;

        // if node 1 is number, node 2 has to be number, int or real, return node2
        if(node_1 == TypeRegistry::Number) return node_2;
        // if node 2 is number, it is the other way around
        if(node_2 == TypeRegistry::Number) return node_1;

        // in all other cases, node_1 is already specialized
        return node_1;
    }
};