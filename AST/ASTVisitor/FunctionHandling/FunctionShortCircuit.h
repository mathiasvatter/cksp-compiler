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

		auto sc_logic_tree = build_sc_logic(std::move(node.condition), m_short_circuit_var);
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
	// Erzeugt eine Deklaration für eine neue, initialisierte temporäre Variable
	std::unique_ptr<NodeSingleDeclaration> create_temp_var_decl(
		std::shared_ptr<NodeVariable>& out_var_ptr, // Gibt den erzeugten Variablen-Knoten zurück
		const Token& tok
	) {
		out_var_ptr = std::make_shared<NodeVariable>(
			std::nullopt,
			m_def_provider->get_fresh_name("_sc_temp"),
			TypeRegistry::Integer,
			tok,
			DataType::Mutable
		);
		out_var_ptr->is_local = true;
		return std::make_unique<NodeSingleDeclaration>(
			out_var_ptr,
			std::make_unique<NodeInt>(0, tok),
			tok
		);
	}

	// Erzeugt eine Zuweisung `target_var = 1;`
	std::unique_ptr<NodeSingleAssignment> create_success_assignment(
		NodeDataStructure* target_var,
		const Token& tok
	) {
		return std::make_unique<NodeSingleAssignment>(
			target_var->to_reference(),
			std::make_unique<NodeInt>(1, tok),
			tok
		);
	}


	/**
	 * @brief Die EINE, vereinheitlichte rekursive Funktion.
	 * @param condition Der auszuwertende Ausdruck.
	 * @param target_var Die Variable, die bei Erfolg auf 1 gesetzt werden soll.
	 * @return Ein Statement oder Block mit der Short-Circuit-Logik.
	 */
	std::unique_ptr<NodeAST> build_sc_logic(std::unique_ptr<NodeAST> condition, NodeDataStructure* target_var) {
		if (!condition) {
			return std::make_unique<NodeBlock>(Token{}, true);
		}

		if (auto* bin_expr = condition->cast<NodeBinaryExpr>(); bin_expr and bin_expr->needs_short_circuiting()) {
			if (bin_expr->op == token::BOOL_XOR) {
				// ----- XOR-LOGIK -----
				// Erzeugt einen Block mit temp. Variablen für L und R.

				auto xor_block = std::make_unique<NodeBlock>(bin_expr->tok, true);
				std::shared_ptr<NodeVariable> temp_L_var, temp_R_var;

				xor_block->add_as_stmt(create_temp_var_decl(temp_L_var, bin_expr->tok));
				xor_block->add_as_stmt(create_temp_var_decl(temp_R_var, bin_expr->tok));

				// Rekursiver Aufruf für L, ZIEL = temp_L_var
				xor_block->add_as_stmt(build_sc_logic(std::move(bin_expr->left), temp_L_var.get()));

				// Rekursiver Aufruf für R, ZIEL = temp_R_var
				xor_block->add_as_stmt(build_sc_logic(std::move(bin_expr->right), temp_R_var.get()));

				// Finaler Vergleich: if (_temp_L != _temp_R) { target_var = 1; }
				auto final_xor_if = std::make_unique<NodeIf>(bin_expr->tok);
				final_xor_if->set_condition(
					std::make_unique<NodeBinaryExpr>(token::NOT_EQUAL, temp_L_var->to_reference(), temp_R_var->to_reference(), bin_expr->tok)
				);
				// Bei Erfolg wird die ursprüngliche Zielvariable gesetzt.
				final_xor_if->if_body->add_as_stmt(create_success_assignment(target_var, bin_expr->tok));
				xor_block->add_as_stmt(std::move(final_xor_if));

				return xor_block;

			} else if (bin_expr->op == token::BOOL_AND) {
				// ----- HYBRIDE AND-LOGIK -----

			    // Prüfe, ob die linke Seite (L) "einfach" ist, d.h. nicht selbst
			    // AND, OR, XOR oder NOT enthält und somit direkt als Bedingung dienen kann.
			    bool left_is_simple = true;
			    if (auto* left_as_bin = bin_expr->left->cast<NodeBinaryExpr>()) {
			        if (left_as_bin->op == token::BOOL_AND || left_as_bin->op == token::BOOL_OR || left_as_bin->op == token::BOOL_XOR) {
			            left_is_simple = false;
			        }
			    } else if (auto* left_as_unary = bin_expr->left->cast<NodeUnaryExpr>()) {
			        if (left_as_unary->op == token::BOOL_NOT) {
			            left_is_simple = false;
			        }
			    }

			    if (left_is_simple) {
			        // --- Eleganter Fall: L ist einfach ---
			        // Erzeuge direkt 'if (L) { <Logik für R> }'

			        auto simple_if = std::make_unique<NodeIf>(bin_expr->tok);
			        // L wird direkt als Bedingung eingesetzt.
			        simple_if->set_condition(std::move(bin_expr->left));
			        // Der Body enthält die Logik für R, die bei Erfolg das finale Ziel setzt.
			        simple_if->if_body->add_as_stmt(build_sc_logic(std::move(bin_expr->right), target_var));

			        return simple_if;

			    } else {
			        // --- Robuster Fall: L ist komplex (z.B. XOR oder ein anderes AND/OR) ---
			        // Hier ist die temporäre Variable unvermeidlich.

			        auto and_block = std::make_unique<NodeBlock>(bin_expr->tok, true);
			        std::shared_ptr<NodeVariable> temp_L_var;

			        // 1. Temp. Variable für das Ergebnis von L erstellen.
			        and_block->add_as_stmt(create_temp_var_decl(temp_L_var, bin_expr->tok));

			        // 2. Logik für das komplexe L ausführen, die NUR in die temp. Variable schreibt.
			        and_block->add_as_stmt(build_sc_logic(std::move(bin_expr->left), temp_L_var.get()));

			        // 3. Nur wenn L erfolgreich war, die Logik für R ausführen.
			        auto if_L_succeeded = std::make_unique<NodeIf>(bin_expr->tok);
			        if_L_succeeded->set_condition(
			             std::make_unique<NodeBinaryExpr>(token::EQUAL, temp_L_var->to_reference(), std::make_unique<NodeInt>(1, bin_expr->tok), bin_expr->tok)
			        );
			        if_L_succeeded->if_body->add_as_stmt(build_sc_logic(std::move(bin_expr->right), target_var));
			        and_block->add_as_stmt(std::move(if_L_succeeded));

			        return and_block;
			    }

			} else if (bin_expr->op == token::BOOL_OR) {
				// 'L or R' wird zu -> if(L){ ERFOLG } else { <Logik für R> }

				// KORREKTUR: Rufe auch hier den Haupt-Disponenten `build_sc_logic` auf!
				auto logic_L = build_sc_logic(std::move(bin_expr->left), target_var);
				auto logic_R = build_sc_logic(std::move(bin_expr->right), target_var);

				if (auto* if_L_node = logic_L->cast<NodeIf>()) {
					if_L_node->else_body->add_as_stmt(std::move(logic_R));
					return logic_L;
				} else {
					auto or_block = std::make_unique<NodeBlock>(bin_expr->tok, true);
					or_block->add_as_stmt(std::move(logic_L));

					auto if_not_set = std::make_unique<NodeIf>(bin_expr->tok);
					if_not_set->set_condition(
						std::make_unique<NodeBinaryExpr>(token::EQUAL, target_var->to_reference(), std::make_unique<NodeInt>(0, bin_expr->tok), bin_expr->tok)
					);
					if_not_set->if_body->add_as_stmt(std::move(logic_R));
					or_block->add_as_stmt(std::move(if_not_set));
					return or_block;
				}
			}
		} else if (auto unary_expr = condition->cast<NodeUnaryExpr>(); unary_expr and unary_expr->needs_short_circuiting()) {
			// ----- HIER IST DIE NEUE NOT-LOGIK -----
			if (unary_expr->op == token::BOOL_NOT) {
				// Strategie für 'not E':
				// 1. Erzeuge Block: { declare _temp=0; }
				// 2. Fülle ihn mit: <Logik für E, die bei Erfolg _temp=1 setzt>
				// 3. Füge hinzu: if (_temp == 0) { ursprüngliches_ziel = 1; }

				auto not_block = std::make_unique<NodeBlock>(unary_expr->tok, true);
				std::shared_ptr<NodeVariable> temp_var;

				// 1. Temporäre Variable für das Ergebnis des inneren Ausdrucks erstellen.
				not_block->add_as_stmt(create_temp_var_decl(temp_var, unary_expr->tok));

				// 2. Logik für den inneren Ausdruck (E) erzeugen, die in die temp. Variable schreibt.
				not_block->add_as_stmt(build_sc_logic(std::move(unary_expr->operand), temp_var.get()));

				// 3. Finaler Check: Wenn die temp. Variable NICHT gesetzt wurde (also 0 ist),
				//    dann war der innere Ausdruck falsch, und 'not E' ist somit wahr.
				auto final_not_if = std::make_unique<NodeIf>(unary_expr->tok);
				final_not_if->set_condition(
				   std::make_unique<NodeBinaryExpr>(
					  token::EQUAL, // Prüfe auf Gleichheit mit 0
					  temp_var->to_reference(),
					  std::make_unique<NodeInt>(0, unary_expr->tok),
					  unary_expr->tok
				   )
				);
				// Bei Erfolg wird die ursprüngliche Zielvariable gesetzt.
				final_not_if->if_body->add_as_stmt(create_success_assignment(target_var, unary_expr->tok));
				not_block->add_as_stmt(std::move(final_not_if));

				return not_block;
			}
		}

		// ---- Basisfall der Rekursion ----
		// Erzeugt: if(condition) { target_var = 1; }
		auto tok = condition->tok;
		auto final_if = std::make_unique<NodeIf>(tok);
		final_if->set_condition(std::move(condition));
		final_if->if_body->add_as_stmt(create_success_assignment(target_var, tok));
		return final_if;
	}
};





class ShortCircuitNeed final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	bool do_short_circuit = false;
public:
	bool needs_transformation(NodeAST &node) {
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

	NodeAST* visit(NodeUnaryExpr& node) override {
		if ( node.has_return_func_and_bool()) {
			do_short_circuit = true;
			return &node;
		}
		node.operand->accept(*this);
		return &node;
	}
};