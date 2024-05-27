//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class TypeCasting : public ASTVisitor {
public:
	explicit TypeCasting(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {};

	void visit(NodeProgram& node) override;

	/// Type casting with Base Types
	void visit(NodeInt& node) override;
	void visit(NodeString& node) override;
	void visit(NodeReal& node) override;

	void visit(NodeSingleDeclareStatement& node) override;
	void visit(NodeSingleAssignStatement& node) override;
	void visit(NodeUIControl& node) override;

    /// check if every member has same type only if in assign or declare statement
	void visit(NodeParamList& node) override;
	void visit(NodeVariableRef& node) override;
	void visit(NodeVariable& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeArrayRef& node) override;

    void visit(NodeNDArray& node) override;
    void visit(NodeNDArrayRef& node) override;

	void visit(NodeBinaryExpr& node) override;
	void visit(NodeUnaryExpr& node) override;
	void visit(NodeFunctionCall& node) override;
//	void visit(NodeBody& node) override;

private:
	DefinitionProvider* m_def_provider;
    std::vector<NodeReference*> m_references;

    /// error if composite type was not added to the type registry
    static inline CompileError throw_composite_error(NodeReference* node) {
        auto error = CompileError(ErrorType::TypeError,"", "", node->tok);
        error.m_message = "Composite Type is referenced but does not exist: "+node->name;
        return error;
    }

    static inline CompileError throw_type_error(NodeAST* node1, NodeAST* node2) {
        auto error = CompileError(ErrorType::TypeError,"", "", node1->tok);
        error.m_message = "Type mismatch: " + node1->ty->to_string() + " and " + node2->ty->to_string();
        return error;
    }
	/// tries to match the type of node 1 to the type of node2 after checking type compatibility
	static inline Type* match_type(NodeAST* node1, NodeAST* node2) {
//		auto error = CompileError(ErrorType::TypeError,"", "", node1->tok);
		if(!node1->ty->is_compatible(node2->ty)) {
            throw_type_error(node1, node2).exit();
		}

		// specialize types:
        auto specialized_type = specialize_type(node1->ty, node2->ty);
        node1->ty = specialized_type;
        return specialized_type;
	}

    /// tries to match the types of reference and declaration by also checking for element typ
    /// in case declaration is a composite type
    static inline Type* match_reference_declaration(NodeReference* node) {
        auto declaration_type = node->declaration->ty->get_element_type() ? node->declaration->ty->get_element_type() : node->declaration->ty;
        auto reference_type = node->ty->get_element_type() ? node->ty->get_element_type() : node->ty;
        if(!reference_type->is_compatible(declaration_type)) {
            throw_type_error(node, node->declaration).exit();
        }
        // match type from declaration to reference
        reference_type = specialize_type(reference_type, declaration_type);

        if(!declaration_type->is_compatible(reference_type)) {
            throw_type_error(node->declaration, node).exit();
        }
        // match type from reference to declaration
        declaration_type = specialize_type(declaration_type, reference_type);
        return node->ty;
    }

    /// tries to find the most specialized type for type1 by looking at type2
    static inline Type* specialize_type(Type* type1, Type* type2) {
        // get element types if composite type (element typ not nullptr):
        auto node_1 = type1->get_element_type() ? type1->get_element_type() : type1;
        auto node_2 = type2->get_element_type() ? type2->get_element_type() : type2;

        // specialize types:
        // if node 2 is unknown, return type of node1
        if(node_2 == TypeRegistry::Unknown) return type1;
        // if node 1 is unknown, return type of node2
        if(node_1 == TypeRegistry::Unknown) return type2;

        // if node 1 is any, node2 has to be more specialized, return node_2
        if(node_1 == TypeRegistry::Any) return type2;
        // if node 2 is any, node1 has to be more specialized, return node_1
        if(node_2 == TypeRegistry::Any) return type1;

        // if node 1 is number, node 2 has to be number, int or real, return node2
        if(node_1 == TypeRegistry::Number) return type2;
        // if node 2 is number, it is the other way around
        if(node_2 == TypeRegistry::Number) return type1;

        // in all other cases, node_1 is already specialized
        return type1;
    }
};