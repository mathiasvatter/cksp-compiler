//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

/**
 * This class completes ASTNodes like arrays, UI controls, etc., by filling in declaration information.
 * It tracks data structure definitions with DefinitionProvider and sets scopes for body nodes.
 * It adds scope to the following:
 * - if-statement
 * - select-statement
 * - while-statement
 * - function definition
 * - callback
 */
class ASTBuildDataStructures: public ASTVisitor {
public:
	explicit ASTBuildDataStructures(DefinitionProvider* definition_provider);

    void visit(NodeProgram& node) override;
	void visit(NodeCallback& node) override;

	void visit(NodeBody& node) override;
	void visit(NodeUIControl& node) override;

	void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;

	void visit(NodeNDArray& node) override;
    void visit(NodeNDArrayRef& node) override;

	/// List Struct and Reference
	void visit(NodeListStruct& node) override;
	void visit(NodeListStructRef& node) override;
	/// Variable
	void visit(NodeVariable& node) override;
    void visit(NodeVariableRef& node) override;

	/// TODO: Visit Function Calls and connect to function definitions to check scoping? Not needed really
private:
    NodeProgram* m_program = nullptr;
	NodeCallback* m_init_callback = nullptr;
	bool m_is_init_callback = false;
	DefinitionProvider* m_def_provider = nullptr;


//    static inline ASTType infer_type_from_identifier(std::string& var_name) {
//        ASTType type = ASTType::Unknown;
//        if(contains(VAR_IDENT, var_name[0]) || contains(ARRAY_IDENT, var_name[0])) {
//            std::string identifier(1, var_name[0]);
//            var_name = var_name.erase(0,1);
//            token token_type = *get_token_type(TYPES, identifier);
//            type = token_to_type(token_type);
//        }
//        return type;
//    }

	/// Checks for existence and uniqueness of "on init" callback
	/// If found, returns pointer to the callback node
	static NodeCallback* move_on_init_callback(NodeProgram& node);
	/// Checks for uniqueness of all callbacks except "on ui_control"
	static bool check_unique_callbacks(NodeProgram& node);
};

