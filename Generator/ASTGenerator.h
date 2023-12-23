//
// Created by Mathias Vatter on 21.11.23.
//

#pragma once

#include <sstream>
#include "../AST/ASTVisitor.h"

template<typename T, typename U>
U get_pair_value(const std::vector<std::pair<T, U>>& vec, T type) {
	auto it = std::find_if(vec.begin(), vec.end(), [type](const std::pair<T, U>& pair) {
	  return pair.first == type;
	});

	if (it != vec.end()) {
		return it->second;
	}

	// Hier können Sie entscheiden, was passieren soll, wenn kein Match gefunden wird.
	// Zum Beispiel könnten Sie einen Standardwert zurückgeben oder eine Ausnahme werfen.
	return U();  // Gibt den Standardwert des Typs U zurück.
}

inline std::string sanitize_dots(const std::string& str) {
	std::string result;
	for (char ch : str) {
		if (ch == '.') {
			result += "__";
		} else {
			result += ch;
		}
	}
	return result;
}


class ASTGenerator : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;
    void visit(NodeInt& node) override;
    void visit(NodeReal& node) override;
	void visit(NodeString& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeParamList& node) override;
    void visit(NodeBinaryExpr& node) override;
    void visit(NodeUnaryExpr& node) override;
    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeStatement& node) override;
    void visit(NodeIfStatement& node) override;
    void visit(NodeWhileStatement& node) override;
    void visit(NodeSelectStatement& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionHeader& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;
    void visit(NodeGetControlStatement& node) override;
    void visit(NodeSetControlStatement& node) override;

    std::ostringstream os;


	void generate(const std::string& path) const;
	void print() const;

	std::vector<std::pair<ASTType, char>> array_identifier = {{String, '!'}, {Integer, '%'}, {Real, '?'}};
	std::vector<std::pair<ASTType, char>> variable_identifier = {{String, '@'}, {Integer, '$'}, {Real, '~'}};

private:
    static std::string get_compiled_date_time();
};


