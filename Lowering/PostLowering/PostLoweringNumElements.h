//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of num_elements
 * has to happen after function inlining
 * if num_elements member in array declaration is given -> create declaration for num_elements array
 * 	declare ndarray::num_elements[3] := (3*3, 3, 3)
 * if no dimension is given -> was array -> call to num_elements
 */
class PostLoweringNumElements : public ASTLowering {
private:
	static NodeSingleDeclaration* get_next_declaration(NodeDataStructure& data) {
		if(auto decl = data.parent->cast<NodeSingleDeclaration>()) {
			return decl;
		} else if(auto ui_control = data.parent->cast<NodeUIControl>()) {
			if(auto decl1 = ui_control->parent->cast<NodeSingleDeclaration>()) {
				return decl1;
			}
		}
		return nullptr;
	}
public:
	explicit PostLoweringNumElements(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeNumElements &node) override {
		if(node.array->ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "<num_elements> can only be used with <Composite> types like <Arrays> or <NDArrays>.";
			error.exit();
		}

		auto decl = node.array->get_declaration();
		if(!decl) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Declaration of array in <num_elements> node was not set.";
			error.exit();
		}

		// check if num_elements member is set ->
		// if yes -> create declaration for num_elements array
		// if not -> call to num_elements
		if(auto node_array = decl->cast<NodeArray>()) {
			// in case nd array was written in raw array form -> declaration has num_elements but no dimension
			if(node_array->num_elements and node.dimension) {
				auto node_block = std::make_unique<NodeBlock>(node_array->tok);
				auto node_declaration = get_next_declaration(*node_array);
				if(!node_declaration) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node_array->tok);
					error.m_message = "<NodeArray> is not in a declaration.";
					error.exit();
				}
//				// if once declared, make empty
//				if(!node_array->num_elements->empty()) {
//					auto node_num_elements = std::make_shared<NodeArray>(
//						std::optional<Token>(),
//						node_array->name + OBJ_DELIMITER+"num_elements",
//						TypeRegistry::ArrayOfInt,
//						std::make_unique<NodeInt>(node_array->num_elements->size(), node.tok),
//						node.tok
//					);
//					auto node_num_elements_decl = std::make_unique<NodeSingleDeclaration>(
//						node_num_elements,
//						node_array->num_elements->to_initializer_list(),
//						node.tok
//					);
//					node_num_elements_decl->variable->data_type = DataType::Const;
//					node_array->num_elements = std::make_unique<NodeParamList>(node_array->tok);
//					node_block->add_as_stmt(std::move(node_num_elements_decl));
//
//					auto node_array_decl = std::make_unique<NodeSingleDeclaration>(
//						std::move(node_declaration->variable),
//						std::move(node_declaration->value),
//						node_declaration->tok
//						);
//					node_block->add_as_stmt(std::move(node_array_decl));
//					node_declaration->replace_with(std::move(node_block));
//				}

				auto num_elements_call = std::make_unique<NodeArrayRef>(
					node_array->name + OBJ_DELIMITER+"num_elements",
					std::move(node.dimension),
					node.tok
				);
//				num_elements_call->cast<NodeArrayRef>()->set_index(std::move(node.dimension));
				num_elements_call->ty = TypeRegistry::Integer;
				return node.replace_with(std::move(num_elements_call));

			} else {
				// use builtin num_elements function
				return node.replace_with(DefinitionProvider::num_elements(std::move(node.array)));
			}
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "<reference> node in <num_elements> has somehow a declaration that is not an <Array>.";
			error.exit();
		}
		return &node;
	}

	inline static std::unique_ptr<NodeAST> inline_clip_function(std::unique_ptr<NodeAST> x, std::unique_ptr<NodeAST> b) {
		// b-x
		auto b_minus_x = std::make_unique<NodeBinaryExpr>(
			token::SUB,
			b->clone(),
			x->clone(),
			Token()
		);
		// sh_right(b-x, 31)
		auto sh_right_b_minus_x = DefinitionProvider::sh_right(std::move(b_minus_x), std::make_unique<NodeInt>(31, Token()));

		// (x-b) .and. sh_right((b-x), 31)
		auto and_expr = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeBinaryExpr>(
				token::SUB,
				x->clone(),
				b->clone(),
				Token()
			),
			std::move(sh_right_b_minus_x),
			Token()
		);

		// -x .and. sh_right(x, 31)
		auto sh_right_x = DefinitionProvider::sh_right(x->clone(), std::make_unique<NodeInt>(31, Token()));
		auto and_expr2 = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeUnaryExpr>(
				token::SUB,
				x->clone(),
				Token()
			),
			std::move(sh_right_x),
			Token()
		);

		// -x .and. sh_right(x, 31) + (x-b) .and. sh_right((b-x), 31)
		auto add_expr = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			std::move(and_expr2),
			std::move(and_expr),
			Token()
		);

		// x + (-x .and. sh_right(x, 31)) - ((x - b) .and. sh_right((b - x), 31))
		return std::make_unique<NodeBinaryExpr>(
			token::ADD,
			x->clone(),
			std::move(add_expr),
			Token()
		);
	}


};