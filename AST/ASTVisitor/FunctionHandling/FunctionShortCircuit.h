//
// Created by Mathias Vatter on 15.07.25.
//

#pragma once

#include "../ASTVisitor.h"

class FunctionShortCircuit final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	std::unique_ptr<NodeIf> m_short_circuit_if = nullptr;
	NodeDataStructure* m_short_circuit_var = nullptr;
	NodeBlock* m_short_circuit_block = nullptr;
public:
	explicit FunctionShortCircuit(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* do_short_circuit(NodeIf& node) {
		auto new_outer_block = std::make_unique<NodeBlock>(node.tok, true);

		auto decl = std::make_unique<NodeSingleDeclaration>(
			std::make_shared<NodeVariable>(
				std::nullopt,
				m_def_provider->get_fresh_name("_short_circuit"),
				TypeRegistry::Integer,
				node.tok,
				DataType::Mutable
			),
			std::make_unique<NodeInt>(0, node.tok),
			node.tok
		);
		decl->variable->is_local = true; // ensure it is a local variable
		m_short_circuit_var = decl->variable.get();
		new_outer_block->add_as_stmt(std::move(decl));

		// HIER IST DIE ÄNDERUNG: Statt 'node.condition->accept(*this)' rufen wir
		// unsere saubere, rekursive Funktion auf.
		auto sc_logic_tree = build_sc_logic(std::move(node.condition));
		new_outer_block->add_as_stmt(std::move(sc_logic_tree));

		// Der Rest bleibt unverändert.
		auto final_if = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::EQUAL,
				m_short_circuit_var->to_reference(),
				std::make_unique<NodeInt>(1, node.tok),
				node.tok
			),
			std::move(node.if_body),
			std::move(node.else_body),
			node.tok
		);
		new_outer_block->add_as_stmt(std::move(final_if));
		return node.replace_with(std::move(new_outer_block));
	}

private:
    /**
     * @brief Die rekursive Kernfunktion (korrigierte Version).
     * @param condition Der auszuwertende Ausdruck.
     * @param success_block Der Block, in den die Erfolgszuweisung '_short_circuit = 1' eingefügt werden soll.
     */
    std::unique_ptr<NodeIf> build_sc_logic(std::unique_ptr<NodeAST> condition) {
        if (auto bin_expr = condition->cast<NodeBinaryExpr>()) {
        	if (bin_expr->op == token::BOOL_AND) {
        		// 'L and R' wird zu -> if(L){ <Logik für R> }
        		auto logic_L = build_sc_logic(std::move(bin_expr->left));
        		auto logic_R = build_sc_logic(std::move(bin_expr->right));

        		// "Stitching": Ersetze den Erfolgsblock von L durch die Logik von R.
        		if (auto* if_L_node = logic_L->cast<NodeIf>()) {
        			// WICHTIG: Ersetze den Body, anstatt nur hinzuzufügen.
        			if_L_node->set_if_body(std::make_unique<NodeBlock>(logic_R->tok, true));
        			if_L_node->if_body->add_as_stmt(std::move(logic_R));
        		}
        		return logic_L;

        	} else if (bin_expr->op == token::BOOL_OR) {
        		// 'L or R' wird zu -> if(L){ ERFOLG } else { <Logik für R> }
        		auto logic_L = build_sc_logic(std::move(bin_expr->left));
        		auto logic_R = build_sc_logic(std::move(bin_expr->right));

        		// "Stitching": Füge die Logik von R in den 'else'-Zweig von L ein.
        		if (auto* if_L_node = logic_L->cast<NodeIf>()) {
        			if_L_node->else_body->add_as_stmt(std::move(logic_R));
        		}
        		return logic_L;
        	}
        }

    	// ---- Basisfall der Rekursion ----
    	// Gibt eine saubere 'if(condition){ _short_circuit = 1; }' Struktur zurück.
    	auto tok = condition->tok;
    	auto final_if = std::make_unique<NodeIf>(tok);
    	final_if->set_condition(std::move(condition)); // condition wird hier sicher verwendet
    	final_if->if_body->add_as_stmt(
    		std::make_unique<NodeSingleAssignment>(
				m_short_circuit_var->to_reference(),
				std::make_unique<NodeInt>(1, tok),
				tok
			)
		);
    	return final_if;
    }

};


class ShortCircuitNeed final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	bool do_short_circuit = false;
public:
	bool needs_transformation(NodeBinaryExpr &node) {
		do_short_circuit = false;
		node.accept(*this);
		return do_short_circuit;
	}

	NodeAST* visit(NodeBinaryExpr& node) override {
		if ( node.has_return_func_and_bool()) {
			do_short_circuit = true;
			return &node;
		}
		node.left->accept(*this);
		node.right->accept(*this);
		return &node;
	}
};