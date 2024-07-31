//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../Lowering/ASTLowering.h"

/**
 * @class ASTCollectLowerings
 *
 * @brief This class is responsible for lowering various data structure AST nodes to a more primitive form.
 *
 * It inherits from the ASTVisitor class and overrides its visit methods to provide custom behavior for each node type.
 * The lowering process is necessary for certain operations that are not directly supported by the target language.
 *
 * The following AST nodes are being lowered by this class:
 * - NodeSingleDeclaration: Lower ndarray when declaration or ui_control array or determine size of array in declaration.
 * - NodeSingleAssignment: Lower get_control statements to set_control_par.
 * - NodeGetControl: Lower get_control statements to get_control_par.
 * - NodeFunctionCall: Lower property functions to get_control_par.
 * - NodeNDArray: Lower ndArray when they are a reference.
 * - NodeConst: Lower const block to single declare statements.
 * - NodeListRef: Lower list struct references to array references.
 * - NodeList: Lower list structs to arrays and while loops.
 *
 * @param definition_provider A pointer to a DefinitionProvider object. This object is used to resolve definitions of
 * variables, arrays, etc.
 */
class ASTCollectLowerings: public ASTVisitor {
public:
    explicit ASTCollectLowerings(DefinitionProvider* definition_provider);
	NodeAST * visit(NodeProgram& node) override;

	/// do not visit return statements -> wait for them to get lowered to assign statements
//	void visit(NodeReturn& node) override;

	/// flatten blocks
	NodeAST * visit(NodeBlock& node) override;
	/// lower struct members to array members
	NodeAST * visit(NodeStruct& node) override;
	NodeAST * visit(NodeFunctionDefinition& node) override;
	/// lower ndarray when declaration or ui_control array
	NodeAST * visit(NodeSingleDeclaration& node) override;
	/// lower get_control statements to set_control_par
	NodeAST * visit(NodeSingleAssignment& node) override;
	/// lower get_control statements to get_control_par
	NodeAST * visit(NodeGetControl& node) override;
	/// lower property functions to get_control_par
	NodeAST * visit(NodeFunctionCall& node) override;
    /// determine size of array in declaration if possible
	NodeAST * visit(NodeArray& node) override;
	/// lower ndarray when in function params
	NodeAST * visit(NodeNDArray& node) override;
	/// lower ndArray when they are a reference
	NodeAST * visit(NodeNDArrayRef& node) override;
	/// move node block of const out of const construct
	NodeAST * visit(NodeConst& node) override;
//    void visit(NodeFamily& node) override;

	/// lower list struct references to array references
	NodeAST * visit(NodeListRef& node) override;
	/// lower list structs to arrays and while loops
	NodeAST * visit(NodeList& node) override;

	/// lower pointer reference types and replace with __repr__ if in string surrounding
	NodeAST * visit(NodePointerRef& node) override;
	/// lower access chain to nested array_ref structure
	NodeAST * visit(NodeAccessChain& node) override;
	/// lower nil to -1
	NodeAST * visit(NodeNil& node) override;

private:
    DefinitionProvider* m_def_provider;
};


