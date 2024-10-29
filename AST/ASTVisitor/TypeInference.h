//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class TypeInference : public ASTVisitor {
public:
	explicit TypeInference(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {};

	NodeAST * visit(NodeProgram& node) override;

	/// Type casting with Base Types
	NodeAST * visit(NodeInt& node) override;
	NodeAST * visit(NodeString& node) override;
	NodeAST * visit(NodeReal& node) override;
	NodeAST * visit(NodeNil& node) override;

    NodeAST * visit(NodeCallback& node) override;

	NodeAST * visit(NodeSingleDeclaration& node) override;
	NodeAST * visit(NodeSingleAssignment& node) override;
	NodeAST * visit(NodeUIControl& node) override;
	NodeAST * visit(NodeGetControl& node) override;

	NodeAST * visit(NodeParamList& node) override;
    /// check if every member has same type
    NodeAST * visit(NodeInitializerList& node) override;

	NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodeVariable& node) override;
	NodeAST * visit(NodeArray& node) override;
	NodeAST * visit(NodeArrayRef& node) override;

    NodeAST * visit(NodeNDArray& node) override;
    NodeAST * visit(NodeNDArrayRef& node) override;

	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;

    NodeAST * visit(NodeList& node) override;
    NodeAST * visit(NodeListRef& node) override;

	NodeAST * visit(NodeFunctionVarRef& node) override;

	NodeAST * visit(NodeBinaryExpr& node) override;
	NodeAST * visit(NodeUnaryExpr& node) override;
    NodeAST * visit(NodeFunctionCall& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;
    NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeReturn& node) override;

	NodeAST * visit(NodeAccessChain& node) override;

	NodeAST * visit(NodeConst& node) override;
	NodeAST * visit(NodeStruct& node) override;
	NodeAST * visit(NodeNumElements& node) override;

    /// iterates through all references and declarations and tries to match the types
    /// with cast set to true -> will cast types of data structures if no type could be infered
    static void cast_data_structure_types(DefinitionProvider* def_provider, bool cast= false);

private:
	DefinitionProvider* m_def_provider;

public:
    /// error if composite type was not added to the type registry
    static inline CompileError throw_composite_error(NodeReference* node) {
        auto error = CompileError(ErrorType::TypeError,"", "", node->tok);
        error.m_message = "Composite Type is referenced but does not exist: "+node->name;
        return error;
    }

    static inline CompileError throw_type_error(NodeAST* node1, NodeAST* node2, const std::string& message="") {
        auto error = CompileError(ErrorType::TypeError,"", "", node1->tok);
        error.m_message = "Type mismatch: " + node1->ty->to_string() + " and " + node2->ty->to_string()+". ";
		error.m_message += message;
        return error;
    }

	static Type* update_function_type(NodeFunctionHeader* header, Type* return_type) {
		auto function_type = header->ty;
		auto function = static_cast<FunctionType*>(function_type);
		auto param_types = function->m_params;
		for(int i=0; i<header->args->size(); i++) {
			auto &old_param_type = param_types[i];
			auto &new_param = header->args->param(i);
			auto throwaway_old_node = new_param->clone();
			throwaway_old_node->ty = old_param_type;
			param_types[i] = match_type(throwaway_old_node.get(), new_param.get());
		}
		auto new_return_var = std::make_unique<NodeVariableRef>("return", header->tok);
		new_return_var->ty = return_type;
		auto old_return_var = std::make_unique<NodeVariableRef>("return", header->tok);
		old_return_var->ty = function->get_return_type();
		match_type(old_return_var.get(), new_return_var.get());
		function->m_return_type = std::move(old_return_var->ty);
		header->ty = function;
		return function;
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
	static inline Type* match_type(NodeAST* node1, NodeAST* node2, const std::string& message="") {
		if(!node1->ty->is_compatible(node2->ty)) {
            throw_type_error(node1, node2, message).exit();
		}
		// if one of them is composite type -> the other will be too
		if(node2->ty->get_type_kind() == TypeKind::Composite and node1->ty == TypeRegistry::Unknown) {
			// stash elem_typ temporarily
			auto elem_type = node1->ty->get_element_type();
			node1->ty = node2->ty;
			node1->set_element_type(elem_type);
		} else if(node1->ty->get_type_kind() == TypeKind::Composite and node2->ty == TypeRegistry::Unknown) {
			// stash elem_typ temporarily
			auto elem_type = node2->ty->get_element_type();
			node2->ty = node1->ty;
			node2->set_element_type(elem_type);
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

		// do not alter declaration type if it already has a type
		if(node->declaration->ty->get_element_type() != TypeRegistry::Unknown) return node->ty;

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