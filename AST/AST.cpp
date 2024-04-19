//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTVisitor.h"
#include "ASTHandler.h"

// ************* NodeAST Base Class ***************
void NodeAST::accept(ASTVisitor &visitor) {
}
void NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		parent->replace_child(this, std::move(newNode));
	}
}

// ************* DataStructure ***************
void DataStructure::accept(ASTVisitor &visitor) {
}
DataStructure::DataStructure(const DataStructure& other)
        : NodeAST(other),
          is_engine(other.is_engine), is_used(other.is_used), persistence(other.persistence),
          is_local(other.is_local), is_global(other.is_global), is_compiler_return(other.is_compiler_return),
          data_type(other.data_type), name(other.name), declaration(other.declaration) {}
std::unique_ptr<NodeAST> DataStructure::clone() const {
    return std::make_unique<DataStructure>(*this);
}

// ************* NodeDeadCode ***************
void NodeDeadCode::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeDeadCode::clone() const {
    return std::make_unique<NodeDeadCode>(*this);
}

// ************* NodeInt ***************
void NodeInt::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeInt::clone() const {
    return std::make_unique<NodeInt>(*this);
}

// ************* NodeReal ***************
void NodeReal::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeReal::clone() const {
    return std::make_unique<NodeReal>(*this);
}

// ************* NodeString ***************
void NodeString::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeString::clone() const {
    return std::make_unique<NodeString>(*this);
}

// ************* NodeVariable ***************
void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeVariable::NodeVariable(const NodeVariable& other)
        : DataStructure(other) {}
std::unique_ptr<NodeAST> NodeVariable::clone() const {
    return std::make_unique<NodeVariable>(*this);
}

// ************* NodeParamList ***************
void NodeParamList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeParamList::NodeParamList(const NodeParamList& other) : NodeAST(other) {
    params = clone_vector(other.params);
}
std::unique_ptr<NodeAST> NodeParamList::clone() const {
    return std::make_unique<NodeParamList>(*this);
}
void NodeParamList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for (auto& param : params) {
        if (param.get() == oldChild) {
            param = std::move(newChild);
            return;
        }
    }
}

// ************* NodeArray ***************
void NodeArray::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeArray::NodeArray(const NodeArray& other)
        : DataStructure(other), show_brackets(other.show_brackets), sizes(clone_unique(other.sizes)),
          indexes(clone_unique(other.indexes)), dimensions(other.dimensions) {}
std::unique_ptr<NodeAST> NodeArray::clone() const {
    return std::make_unique<NodeArray>(*this);
}

ASTHandler *NodeArray::get_handler() const {
    static ArrayHandler handler;
    return &handler;
}

// ************* NodeNDArray ***************
void NodeNDArray::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeNDArray::NodeNDArray(const NodeNDArray& other)
	: NodeAST(other), is_engine(other.is_engine), is_used(other.is_used), persistence(other.persistence),
	  is_local(other.is_local), is_global(other.is_global), is_compiler_return(other.is_compiler_return),
	  show_brackets(other.show_brackets), var_type(other.var_type), name(other.name),
	  sizes(clone_unique(other.sizes)), indexes(clone_unique(other.indexes)),
	  declaration(other.declaration), dimensions(other.dimensions) {}
std::unique_ptr<NodeAST> NodeNDArray::clone() const {
	return std::make_unique<NodeNDArray>(*this);
}

// ************* NodeUIControl ***************
void NodeUIControl::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeUIControl::NodeUIControl(const NodeUIControl& other)
        : DataStructure(other), ui_control_type(other.ui_control_type),
          control_var(clone_unique(other.control_var)), params(clone_unique(other.params)),
          sizes(clone_unique(other.sizes)), arg_ast_types(other.arg_ast_types), arg_var_types(other.arg_var_types) {}
std::unique_ptr<NodeAST> NodeUIControl::clone() const {
    return std::make_unique<NodeUIControl>(*this);
}

// ************* NodeUnaryExpr ***************
void NodeUnaryExpr::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeUnaryExpr::NodeUnaryExpr(const NodeUnaryExpr& other)
        : NodeAST(other), op(other.op), operand(clone_unique(other.operand)) {}
std::unique_ptr<NodeAST> NodeUnaryExpr::clone() const {
    return std::make_unique<NodeUnaryExpr>(*this);
}
void NodeUnaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
    }
}

// ************* NodeBinaryExpr ***************
void NodeBinaryExpr::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeBinaryExpr::NodeBinaryExpr(const NodeBinaryExpr& other)
        : NodeAST(other), op(other.op), has_forced_parenth(other.has_forced_parenth),
          left(clone_unique(other.left)), right(clone_unique(other.right)) {}
std::unique_ptr<NodeAST> NodeBinaryExpr::clone() const {
    return std::make_unique<NodeBinaryExpr>(*this);
}
void NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (left.get() == oldChild) {
        left = std::move(newChild);
    } else if (right.get() == oldChild) {
        right = std::move(newChild);
    }
}

// ************* NodeAssignStatement ***************
void NodeAssignStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeAssignStatement::NodeAssignStatement(const NodeAssignStatement& other)
        : NodeAST(other),
          array_variable(clone_unique(other.array_variable)), assignee(clone_unique(other.assignee)) {}
std::unique_ptr<NodeAST> NodeAssignStatement::clone() const {
    return std::make_unique<NodeAssignStatement>(*this);
}

// ************* NodeSingleAssignStatement ***************
void NodeSingleAssignStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleAssignStatement::NodeSingleAssignStatement(const NodeSingleAssignStatement& other)
        : NodeAST(other),
          array_variable(clone_unique(other.array_variable)), assignee(clone_unique(other.assignee)) {}
std::unique_ptr<NodeAST> NodeSingleAssignStatement::clone() const {
    return std::make_unique<NodeSingleAssignStatement>(*this);
}
void NodeSingleAssignStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (array_variable.get() == oldChild) {
        array_variable = std::move(newChild);
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
    }
}

// ************* NodeDeclareStatement ***************
void NodeDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeDeclareStatement::NodeDeclareStatement(const NodeDeclareStatement& other)
        : NodeAST(other),
          to_be_declared(clone_vector(other.to_be_declared)), assignee(clone_unique(other.assignee)) {}
std::unique_ptr<NodeAST> NodeDeclareStatement::clone() const {
    return std::make_unique<NodeDeclareStatement>(*this);
}

// ************* NodeSingleDeclareStatement ***************
void NodeSingleDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSingleDeclareStatement::NodeSingleDeclareStatement(const NodeSingleDeclareStatement& other)
        : NodeAST(other),
          to_be_declared(clone_unique(other.to_be_declared)), assignee(clone_unique(other.assignee)) {}
std::unique_ptr<NodeAST> NodeSingleDeclareStatement::clone() const {
    return std::make_unique<NodeSingleDeclareStatement>(*this);
}
void NodeSingleDeclareStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (to_be_declared.get() == oldChild) {
        to_be_declared = std::move(newChild);
    } else if (assignee.get() == oldChild) {
        assignee = std::move(newChild);
    }
}

ASTHandler *NodeSingleDeclareStatement::get_handler() const {
    static SingleDeclareStatementHandler handler;
    return &handler;
}

// ************* NodeReturnStatement ***************
void NodeReturnStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeReturnStatement::NodeReturnStatement(const NodeReturnStatement& other)
	: NodeAST(other), return_variables(clone_vector(other.return_variables)) {}
std::unique_ptr<NodeAST> NodeReturnStatement::clone() const {
	return std::make_unique<NodeReturnStatement>(*this);
}
void NodeReturnStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for(auto &ret : return_variables) {
		if (ret.get() == oldChild) {
			ret = std::move(newChild);
		}
	}
}

// ************* NodeGetControlStatement ***************
void NodeGetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeGetControlStatement::NodeGetControlStatement(const NodeGetControlStatement& other)
        : NodeAST(other), ui_id(clone_unique(other.ui_id)), control_param(other.control_param) {}
std::unique_ptr<NodeAST> NodeGetControlStatement::clone() const {
    return std::make_unique<NodeGetControlStatement>(*this);
}
void NodeGetControlStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ui_id.get() == oldChild) {
		ui_id = std::move(newChild);
	}
}

// ************* NodeSetControlStatement ***************
void NodeSetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSetControlStatement::NodeSetControlStatement(const NodeSetControlStatement& other)
        : NodeAST(other), get_control(clone_unique(other.get_control)), assignee(clone_unique(other.assignee)) {}
std::unique_ptr<NodeAST> NodeSetControlStatement::clone() const {
    return std::make_unique<NodeSetControlStatement>(*this);
}

// ************* NodeStatement ***************
void NodeStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeStatement::NodeStatement(const NodeStatement& other)
        : NodeAST(other), statement(clone_unique(other.statement)), function_inlines(other.function_inlines){}
std::unique_ptr<NodeAST> NodeStatement::clone() const {
    return std::make_unique<NodeStatement>(*this);
}
void NodeStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (statement.get() == oldChild) {
		statement = std::move(newChild);
	}
}

// ************* NodeConstStatement ***************
void NodeConstStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeConstStatement::NodeConstStatement(const NodeConstStatement& other)
        : NodeAST(other), prefix(other.prefix), constants(clone_vector<NodeStatement>(other.constants)) {}
std::unique_ptr<NodeAST> NodeConstStatement::clone() const {
    return std::make_unique<NodeConstStatement>(*this);
}

// ************* NodeStructStatement ***************
void NodeStructStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStructStatement::NodeStructStatement(const NodeStructStatement& other)
        : NodeAST(other), prefix(other.prefix), members(clone_vector<NodeStatement>(other.members)) {}
std::unique_ptr<NodeAST> NodeStructStatement::clone() const {
    return std::make_unique<NodeStructStatement>(*this);
}

// ************* NodeFamilyStatement ***************
void NodeFamilyStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFamilyStatement::NodeFamilyStatement(const NodeFamilyStatement& other)
        : NodeAST(other), prefix(other.prefix), members(clone_vector<NodeStatement>(other.members)) {}
std::unique_ptr<NodeAST> NodeFamilyStatement::clone() const {
    return std::make_unique<NodeFamilyStatement>(*this);
}

// ************* NodeListStatement ***************
void NodeListStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeListStatement::NodeListStatement(const NodeListStatement& other)
        : NodeAST(other), name(other.name), size(other.size), body(clone_vector(other.body)) {}
std::unique_ptr<NodeAST> NodeListStatement::clone() const {
    return std::make_unique<NodeListStatement>(*this);
}

// ************* NodeStatementList ***************
void NodeStatementList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeStatementList::NodeStatementList(const NodeStatementList& other) : NodeAST(other), scope(other.scope) {
    statements = clone_vector(other.statements);
}
std::unique_ptr<NodeAST> NodeStatementList::clone() const {
    return std::make_unique<NodeStatementList>(*this);
}

// ************* NodeIfStatement ***************
void NodeIfStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeIfStatement::NodeIfStatement(const NodeIfStatement& other)
        : NodeAST(other), condition(clone_unique(other.condition)), statements(clone_unique(other.statements)), else_statements(clone_unique(other.else_statements)) {}
std::unique_ptr<NodeAST> NodeIfStatement::clone() const {
    return std::make_unique<NodeIfStatement>(*this);
}
void NodeIfStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (condition.get() == oldChild) {
		condition = std::move(newChild);
	}
}

// ************* NodeForStatement ***************
void NodeForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeForStatement::NodeForStatement(const NodeForStatement& other)
        : NodeAST(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
        step(clone_unique(other.step)), statements(clone_unique(other.statements)) {}
std::unique_ptr<NodeAST> NodeForStatement::clone() const {
    return std::make_unique<NodeForStatement>(*this);
}
void NodeForStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
        iterator_end = std::move(newChild);
    }
}

// ************* NodeRangedForStatement ***************
void NodeRangedForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeRangedForStatement::NodeRangedForStatement(const NodeRangedForStatement& other)
        : NodeAST(other), keys(clone_unique(other.keys)), range(clone_unique(other.range)),
        statements(clone_unique(other.statements)) {}
std::unique_ptr<NodeAST> NodeRangedForStatement::clone() const {
    return std::make_unique<NodeRangedForStatement>(*this);
}
void NodeRangedForStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (range.get() == oldChild) {
        range = std::move(newChild);
    }
}

// ************* NodeWhileStatement ***************
void NodeWhileStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeWhileStatement::NodeWhileStatement(const NodeWhileStatement& other)
        : NodeAST(other), condition(clone_unique(other.condition)), statements(clone_unique(other.statements)) {}
std::unique_ptr<NodeAST> NodeWhileStatement::clone() const {
    return std::make_unique<NodeWhileStatement>(*this);
}
void NodeWhileStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (condition.get() == oldChild) {
		condition = std::move(newChild);
	}
}

// ************* Helper to clone map with unique_ptr keys and vector of unique_ptr values ***************
static std::map< std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> clone_map(
        const std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>>& original) {
    std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> cloned_map;
    for (const auto& pair : original) {
        cloned_map[clone_vector(pair.first)] = clone_vector(pair.second);
    }
    return cloned_map;
}

static std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeStatementList>>> clone_cases(
        const std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeStatementList>>>& original) {
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeStatementList>>> cloned_cases;
    cloned_cases.reserve(original.size());
    for (auto& pair : original) {
        cloned_cases.emplace_back(clone_vector(pair.first), clone_unique(pair.second));
    }
    return cloned_cases;
}

// ************* NodeSelectStatement ***************
void NodeSelectStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeSelectStatement::NodeSelectStatement(const NodeSelectStatement& other)
        : NodeAST(other), expression(clone_unique(other.expression)), cases(clone_cases(other.cases)) {}
std::unique_ptr<NodeAST> NodeSelectStatement::clone() const {
    return std::make_unique<NodeSelectStatement>(*this);
}
void NodeSelectStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (expression.get() == oldChild) {
		expression = std::move(newChild);
	} else {
        for ( auto & cas : cases) {
            if(cas.first[0].get() == oldChild) {
                cas.first[0] = std::move(newChild);
            }
        }
    }
}

// ************* NodeCallback ***************
void NodeCallback::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}
NodeCallback::NodeCallback(const NodeCallback& other)
        : NodeAST(other), begin_callback(other.begin_callback), callback_id(clone_unique(other.callback_id)), statements(clone_unique(other.statements)), end_callback(other.end_callback) {}
std::unique_ptr<NodeAST>  NodeCallback::clone() const {
    return std::make_unique<NodeCallback>(*this);
}
void NodeCallback::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (callback_id.get() == oldChild) {
        callback_id = std::move(newChild);
    }
}

// ************* NodeImport ***************
void NodeImport::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}
NodeImport::NodeImport(const NodeImport& other) : NodeAST(other), filepath(other.filepath), alias(other.alias) {}
std::unique_ptr<NodeAST> NodeImport::clone() const {
    return std::make_unique<NodeImport>(*this);
}

// ************* NodeFunctionHeader ***************
void NodeFunctionHeader::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionHeader::NodeFunctionHeader(const NodeFunctionHeader& other)
        : NodeAST(other), name(other.name), is_engine(other.is_engine),
        has_forced_parenth(other.has_forced_parenth), args(clone_unique(other.args)),
        arg_ast_types(other.arg_ast_types), arg_var_types(other.arg_var_types) {
}
std::unique_ptr<NodeAST> NodeFunctionHeader::clone() const {
    return std::make_unique<NodeFunctionHeader>(*this);
}

// ************* NodeFunctionCall ***************
void NodeFunctionCall::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionCall::NodeFunctionCall(const NodeFunctionCall& other)
        : NodeAST(other), is_call(other.is_call), function(clone_unique(other.function)) {}
std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}

// ************* NodeFunctionDefinition ***************
void NodeFunctionDefinition::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
NodeFunctionDefinition::NodeFunctionDefinition(const NodeFunctionDefinition& other)
        : NodeAST(other), is_used(other.is_used), is_compiled(other.is_compiled),
        header(clone_unique(other.header)), override(other.override),
          call(other.call), body(clone_unique(other.body)) {
    if (other.return_variable) {
        return_variable = std::make_optional(clone_unique(other.return_variable.value()));
    }
}
std::unique_ptr<NodeAST> NodeFunctionDefinition::clone() const {
    return std::make_unique<NodeFunctionDefinition>(*this);
}

// ************* NodeProgramm ***************
void NodeProgram::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}
NodeProgram::NodeProgram(const NodeProgram& other) : NodeAST(other) {
    callbacks = clone_vector<NodeCallback>(other.callbacks);
    function_definitions = clone_vector<NodeFunctionDefinition>(other.function_definitions);
}
std::unique_ptr<NodeAST> NodeProgram::clone() const {
    return std::make_unique<NodeProgram>(*this);
}











