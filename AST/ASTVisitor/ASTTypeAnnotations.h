//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include "ASTVisitor.h"

/**
 * @class ASTTypeAnnotations
 * @brief Applies type annotations to AST nodes.
 *
 * This class traverses the AST and applies pre-set type annotations to DataStructure Nodes
 * and checks if the type annotations are compatible with the syntax. Replaces the node types when
 * applicable.
 */
class ASTTypeAnnotations : public ASTVisitor {
public:
	explicit ASTTypeAnnotations(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}

		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.visited = true;

		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);

		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if (node.variable->ty == TypeRegistry::Any or node.variable->ty == TypeRegistry::Number) {
			auto error = get_apply_type_annotations_error(node.variable);
			error.m_message += " Union types like <Any> or <Number> can only be used in function parameters.";
			error.exit();
		}
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		check_annotation_with_expected(node, TypeRegistry::ArrayOfUnknown);
		check_for_correct_object_type_annotation(node);
		return apply_type_annotations(node.get_shared());
	}
	NodeAST* visit(NodeVariable& node) override {
		check_annotation_with_expected(node, TypeRegistry::Unknown);
		check_for_correct_object_type_annotation(node);
		return apply_type_annotations(node.get_shared());
	}
	NodeAST* visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		check_annotation_with_expected(node, std::make_unique<CompositeType>(CompoundKind::Array, TypeRegistry::Unknown, node.dimensions).get());
		check_for_correct_object_type_annotation(node);
		return apply_type_annotations(node.get_shared());
	}
	NodeAST* visit(NodeFunctionHeader& node) override {
		for(auto &param : node.params) param->variable->accept(*this);
//		check_annotation_with_expected(node, TypeRegistry::Unknown);
//		return apply_type_annotations(node.get_shared());
		return &node;
	}
	NodeAST* visit(NodePointer& node) override {
		check_annotation_with_expected(node, TypeRegistry::Unknown);
		check_for_correct_object_type_annotation(node);
		return apply_type_annotations(node.get_shared());
	}
	NodeAST* visit(NodeList& node) override {
		for(auto & b : node.body) b->accept(*this);
		check_annotation_with_expected(node, std::make_unique<CompositeType>(CompoundKind::List, TypeRegistry::Unknown, 1).get());
		check_for_correct_object_type_annotation(node);
		return apply_type_annotations(node.get_shared());
	}


private:
	DefinitionProvider* m_def_provider = nullptr;

	static CompileError get_apply_type_annotations_error(const std::shared_ptr<NodeDataStructure> &node) {
		auto error = CompileError(ErrorType::InternalError, "", "", node->tok);
		error.m_message = "Type Annotation cannot be applied to node: "+node->name+".";
		error.m_got = node->ty->to_string();
		return error;
	}

	/// check if data structure annotations fit with the detected node type if not in func arguments
	static Type* check_annotation_with_expected(const NodeDataStructure& node, const Type* expected) {
		// skip function parameters
		if(node.is_function_param()) return node.ty;
		if(node.ty == TypeRegistry::Unknown) return node.ty;
		if(!node.ty->is_compatible(expected)) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Type Annotation of "+node.name+" does not match expected type kind.";
			error.m_expected =  "<"+expected->get_type_kind_name()+"> Type";
			error.m_got = "<"+node.ty->get_type_kind_name()+"> Type";
			error.exit();
		}
		return nullptr;
	}

	void check_for_correct_object_type_annotation(const NodeDataStructure& node) const {
		const auto element_type = node.ty->get_element_type();
		if (element_type->get_type_kind() == TypeKind::Object) {
			if (!NodeReference::get_object_ptr(m_program, element_type->to_string())) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Found undefined Type. Type Annotation of "+node.name+" does not match any existing <Object> type.";
				error.m_expected = "valid <Object> Type";
				error.m_got = "<"+node.ty->to_string()+">";
				error.exit();

			}
		}
	}

	/// apply type annotations given before parse time and replace node types accordingly
	/// returns the new datastructure pointer if replaced, or the old one if not
	static NodeDataStructure* apply_type_annotations(const std::shared_ptr<NodeDataStructure>& node) {
		if(node->ty == TypeRegistry::Unknown) return node.get();

		NodeAST* new_data_struct = nullptr;
		if(node->ty->get_type_kind() == TypeKind::Composite) {
			auto comp_type = static_cast<CompositeType*>(node->ty);
			// if var is annotated as array, replace with array
			if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::Array and comp_type->get_dimensions() == 1) {
				auto node_array = node->to_array(nullptr);
				if(!node_array) get_apply_type_annotations_error(node).exit();
//				node_array->is_local = node->is_local;
				node_array->ty = comp_type;
				new_data_struct = node->replace_with(std::move(node_array));
			} else if(comp_type->get_compound_type() == CompoundKind::Array and node->get_node_type() != NodeType::NDArray and comp_type->get_dimensions() > 1) {
				auto node_ndarray = node->to_ndarray();
				if(!node_ndarray) get_apply_type_annotations_error(node).exit();
				node_ndarray->dimensions = comp_type->get_dimensions();
//				node_ndarray->is_local = node->is_local;
				node_ndarray->ty = comp_type;
				new_data_struct = node->replace_with(std::move(node_ndarray));
			}
		} else if (node->ty->get_type_kind() == TypeKind::Basic) {
			// if var is annotated as variable but recognized as array by parser -> throw error
			if(node->get_node_type() == NodeType::Array or node->get_node_type() == NodeType::NDArray) {
				auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
				syntax_error.m_message += " Variable was annotated as <Variable> but recognized as <Array>: "+node->name+".";
				syntax_error.m_expected = "<Variable> Syntax";
				syntax_error.m_got = "<Array> Syntax";
				syntax_error.exit();
				return nullptr;
			}
			// var was annotated as object
		} else if (node->ty->get_type_kind() == TypeKind::Object and node->get_node_type() != NodeType::Pointer) {
			// throw error when node was not recognized as variable by parser
			if(node->get_node_type() != NodeType::Variable) {
				auto syntax_error = CompileError(ErrorType::SyntaxError, "Syntax and Type Annotation are not compatible.", "", node->tok);
				syntax_error.m_message += " Variable was annotated as <Object> but recognized as <Array>: "+node->name+".";
				syntax_error.m_expected = "<Object> Syntax";
				syntax_error.m_got = "<Array> Syntax";
				syntax_error.exit();
				return nullptr;
			} else {
				auto node_pointer = node->to_pointer();
				if(!node_pointer) get_apply_type_annotations_error(node).exit();
//				node_pointer->is_local = node->is_local;
				node_pointer->ty = node->ty;
				new_data_struct = node->replace_with(std::move(node_pointer));
			}
		} else if(node->ty->get_type_kind() == TypeKind::Function and node->get_node_type() != NodeType::FunctionHeader and node->get_node_type() == NodeType::Variable) {
			auto node_function = std::make_unique<NodeFunctionHeader>(node->name, node->tok);
			if(!node_function) get_apply_type_annotations_error(node).exit();
			node_function->is_local = node->is_local;
			node_function->ty = node->ty;
			new_data_struct = node->replace_with(std::move(node_function));
		}
		if(new_data_struct) {
			return static_cast<NodeDataStructure*>(new_data_struct);
		}
		return node.get();
	}

};