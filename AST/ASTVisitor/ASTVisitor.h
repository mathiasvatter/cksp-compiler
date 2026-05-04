//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include "../Types/TypeRegistry.h"
#include "../ASTNodes/AST.h"
#include "../ASTNodes/ASTInstructions.h"
#include "../ASTNodes/ASTDataStructures.h"
#include "../ASTNodes/ASTReferences.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

#define TRACE() \
std::fprintf(stderr, ">> %s\n", __PRETTY_FUNCTION__);

class ASTVisitor {
protected:
	NodeProgram* m_program = nullptr;

public:
	virtual ~ASTVisitor() = default;
	static CompileError get_raw_compile_error(ErrorType err_type, const NodeAST& node);
    static std::unique_ptr<NodeBlock> make_while_loop(NodeReference* var, int32_t from, int32_t to, std::unique_ptr<NodeBlock> body, NodeAST* parent);
	static std::unique_ptr<NodeIf> make_nil_check(std::unique_ptr<NodeReference> ref);
	static std::shared_ptr<NodeVariable> get_iterator_var(const Token& tok, const std::string& name="_iter") {
		auto iter = std::make_shared<NodeVariable>(
			std::nullopt,
			name,
			TypeRegistry::Integer,
			tok,
			DataType::Mutable
		);
		iter->is_local = true;
		// iter->is_engine = true;
		return iter;
	}
	static std::unique_ptr<NodeFunctionCall> get_cksp_kontakt_warning(const std::string& msg, Token tok = Token());

	//	explicit ASTVisitor(NodeProgram* program) : m_program(program) {}

    virtual NodeAST* visit(NodeDeadCode& node) {return &node;}
	virtual NodeAST* visit(NodeWildcard& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeInt& node) {
		return &node;
	}
    virtual NodeAST* visit(NodeReal& node) {
		return &node;
	}
    virtual NodeAST* visit(NodeString& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeBoolean& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeFormatString& node) {
		visit_all(node.elements, *this);
		return &node;
	}
	virtual NodeAST* visit(NodeNil& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeVariable& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeVariableRef& node) {
		if (node.parent->cast<NodeSingleDeclaration>()) {

		}
		return &node;
	}
	virtual NodeAST* visit(NodePointerRef& node) {
		return &node;
	}
	virtual NodeAST* visit(NodePointer& node) {
		return &node;
	}
	virtual NodeAST* visit(NodeReferenceList& node) {
		for(const auto & ref : node.references) ref->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeParamList& node) {
		for(const auto & param : node.params) param->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeInitializerList& node) {
		for(const auto & elem : node.elements) elem->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeArray& node) {
		if(node.size) node.size->accept(*this);
		if(node.num_elements) node.num_elements->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeArrayRef& node) {
		if(node.index) node.index->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeNDArray& node) {
		if(node.sizes) node.sizes->accept(*this);
		if(node.num_elements) node.num_elements->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeNDArrayRef& node) {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeAccessChain& node) {
		for(const auto & method : node.chain) method->accept(*this);
		return &node;
	}

    virtual NodeAST* visit(NodeUIControl& node){
		node.control_var->accept(*this);
		node.params->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeUnaryExpr& node) {
		node.operand->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeBinaryExpr& node) {
		node.left->accept(*this);
		node.right->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeFunctionParam& node) {
		node.variable ->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeDeclaration& node) {
        for(auto const &decl : node.variable) decl->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeSingleDeclaration& node) {
        node.variable ->accept(*this);
		if(node.value) node.value -> accept(*this);
		return &node;
    }
    virtual NodeAST* visit(NodeAssignment& node) {
		for(auto& l_val : node.l_values) l_val->accept(*this);
		node.r_values->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeSingleAssignment& node) {
        node.l_value ->accept(*this);
		node.r_value -> accept(*this);
		return &node;
    }
	virtual NodeAST* visit(NodeCompoundAssignment& node) {
		node.l_value ->accept(*this);
		node.r_value -> accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeBreak& node) {return &node;}
	virtual NodeAST* visit(NodeReturn& node) {
		for(const auto &ret : node.return_variables) ret->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeSingleReturn& node) {
		node.return_variable->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeDelete& node) {
//		CompileError(ErrorType::InternalError, "<Delete> node not yet implemented.", "", node.tok).exit();
		for(const auto &del : node.ptrs) {
			del->accept(*this);
		}
		return &node;
	}
	virtual NodeAST* visit(NodeSingleDelete& node) {
		node.ptr->accept(*this);
		if(node.num) node.num->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeSortSearch& node) {
		node.array->accept(*this);
		node.value->accept(*this);
		if(node.from) node.from->accept(*this);
		if(node.to) node.to->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeNumElements& node) {
		node.array->accept(*this);
		if(node.dimension) node.dimension->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeUseCount& node) {
		node.ref->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodePairs& node) {
		node.range->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeRange& node) {
		if(node.start) node.start->accept(*this);
		node.stop->accept(*this);
		if(node.step) node.step->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeSingleRetain& node) {
		node.ptr->accept(*this);
		node.num->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeRetain& node) {
		for(auto &ptr : node.ptrs)
			ptr->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeGetControl& node) {
		node.ui_id->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeSetControl& node) {
		node.ui_id->accept(*this);
		node.value->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeConst& node) {
        node.constants->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeStruct& node) {
		node.members->accept(*this);
		for(const auto & m: node.methods) {
			m->accept(*this);
		}
		return &node;
	}
    virtual NodeAST* visit(NodeFamily& node) {
        node.members->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeNamespace& node) {
		node.members->accept(*this);
		for(const auto & m: node.function_definitions) {
			m->accept(*this);
		}
		return &node;
	}
    virtual NodeAST* visit(NodeList& node) {
        for(const auto & b : node.body) {
            b->accept(*this);
        }
		return &node;
    }
	virtual NodeAST* visit(NodeListRef& node) {
		node.indexes->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeStatement& node) {
		node.statement->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeIf& node) {
		node.condition->accept(*this);
		node.if_body->accept(*this);
		node.else_body->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeTernary& node) {
		node.condition->accept(*this);
		node.if_branch->accept(*this);
		node.else_branch->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeFor& node) {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
		if(node.step) node.step->accept(*this);
        node.body->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeForEach& node) {
		if(node.key) node.key->accept(*this);
		if(node.value) node.value->accept(*this);
        node.range->accept(*this);
        node.body->accept(*this);
		return &node;
    }
	virtual NodeAST* visit(NodeWhile& node) {
		node.condition->accept(*this);
        node.body->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeSelect& node) {
		node.expression->accept(*this);
		for(const auto &cas: node.cases) {
            for(auto &c: cas.first) {
                c->accept(*this);
            }
            cas.second->accept(*this);
		}
		return &node;
	}
	virtual NodeAST* visit(NodeCallback& node) {
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeFunctionHeader& node) {
		for(const auto &param : node.params) param->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeFunctionHeaderRef& node) {
		if(node.args) node.args->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeFunctionCall& node) {
		node.function->accept(*this);
		return &node;
	}
	virtual NodeAST* visit(NodeFunctionDefinition& node) {
		// if(node.visited) return &node;
		node.header ->accept(*this);
		if (node.return_variable)
			node.return_variable.value()->accept(*this);
        node.body->accept(*this);
		return &node;
	}
    virtual NodeAST* visit(NodeProgram& node) {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		visit_all(node.namespaces, *this);
		// visit_all(node.struct_definitions, *this);
		visit_all(node.callbacks, *this);
		visit_all(node.function_definitions, *this);
		// node.merge_function_definitions();
		node.reset_function_visited_flag();
		return &node;
	}
    virtual NodeAST* visit(NodeBlock& node) {
        for(const auto & stmt : node.statements) {
			if(!stmt) {
				auto error = CompileError(ErrorType::InternalError, "Null statement in block.", "", node.tok);
				error.exit();
			}
			stmt->accept(*this);
		}
		return &node;
    }
    virtual NodeAST* visit(NodeImport& node) {
		return &node;
    }
};



