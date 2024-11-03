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

	/// map value will be reset after ref is in one of these functions
	inline static const std::unordered_set<std::string> no_propagation = {
		"inc", "dec",
	};

    inline static const std::unordered_map<Type*, std::string> m_return_arrays = {{TypeRegistry::ArrayOfInt, "_return_vars_int"}, {TypeRegistry::ArrayOfReal, "_return_vars_real"}, {TypeRegistry::ArrayOfString, "_return_vars_str"}};
    inline static const std::unordered_map<Type*, std::string> m_local_var_arrays = {{TypeRegistry::ArrayOfInt, "_loc_var_int"}, {TypeRegistry::ArrayOfReal, "_loc_var_real"}, {TypeRegistry::ArrayOfString, "_loc_var_str"}};

public:
	static CompileError get_raw_compile_error(ErrorType err_type, const NodeAST& node);
    static std::unique_ptr<NodeBlock> make_while_loop(NodeReference* var, int32_t from, int32_t to, std::unique_ptr<NodeBlock> body, NodeAST* parent);
	static std::unique_ptr<NodeIf> make_nil_check(std::unique_ptr<NodeReference> ref);
	static std::shared_ptr<NodeVariable> get_iterator_var(const Token& tok) {
		auto iter = std::make_shared<NodeVariable>(
			std::nullopt,
			"_iter",
			TypeRegistry::Integer,
			DataType::Mutable,
			tok
		);
		iter->is_local = true;
		return iter;
	}

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
	virtual NodeAST* visit(NodeReferenceList& node) {
		for(auto & ref : node.references) ref->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeParamList& node) {
		for(auto & param : node.params) param->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeInitializerList& node) {
		for(auto & elem : node.elements) elem->accept(*this);
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
		for(auto & method : node.chain) method->accept(*this);
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
	virtual NodeAST* visit(NodeFunctionParam& node) {
		node.variable ->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeDeclaration& node) {
        for(auto const &decl : node.variable) decl->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeSingleDeclaration& node) {
        node.variable ->accept(*this);
		if(node.value) node.value -> accept(*this);
		if(node.retain_stmt) node.retain_stmt->accept(*this);
		return &node;
    };
    virtual NodeAST* visit(NodeAssignment& node) {
		for(auto& l_val : node.l_values) l_val->accept(*this);
		node.r_values->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeSingleAssignment& node) {
		if(node.delete_stmt) node.delete_stmt->accept(*this);
        node.l_value ->accept(*this);
		node.r_value -> accept(*this);
		if(node.retain_stmt) node.retain_stmt->accept(*this);
		return &node;
    };
	virtual NodeAST* visit(NodeBreak& node) {return &node;}
	virtual NodeAST* visit(NodeReturn& node) {
		for(auto &ret : node.return_variables) ret->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeSingleReturn& node) {
		node.return_variable->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeDelete& node) {
//		CompileError(ErrorType::InternalError, "<Delete> node not yet implemented.", "", node.tok).exit();
		for(auto &del : node.ptrs) {
			del->accept(*this);
		}
		return &node;
	};
	virtual NodeAST* visit(NodeSingleDelete& node) {
		node.ptr->accept(*this);
		if(node.num) node.num->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeNumElements& node) {
		node.array->accept(*this);
		if(node.dimension) node.dimension->accept(*this);
		if(node.size) node.size->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeUseCount& node) {
		node.ref->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeSingleRetain& node) {
		node.ptr->accept(*this);
		node.num->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeRetain& node) {
		for(auto &ptr : node.ptrs)
			ptr->accept(*this);
		return &node;
	};
    virtual NodeAST* visit(NodeGetControl& node) {
		node.ui_id->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeSetControl& node) {
		node.ui_id->accept(*this);
		node.value->accept(*this);
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
		for(auto &key : node.keys) key->accept(*this);
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
		for(auto &param : node.params) param->variable->accept(*this);
		return &node;
	};
	virtual NodeAST* visit(NodeFunctionHeaderRef& node) {
		if(node.args) node.args->accept(*this);
		return &node;
	}
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
		for(auto & func_def : node.function_definitions) {
			func_def->accept(*this);
		}
		node.merge_function_definitions();
		node.reset_function_visited_flag();
		return &node;
	};
    virtual NodeAST* visit(NodeBlock& node) {
        for(auto & stmt : node.statements) {
			if(!stmt) {
				auto error = CompileError(ErrorType::InternalError, "Null statement in block.", "", node.tok);
				error.exit();
			}
			stmt->accept(*this);
		}
		return &node;
    };
    virtual NodeAST* visit(NodeImport& node) {
		return &node;
    };
};



