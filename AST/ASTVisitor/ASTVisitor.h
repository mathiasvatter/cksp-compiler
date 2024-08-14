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

    inline static const std::unordered_set<std::string> m_restricted_builtin_functions = {"save_array", "save_array_str", "load_array", "load_array_str"};
    inline static const std::unordered_map<Type*, std::string> m_return_arrays = {{TypeRegistry::ArrayOfInt, "_return_vars_int"}, {TypeRegistry::ArrayOfReal, "_return_vars_real"}, {TypeRegistry::ArrayOfString, "_return_vars_str"}};
    inline static const std::unordered_map<Type*, std::string> m_local_var_arrays = {{TypeRegistry::ArrayOfInt, "_loc_var_int"}, {TypeRegistry::ArrayOfReal, "_loc_var_real"}, {TypeRegistry::ArrayOfString, "_loc_var_str"}};

    static std::unique_ptr<NodeBlock> make_while_loop(NodeAST* var, int32_t from, int32_t to, std::unique_ptr<NodeBlock> body, NodeAST* parent);

	static CompileError get_raw_compile_error(ErrorType err_type, const NodeAST& node);

public:
    virtual NodeAST* visit(NodeDeadCode& node) {return &node;};
	virtual NodeAST* visit(NodeWildcard& node) {
		return &node;
	};
	virtual NodeAST* visit(NodeInt& node) {
		return &node;
	};
    virtual NodeAST* visit(NodeReal& node) {
		return &node;
	};
    virtual NodeAST* visit(NodeString& node) {
		return &node;
	};
	virtual NodeAST* visit(NodeNil& node) {
		return &node;
	};
	virtual NodeAST* visit(NodeVariable& node) {
		return &node;
	};
	virtual NodeAST* visit(NodeVariableRef& node) {
		return &node;
	};
	virtual NodeAST* visit(NodePointerRef& node) {
		return &node;
	};
	virtual NodeAST* visit(NodePointer& node) {
		return &node;
	};
    virtual NodeAST* visit(NodeParamList& node) {
		for(auto & param : node.params) {
			param->accept(*this);
		}
		return &node;
	};
	virtual NodeAST* visit(NodeArray& node) {
		if(node.size) node.size->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeArrayRef& node) {
		if(node.index) node.index->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeNDArray& node) {
		if(node.sizes) node.sizes->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeNDArrayRef& node) {
		if(node.indexes) node.indexes->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeAccessChain& node) {
		for(auto & method : node.chain) {
			method->accept(*this);
		}
		return &node;
	}
    virtual NodeAST* visit(NodeUIControl& node){
		node.control_var->accept(*this);
		node.params->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeUnaryExpr& node) {
		node.operand->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeBinaryExpr& node) {
		node.left->accept(*this);
		node.right->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeDeclaration& node) {
        for(auto const &decl : node.variable) {
            decl->accept(*this);
        }
		if(node.value) node.value -> accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeSingleDeclaration& node) {
        node.variable ->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
    };
    virtual NodeAST* visit(NodeAssignment& node) {
		node.l_value->accept(*this);
		node.r_value->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeSingleAssignment& node) {
        node.l_value ->accept(*this);
		node.r_value -> accept(*this);
		return &node;
    };
	virtual NodeAST* visit(NodeReturn& node) {
		for(auto &ret : node.return_variables) {
			ret->accept(*this);
		}
		return &node;
	};
	virtual NodeAST* visit(NodeSingleReturn& node) {
		node.return_variable->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeDelete& node) {
		CompileError(ErrorType::InternalError, "<Delete> node not yet implemented.", "", node.tok).exit();
		for(auto &del : node.delete_pointer) {
			del->accept(*this);
		}
		return &node;
	};
    virtual NodeAST* visit(NodeGetControl& node) {
		node.ui_id->accept(*this);
		return &node;
	};

    virtual NodeAST* visit(NodeConst& node) {
        node.constants->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeStruct& node) {
		node.members->accept(*this);
		for(auto & m: node.methods) {
			m->accept(*this);
		}
		return &node;
	};
    virtual NodeAST* visit(NodeFamily& node) {
        node.members->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeList& node) {
        for(auto & b : node.body) {
            b->accept(*this);
        }
		return &node;
    };
	virtual NodeAST* visit(NodeListRef& node) {
		node.indexes->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeStatement& node) {
		node.statement->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeIf& node) {
		node.condition->accept(*this);
		node.if_body->accept(*this);
		node.else_body->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeFor& node) {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
        node.body->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeForEach& node) {
        node.keys->accept(*this);
        node.range->accept(*this);
        node.body->accept(*this);
		return &node;
    };
	virtual NodeAST* visit(NodeWhile& node) {
		node.condition->accept(*this);
        node.body->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeSelect& node) {
		node.expression->accept(*this);
		for(const auto &cas: node.cases) {
            for(auto &c: cas.first) {
                c->accept(*this);
            }
            cas.second->accept(*this);
		}
		return &node;
	};
	virtual NodeAST* visit(NodeCallback& node) {
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeFunctionHeader& node) {
		node.args->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeFunctionCall& node) {
		node.function->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeFunctionDefinition& node) {
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
        node.body->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeProgram& node) {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
		node.merge_function_definitions();
		node.reset_function_visited_flag();
		return &node;
	};
    virtual NodeAST* visit(NodeBlock& node) {
        for(auto & stmt : node.statements) {
            stmt->accept(*this);
        }
		return &node;
    };
    virtual NodeAST* visit(NodeImport& node) {
		return &node;
    };
};



