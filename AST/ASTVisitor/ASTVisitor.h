//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include <unordered_map>
#include <set>
#include <unordered_set>

#include "../TypeRegistry.h"
#include "../ASTNodes/AST.h"
#include "../ASTNodes/ASTInstructions.h"
#include "../ASTNodes/ASTDataStructures.h"
#include "../ASTNodes/ASTReferences.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVisitor {
protected:
	NodeProgram* m_program = nullptr;
//	NodeCallback* m_current_callback = nullptr;

    std::set<std::string> m_restricted_builtin_functions = {"save_array", "save_array_str", "load_array", "load_array_str"};
    std::unordered_map<std::string, Type*> m_compiler_variables = {{"_list_it",TypeRegistry::Integer}, {"_ui_array_it", TypeRegistry::Integer},
                                                                     {"_string_it", TypeRegistry::Integer},
                                                                     {"_iterator", TypeRegistry::Integer}};
    std::unordered_map<Type*, std::string> m_return_arrays = {{TypeRegistry::ArrayOfInt, "_return_vars_int"}, {TypeRegistry::ArrayOfReal, "_return_vars_real"}, {TypeRegistry::ArrayOfString, "_return_vars_str"}};
    std::unordered_map<Type*, std::string> m_local_var_arrays = {{TypeRegistry::ArrayOfInt, "_loc_var_int"}, {TypeRegistry::ArrayOfReal, "_loc_var_real"}, {TypeRegistry::ArrayOfString, "_loc_var_str"}};


    std::unique_ptr<NodeStatement> make_declare_variable(const std::string& name, int32_t value, DataType type, NodeAST* parent);
    std::unique_ptr<NodeBody> array_initialization(NodeArray* array, NodeParamList* list);
    static std::unique_ptr<NodeBody> make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeBody> body, NodeAST* parent);

public:
    virtual void visit(NodeDeadCode& node) {};
	virtual void visit(NodeInt& node) {node.type = ASTType::Integer;};
    virtual void visit(NodeReal& node) {node.type = ASTType::Real;};
    virtual void visit(NodeString& node) {node.type = ASTType::String;};
	virtual void visit(NodeVariable& node) {};
	virtual void visit(NodeVariableRef& node) {};
    virtual void visit(NodeParamList& node) {
		for(auto & param : node.params) {
			param->accept(*this);
		}
	};
	virtual void visit(NodeArray& node) {
		if(node.size) node.size->accept(*this);
	};
	virtual void visit(NodeArrayRef& node) {
		if(node.index) node.index->accept(*this);
	};
	virtual void visit(NodeNDArray& node) {
		node.sizes->accept(*this);
	};
	virtual void visit(NodeNDArrayRef& node) {
		if(node.indexes) node.indexes->accept(*this);
	};
    virtual void visit(NodeUIControl& node){
		node.control_var->accept(*this);
		node.params->accept(*this);
	};
    virtual void visit(NodeUnaryExpr& node) {
		node.operand->accept(*this);
	};
    virtual void visit(NodeBinaryExpr& node) {
		node.left->accept(*this);
		node.right->accept(*this);
	};
    virtual void visit(NodeDeclaration& node) {
        for(auto const &decl : node.variable) {
            decl->accept(*this);
        }
		if(node.value)
        	node.value -> accept(*this);
	};
    virtual void visit(NodeSingleDeclaration& node) {
        node.variable ->accept(*this);
		if(node.value)
        	node.value -> accept(*this);
    };
    virtual void visit(NodeAssignment& node) {
		node.l_value->accept(*this);
		node.r_value->accept(*this);
	};
    virtual void visit(NodeSingleAssignment& node) {
        node.l_value ->accept(*this);
		node.r_value -> accept(*this);
    };
	virtual void visit(NodeReturn& node) {
		CompileError(ErrorType::SyntaxError, "<return> node not yet implemented.", "", node.tok).exit();
		for(auto &ret : node.return_variables) {
			ret->accept(*this);
		}
	};
    virtual void visit(NodeGetControl& node) {
		node.ui_id->accept(*this);
	};

    virtual void visit(NodeConstStatement& node) {
        node.constants->accept(*this);
	};
    virtual void visit(NodeStruct& node) {
		for(auto & member : node.members) {
			member->accept(*this);
		}
	};
    virtual void visit(NodeFamily& node) {
        node.members->accept(*this);
	};
    virtual void visit(NodeListStruct& node) {
        for(auto & b : node.body) {
            b->accept(*this);
        }
    };
	virtual void visit(NodeListStructRef& node) {
		node.indexes->accept(*this);
	};
    virtual void visit(NodeStatement& node) {
		node.statement->accept(*this);
	};
    virtual void visit(NodeIfStatement& node) {
		node.condition->accept(*this);
		node.statements->accept(*this);
		node.else_statements->accept(*this);
	};
    virtual void visit(NodeForStatement& node) {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
        node.statements->accept(*this);
	};
    virtual void visit(NodeForEachStatement& node) {
        node.keys->accept(*this);
        node.range->accept(*this);
        node.statements->accept(*this);
    };
	virtual void visit(NodeWhileStatement& node) {
		node.condition->accept(*this);
        node.statements->accept(*this);
	};
	virtual void visit(NodeSelectStatement& node) {
		node.expression->accept(*this);
		for(const auto &cas: node.cases) {
            for(auto &c: cas.first) {
                c->accept(*this);
            }
            cas.second->accept(*this);
		}
	};
	virtual void visit(NodeCallback& node) {
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
	};
    virtual void visit(NodeFunctionHeader& node) {
		node.args->accept(*this);
	};
    virtual void visit(NodeFunctionCall& node) {
		node.function->accept(*this);
	};
	virtual void visit(NodeFunctionDefinition& node) {
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
        node.body->accept(*this);
	};
    virtual void visit(NodeProgram& node) {
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
	};
    virtual void visit(NodeBody& node) {
        for(auto & stmt : node.statements) {
            stmt->accept(*this);
        }
    };
    virtual void visit(NodeImport& node) {
    };
};



