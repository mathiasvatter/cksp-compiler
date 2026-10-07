//
// Created for LSP type inlay hints.
//

#pragma once

#include <string>
#include <unordered_map>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/Source/ReferenceIndex.h"

/**
 * Harvests the inferred type of every declaration the source writes without one.
 *
 * Two passes, because neither point in the pipeline knows both halves. Right after parsing
 * a missing annotation is still visible as an unknown type, but nothing is inferred yet.
 * After TypeInference the type is known, but no longer whether the source wrote it. The
 * Untyped pass therefore records where those declarations end, keyed by their name token,
 * and the Inferred pass looks the same tokens up again. Desugaring and parameter renaming
 * change names but keep the token.
 */
class TypeHintHarvester final : public ASTVisitor {
public:
	enum class Pass { Untyped, Inferred };
	/// Name token key -> where the hint goes.
	using UntypedDeclarations = std::unordered_map<std::string, SourcePosition>;

private:
	UntypedDeclarations& m_untyped;
	ReferenceIndex* m_index;
	Pass m_pass;

	static std::string key_of(const Token& token) {
		return token.file() + "@" + std::to_string(token.line) + ":" + std::to_string(token.pos);
	}

	/// A sigil (`$x`) or an annotation gives a declaration its type before inference.
	static bool is_untyped(const Type* type) {
		return !type || type->to_string().find("unknown") != std::string::npos;
	}

	/// After the name, or after the brackets of an array: `declare values[2]: int[]`.
	static SourcePosition hint_position(const NodeDataStructure& node, const bool after_range) {
		if (after_range && node.range.is_valid() && node.range.end.line == node.tok.line) {
			return node.range.end;
		}
		return source_range_from_token(node.tok).end;
	}

	void harvest(const NodeDataStructure& node, const bool after_range = false) const {
		const auto& token = node.tok;
		// Names assembled from a macro parameter stand in no file the user sees.
		if (token.origin || token.file().empty() || token.val.empty()) return;
		if (token.val == NodeStruct::SELF) return;

		if (m_pass == Pass::Untyped) {
			if (is_untyped(node.ty)) m_untyped.try_emplace(key_of(token), hint_position(node, after_range));
			return;
		}
		if (!m_index || is_untyped(node.ty)) return;
		const auto untyped = m_untyped.find(key_of(token));
		if (untyped == m_untyped.end()) return;
		m_index->add_type_hint(token.file(), untyped->second, node.ty->to_string());
	}

public:
	TypeHintHarvester(UntypedDeclarations& untyped, ReferenceIndex* index, const Pass pass)
		: m_untyped(untyped), m_index(index), m_pass(pass) {}

	NodeAST* visit(NodeConst& node) override {
		return &node;
	}

	/// The return type goes after the closing parenthesis, where the header range ends
	/// when no annotation follows: `function scale(x): int`.
	NodeAST* visit(NodeFunctionHeader& node) override {
		const auto& token = node.tok;
		const auto* function_type = node.ty ? node.ty->cast<FunctionType>() : nullptr;
		if (function_type && !token.origin && !token.file().empty() && node.range.is_valid()) {
			const auto* return_type = function_type->get_return_type();
			if (m_pass == Pass::Untyped) {
				if (is_untyped(return_type)) m_untyped.try_emplace(key_of(token), node.range.end);
			} else if (m_index && !is_untyped(return_type)) {
				if (const auto untyped = m_untyped.find(key_of(token)); untyped != m_untyped.end()) {
					m_index->add_type_hint(token.file(), untyped->second, return_type->to_string());
				}
			}
		}
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariable& node) override {
		harvest(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointer& node) override {
		harvest(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArray& node) override {
		harvest(node, true);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArray& node) override {
		harvest(node, true);
		return ASTVisitor::visit(node);
	}
};
