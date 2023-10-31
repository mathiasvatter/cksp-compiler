//
// Created by Mathias Vatter on 28.08.23.
//


#include "AST.h"
#include "ASTVisitor.h"

void NodeAST::accept(ASTVisitor &visitor) {
}

void NodeAST::replace_with(std::unique_ptr<NodeAST> newNode) {
	if (parent) {
		newNode->parent = parent;
		parent->replace_child(this, std::move(newNode));
	}
}

void NodeInt::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeReal::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeString::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeVariable::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeArray::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeBinaryExpr::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeBinaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (left.get() == oldChild) {
		left = std::move(newChild);
	} else if (right.get() == oldChild) {
		right = std::move(newChild);
	}
}

void NodeUnaryExpr::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeUnaryExpr::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (operand.get() == oldChild) {
        operand = std::move(newChild);
    }
}

void NodeAssignStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (statement.get() == oldChild) {
		statement = std::move(newChild);
	}
}

void NodeCallback::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeProgram::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void NodeFunctionHeader::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeFunctionDefinition::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeParamList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeParamList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& param : params) {
		if (param.get() == oldChild) {
			param = std::move(newChild);
			return;
		}
	}
}

void NodeDeclareStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}


//void NodeDefineStatement::accept(ASTVisitor &visitor) {
//	visitor.visit(*this);
//}

void NodeIfStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeForStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeForStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
		iterator_end = std::move(newChild);
	}
}

void NodeWhileStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeFunctionCall::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeSelectStatement::accept(ASTVisitor &visitor) {
	visitor.visit(*this);
}

void NodeConstStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeConstStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
//    for (auto& c : constants) {
//        if (c.get() == oldChild) {
//            c = std::move(newChild);
//            return;
//        }
//    }
//}

void NodeStructStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeStructStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
//    for (auto& m : members) {
//        if (m.get() == oldChild) {
//            m = std::move(newChild);
//            return;
//        }
//    }
//}

void NodeFamilyStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeFamilyStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
//    for (auto& m : members) {
//        if (m.get() == oldChild) {
//            m = std::move(newChild);
//            return;
//        }
//    }
//}

void NodeUIControl::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeMacroDefinition::accept(ASTVisitor &visitor) {
//	visitor.visit(*this);
//}
//
//void NodeMacroHeader::accept(ASTVisitor &visitor) {
//    visitor.visit(*this);
//}

void NodeGetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeSetControlStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeStatementList::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

//void NodeStatementList::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
//    for (auto& stmt : statements) {
//        if (stmt.get() == oldChild) {
//            stmt = std::move(newChild);
//            return;
//        }
//    }
//}

void NodeSingleAssignStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}

void NodeSingleDeclareStatement::accept(ASTVisitor &visitor) {
    visitor.visit(*this);
}
