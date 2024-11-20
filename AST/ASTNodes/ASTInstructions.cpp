//
// Created by Mathias Vatter on 08.06.24.
//

#include "ASTInstructions.h"
#include "ASTReferences.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/ASTLowering.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Desugaring/DesugarDelete.h"
#include "../../Lowering/LoweringFunctionCall.h"
#include "../../Desugaring/DesugaringFamily.h"
#include "../../Desugaring/DesugarFor.h"
#include "../../Desugaring/DesugarForEach.h"
#include "../../Desugaring/DesugarFunctionCall.h"
#include "../../Lowering/LoweringWhile.h"
#include "../../Lowering/LoweringMemAlloc.h"
#include "../../Lowering/LoweringNumElements.h"
#include "../../Desugaring/DesugarSingleAssignment.h"
#include "../../Lowering/PostLowering/PostLoweringNumElements.h"
#include "../../Lowering/LoweringUseCount.h"

// ************* NodeStatement ***************
NodeAST *NodeStatement::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeStatement::NodeStatement(const NodeStatement& other)
        : NodeInstruction(other), statement(clone_unique(other.statement)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeStatement::clone() const {
    return std::make_unique<NodeStatement>(*this);
}
NodeAST *NodeStatement::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (statement.get() == oldChild) {
        statement = std::move(newChild);
        return statement.get();
    }
	throw std::runtime_error("NodeStatement::replace_child: oldChild not found");
    return nullptr;
}

// ************* NodeFunctionCall ***************
NodeAST *NodeFunctionCall::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeFunctionCall::NodeFunctionCall(Token tok) : NodeInstruction(NodeType::FunctionCall, std::move(tok)) {}
NodeFunctionCall::NodeFunctionCall(bool is_call, std::unique_ptr<NodeFunctionHeaderRef> function, Token tok)
	: NodeInstruction(NodeType::FunctionCall, std::move(tok)), is_call(is_call), function(std::move(function)) {
	set_child_parents();
}

NodeFunctionCall::~NodeFunctionCall() {
	if(auto def = definition.lock()) {
		def->call_sites.erase(this);
	}
}

NodeFunctionCall::NodeFunctionCall(const NodeFunctionCall& other)
        : NodeInstruction(other), is_call(other.is_call), is_new(other.is_new), kind(other.kind),
          function(clone_unique(other.function)), definition(other.definition) {
    set_child_parents();
}

std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}
ASTLowering* NodeFunctionCall::get_lowering(struct NodeProgram *program) const {
    static LoweringFunctionCall lowering(program);
    return &lowering;
}

std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_definition(struct NodeProgram *program) {
    auto it = program->function_lookup.find({function->name, (int)function->args->size()});
    if(it != program->function_lookup.end()) {
		auto func_def = it->second.lock();
		func_def->is_used = true;
        definition = it->second;
        kind = Kind::UserDefined;
//		definition->call_sites.emplace(this);
        return func_def;
    }
    return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_builtin_definition(NodeProgram *program) {
    if(!program->def_provider) {
        CompileError(ErrorType::InternalError,"No definition provider found in program.", "", tok).exit();
    }
    if(auto builtin_func = program->def_provider->get_builtin_function(function.get())) {
        function->ty = builtin_func->ty;
        function->has_forced_parenth = builtin_func->header->has_forced_parenth;
        definition = builtin_func;
//		definition->is_thread_safe = builtin_func->is_thread_safe;
		kind = Kind::Builtin;
        return builtin_func;
    }
    return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_property_definition(NodeProgram *program) {
    if(!program->def_provider) {
        CompileError(ErrorType::InternalError,"No definition provider found in program.", "", tok).exit();
    }
    if(auto property_func = program->def_provider->get_property_function(function.get())) {
        if(function->args->size() < 2) {
            CompileError(
                    ErrorType::SyntaxError,
                    "Found Property Function with insufficient amount of arguments.",
                    tok.line, "At least 2 arguments",
                    std::to_string(function->args->size()),
                    tok.file
            ).exit();
        }
        function->ty = property_func->ty;
        definition = property_func;
        kind = Kind::Property;
        return property_func;
    }
    return nullptr;
}


std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_constructor_definition(NodeProgram *program) {
	auto it = program->struct_lookup.find(function->name);
	if(it != program->struct_lookup.end()) {
		auto constructor = it->second->constructor;
		if(!constructor) return nullptr;
		function->ty = constructor->header->ty;
		definition = constructor;
//		definition->call_sites.emplace(this);
		kind = Kind::Constructor;
		return it->second->constructor;
	}
	return nullptr;
}


bool NodeFunctionCall::bind_definition(NodeProgram* program, bool fail) {
    if (get_definition()) {
		// update call sites
//		if(kind == Kind::UserDefined) {
//			definition->call_sites.emplace(this);
//		}
        return true;
    }
    if (find_builtin_definition(program)) {
        return true;
    } else if (find_definition(program)) {
        return true;
    } else if (find_property_definition(program)) {
		return true;
//	} else if(find_method_definition(program)) {
//		return true;
	} else if(find_constructor_definition(program)) {
		return true;
    } else if(fail) {
        CompileError(ErrorType::SyntaxError,"Function has not been declared.", tok.line, "", function->name, tok.file).exit();
    }
    return false;
}

std::unique_ptr<NodeAccessChain> NodeFunctionCall::to_method_chain() {
	auto variable_ref = std::make_unique<NodeVariableRef>(function->name, function->tok);
	auto ptr_strings = variable_ref->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	auto func_call = clone_as<NodeFunctionCall>(this);
	func_call->function->name = ptr_strings.back();
	ptr_strings.pop_back();
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->add_method(std::move(func_call));
	method_chain->parent = this->parent;
	return method_chain;
}

void NodeFunctionCall::update_parents(NodeAST *new_parent) {
	parent = new_parent;
	function->update_parents(this);
}

void NodeFunctionCall::set_child_parents() {
	function->parent = this;
}

std::string NodeFunctionCall::get_string() {
	return function->get_string();
}

void NodeFunctionCall::update_token_data(const Token &token) {
	function -> update_token_data(token);
}

std::string NodeFunctionCall::get_object_name() const {
	size_t pos = function->name.find('.');
	if (pos != std::string::npos) {
		return function->name.substr(0, pos);
	}
	return "";
}

std::string NodeFunctionCall::get_method_name() const {
	size_t pos = function->name.rfind('.');
	if (pos != std::string::npos) {
		return function->name.substr(pos + 1);
	}
	return "";
}

ASTDesugaring * NodeFunctionCall::get_desugaring(NodeProgram *program) const {
	static DesugarFunctionCall desugaring(program);
	return &desugaring;
}

bool NodeFunctionCall::is_builtin_kind() const {
	static const std::unordered_set<NodeFunctionCall::Kind> builtin_kind = {NodeFunctionCall::Kind::Undefined, NodeFunctionCall::Kind::Builtin, NodeFunctionCall::Kind::Property};
	if(builtin_kind.find(kind) != builtin_kind.end()) return true;
	return false;
}

bool NodeFunctionCall::is_string_env() {
	bool is_string = false;
	// is within string environment
	is_string |= parent->ty == TypeRegistry::String;
	// is within message call
	is_string |= parent->parent->get_node_type() == NodeType::FunctionHeaderRef and static_cast<NodeFunctionHeaderRef*>(parent->parent)->name == "message";
	// is within return statement
	is_string |= parent->get_node_type() == NodeType::Return and static_cast<NodeReturn*>(parent)->get_definition()->ty == TypeRegistry::String;
	return is_string;
}

bool NodeFunctionCall::do_param_promotion() const {
	auto def = get_definition();
	if(!def) return false;
	if(def->is_thread_safe) return false;
	if(is_call) return false;
	if(def->call_sites.size() > 2) return false;
	return true;
}

// ************* NodeNumElements ***************
NodeAST *NodeNumElements::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNumElements::NodeNumElements(const NodeNumElements& other)
	: NodeInstruction(other), array(clone_unique(other.array)),
	  dimension(clone_unique(other.dimension)), size_array(other.size_array) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeNumElements::clone() const {
	return std::make_unique<NodeNumElements>(*this);
}
NodeAST *NodeNumElements::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (array.get() == oldChild) {
		if(auto new_ref = cast_node<NodeReference>(newChild.release())) {
			array = std::unique_ptr<NodeReference>(new_ref);
			return array.get();
		}
	} else if (dimension.get() == oldChild) {
		dimension = std::move(newChild);
		return dimension.get();
	}
	return nullptr;
}

ASTLowering* NodeNumElements::get_lowering(NodeProgram *program) const {
	static LoweringNumElements lowering(program);
	return &lowering;
}

ASTLowering* NodeNumElements::get_post_lowering(NodeProgram *program) const {
	static PostLoweringNumElements lowering(program);
	return &lowering;
}

// ************* NodeUseCount ***************
NodeAST *NodeUseCount::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeUseCount::NodeUseCount(const NodeUseCount& other)
	: NodeInstruction(other), ref(clone_unique(other.ref)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeUseCount::clone() const {
	return std::make_unique<NodeUseCount>(*this);
}
NodeAST *NodeUseCount::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ref.get() == oldChild) {
		if(auto new_ref = cast_node<NodeReference>(newChild.release())) {
			ref = std::unique_ptr<NodeReference>(new_ref);
			return ref.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeUseCount::get_lowering(NodeProgram *program) const {
	static LoweringUseCount lowering(program);
	return &lowering;
}

ASTLowering* NodeUseCount::get_post_lowering(NodeProgram *program) const {
	return nullptr;
}

// ************* NodeDelete ***************
NodeAST *NodeDelete::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeDelete::NodeDelete(const NodeDelete& other)
	: NodeInstruction(other), ptrs(clone_vector(other.ptrs)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeDelete::clone() const {
	return std::make_unique<NodeDelete>(*this);
}
NodeAST *NodeDelete::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for(auto &del : ptrs) {
		if (del.get() == oldChild) {
			if(auto new_del = cast_node<NodeReference>(newChild.release())) {
				del = std::unique_ptr<NodeReference>(new_del);
				return del.get();
			}
		}
	}
	return nullptr;
}

ASTDesugaring * NodeDelete::get_desugaring(NodeProgram *program) const {
	static DesugarDelete desugaring(program);
	return &desugaring;
}

// ************* NodeSingleDelete ***************
NodeAST *NodeSingleDelete::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeSingleDelete::NodeSingleDelete(const NodeSingleDelete& other)
	: NodeInstruction(other), ptr(clone_unique(other.ptr)), num(clone_unique(other.num)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleDelete::clone() const {
	return std::make_unique<NodeSingleDelete>(*this);
}

NodeAST *NodeSingleDelete::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ptr.get() == oldChild) {
		if(auto new_ptr = cast_node<NodeReference>(newChild.release())) {
			ptr = std::unique_ptr<NodeReference>(new_ptr);
			return ptr.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeSingleDelete::get_lowering(NodeProgram *program) const {
	static LoweringMemAlloc lowering(program);
	return &lowering;
}

// ************* NodeSingleRetain ***************
NodeAST *NodeSingleRetain::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeSingleRetain::NodeSingleRetain(const NodeSingleRetain& other)
	: NodeInstruction(other), ptr(clone_unique(other.ptr)), num(clone_unique(other.num)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleRetain::clone() const {
	return std::make_unique<NodeSingleRetain>(*this);
}

NodeAST *NodeSingleRetain::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ptr.get() == oldChild) {
		if(auto new_ptr = cast_node<NodeReference>(newChild.release())) {
			ptr = std::unique_ptr<NodeReference>(new_ptr);
			return ptr.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeSingleRetain::get_lowering(NodeProgram *program) const {
	static LoweringMemAlloc lowering(program);
	return &lowering;
}

// ************* NodeRetain ***************
NodeAST *NodeRetain::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeRetain::NodeRetain(const NodeRetain& other)
	: NodeInstruction(other), ptrs(clone_vector(other.ptrs)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeRetain::clone() const {
	return std::make_unique<NodeRetain>(*this);
}

// ************* NodeAssignment ***************
NodeAST *NodeAssignment::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeAssignment::NodeAssignment(const NodeAssignment& other)
        : NodeInstruction(other), l_values(clone_vector(other.l_values)),
          r_values(clone_unique(other.r_values)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeAssignment::clone() const {
    return std::make_unique<NodeAssignment>(*this);
}

ASTDesugaring * NodeAssignment::get_desugaring(NodeProgram *program) const {
    static DesugarDeclareAssign desugaring(program);
    return &desugaring;
}

// ************* NodeSingleAssignment ***************
NodeAST *NodeSingleAssignment::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeSingleAssignment::NodeSingleAssignment(const NodeSingleAssignment& other)
        : NodeInstruction(other), l_value(clone_unique(other.l_value)),
          r_value(clone_unique(other.r_value)), has_object(other.has_object) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleAssignment::clone() const {
    return std::make_unique<NodeSingleAssignment>(*this);
}
NodeAST *NodeSingleAssignment::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (l_value.get() == oldChild) {
		if(auto new_l_value = cast_node<NodeReference>(newChild.release())) {
			l_value = std::unique_ptr<NodeReference>(new_l_value);
			return l_value.get();
		}
    } else if (r_value.get() == oldChild) {
        r_value = std::move(newChild);
        return r_value.get();
    }
    return nullptr;
}

ASTDesugaring * NodeSingleAssignment::get_desugaring(NodeProgram *program) const {
	static DesugarSingleAssignment desugaring(program);
	return &desugaring;
}

//ASTLowering* NodeSingleAssignment::get_lowering(struct NodeProgram *program) const {
//	return this->l_value->get_lowering(program);
//}

//ASTLowering* NodeSingleAssignment::get_data_lowering(struct NodeProgram *program) const {
//    return this->l_value->get_data_lowering(program);
//}

// ************* NodeDeclaration ***************
NodeAST *NodeDeclaration::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeDeclaration::NodeDeclaration(const NodeDeclaration& other)
        : NodeInstruction(other), variable(clone_vector(other.variable)),
          value(clone_unique(other.value)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeDeclaration::clone() const {
    return std::make_unique<NodeDeclaration>(*this);
}

void NodeDeclaration::update_parents(NodeAST* new_parent)  {
    parent = new_parent;
    for(auto const &decl : variable) {
        decl->update_parents(this);
    }
    if(value) value->update_parents(this);
}

ASTDesugaring * NodeDeclaration::get_desugaring(NodeProgram *program) const {
    static DesugarDeclareAssign desugaring(program);
    return &desugaring;
}

// ************* NodeSingleDeclaration ***************
NodeAST *NodeSingleDeclaration::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeSingleDeclaration::NodeSingleDeclaration(const NodeSingleDeclaration& other)
        : NodeInstruction(other), variable(clone_shared(other.variable)),
          value(clone_unique(other.value)),
		  is_promoted(other.is_promoted), has_object(other.has_object) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleDeclaration::clone() const {
    return std::make_unique<NodeSingleDeclaration>(*this);
}
NodeAST *NodeSingleDeclaration::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (variable.get() == oldChild) {
        if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.release())) {
			variable = std::shared_ptr<NodeDataStructure>(new_data_structure);
            return variable.get();
        }
    } else if (value.get() == oldChild) {
        value = std::move(newChild);
        return value.get();
    }
    return nullptr;
}

ASTLowering* NodeSingleDeclaration::get_lowering(NodeProgram *program) const {
	if(variable->get_node_type() == NodeType::UIControl) {
		return variable->get_lowering(program);
	}
	if(variable->get_node_type() == NodeType::List) {
		return variable->get_lowering(program);
	}
//	static LoweringSingleDeclaration lowering(program);
//	return &lowering;
	return nullptr;
}

std::unique_ptr<NodeSingleAssignment> NodeSingleDeclaration::to_assign_stmt(NodeDataStructure* var) {
    // if var provided -> turn to reference else turn variable to reference
    auto node_array_var = var ? var->to_reference() : variable->to_reference();
	node_array_var->ty = var ? var->ty : variable->ty;
    // if declare stmt has r_value -> clone it else create new NodeInt with value 0
    auto node_assignee = value ? value->clone() : TypeRegistry::get_neutral_element_from_type(variable->ty);
    auto node_assign_statement = std::make_unique<NodeSingleAssignment>(
            std::move(node_array_var),
            std::move(node_assignee),
            tok
    );
	node_assign_statement->has_object = this->has_object;
    return node_assign_statement;
}

// ************* NodeFunctionParam ***************
NodeAST *NodeFunctionParam::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeFunctionParam::NodeFunctionParam(const NodeFunctionParam& other)
	: NodeInstruction(other), variable(clone_shared(other.variable)),
	  value(clone_unique(other.value)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeFunctionParam::clone() const {
	return std::make_unique<NodeFunctionParam>(*this);
}
NodeAST *NodeFunctionParam::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (variable.get() == oldChild) {
		if(auto new_data_structure = cast_node<NodeDataStructure>(newChild.release())) {
			variable = std::shared_ptr<NodeDataStructure>(new_data_structure);
			return variable.get();
		}
	} else if (value.get() == oldChild) {
		value = std::move(newChild);
		return value.get();
	}
	return nullptr;
}

ASTLowering* NodeFunctionParam::get_lowering(NodeProgram *program) const {
	return nullptr;
}

// ************* NodeReturn ***************
NodeAST *NodeReturn::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeReturn::NodeReturn(const NodeReturn& other)
        : NodeInstruction(other), return_variables(clone_vector(other.return_variables)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeReturn::clone() const {
    return std::make_unique<NodeReturn>(*this);
}
NodeAST *NodeReturn::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    for(auto &ret : return_variables) {
        if (ret.get() == oldChild) {
            ret = std::move(newChild);
            return ret.get();
        }
    }
    return nullptr;
}

// ************* NodeSingleReturn ***************
NodeAST *NodeSingleReturn::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeSingleReturn::NodeSingleReturn(const NodeSingleReturn& other)
	: NodeInstruction(other), return_variable(clone_unique(other.return_variable)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSingleReturn::clone() const {
	return std::make_unique<NodeSingleReturn>(*this);
}
NodeAST *NodeSingleReturn::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if(return_variable.get() == oldChild) {
		return_variable = std::move(newChild);
		return return_variable.get();
	}
	return nullptr;
}

// ************* NodeBlock ***************
NodeAST *NodeBlock::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeBlock::NodeBlock(const NodeBlock& other) : NodeInstruction(other), scope(other.scope) {
    statements = clone_vector(other.statements);
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeBlock::clone() const {
    return std::make_unique<NodeBlock>(*this);
}

void NodeBlock::append_body(std::unique_ptr<NodeBlock> new_body) {
    if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
    statements.insert(statements.end(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

void NodeBlock::prepend_body(std::unique_ptr<NodeBlock> new_body) {
    if(!new_body) return;
    for(auto &stmt : new_body->statements) {
        stmt->parent = this;
    }
    statements.insert(statements.begin(), std::make_move_iterator(new_body->statements.begin()), std::make_move_iterator(new_body->statements.end()));
}

NodeStatement* NodeBlock::add_stmt(std::unique_ptr<NodeStatement> stmt) {
    stmt->parent = this;
    statements.push_back(std::move(stmt));
	return statements.back().get();
}

//void NodeBlock::flatten() {
//    std::vector<std::unique_ptr<NodeStatement>> temp;
//    for(auto & statement : statements) {
//        if(statement->statement->get_node_type() == NodeType::Block) {
//            // Übertragen Sie die function_inlines vom aktuellen NodeBlock-Element
//            // auf das erste Element der inneren NodeBlock
//			auto node_innner_body = static_cast<NodeBlock*>(statement->statement.get());
////			if(node_innner_body->scope) continue;
//            auto& inner_statements = node_innner_body->statements;
//            if (!inner_statements.empty()) {
//                inner_statements[0]->function_inlines.insert(
//                        inner_statements[0]->function_inlines.end(),
//                        std::make_move_iterator(statement->function_inlines.begin()),
//                        std::make_move_iterator(statement->function_inlines.end())
//                );
//            }
//            // Aktualisieren Sie das parent-Attribut für jedes innere Statement
//            for (int i=0; i<inner_statements.size(); i++) {
//				auto& stmt = inner_statements[i];
//				if(stmt->statement->get_node_type() == NodeType::DeadCode) {
//					inner_statements.erase(inner_statements.begin() + i);
//					i--;
//					continue;
//				}
//                stmt->parent = this;
//            }
//            // Fügen Sie die inneren Statements zum temporären Vector hinzu
//            temp.insert(
//                    temp.end(),
//                    std::make_move_iterator(inner_statements.begin()),
//                    std::make_move_iterator(inner_statements.end())
//            );
//            // Überspringen Sie das Hinzufügen des aktuellen NodeBlock-Elements zu `temp`
//            continue;
//        }
//        // Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht speziell behandelt wird
//        temp.push_back(std::move(statement));
//    }
//    // Ersetzen Sie die alte Liste durch die neue
//    statements = std::move(temp);
//}

void NodeBlock::flatten(bool force) {
	// Reserviere genug Platz für den schlimmsten Fall, in dem keine Flattening nötig ist
	std::vector<std::unique_ptr<NodeStatement>> new_statements;
	new_statements.reserve(statements.size());

	for (auto& statement : statements) {
		if (statement->statement->get_node_type() == NodeType::Block) {
			auto node_innner_body = static_cast<NodeBlock*>(statement->statement.get());
//			node_innner_body->flatten();
			auto& inner_statements = node_innner_body->statements;

			// Entfernen Sie DeadCode-Nodes in einem Schritt
			auto new_end = std::remove_if(inner_statements.begin(), inner_statements.end(),
										  [](const std::unique_ptr<NodeStatement>& stmt) {
											return stmt->statement->get_node_type() == NodeType::DeadCode;
										  });
			inner_statements.erase(new_end, inner_statements.end());

			if(node_innner_body->scope and !force) {
				new_statements.push_back(std::move(statement));
				continue;
			}
			// Aktualisieren Sie das parent-Attribut und fügen Sie sie zu new_statements hinzu
			for (auto& stmt : inner_statements) {
				stmt->parent = this;
				new_statements.push_back(std::move(stmt));
			}

			// Continue, um das Hinzufügen des aktuellen NodeBlock-Elements zu überspringen
			continue;
		}

		// Fügen Sie das aktuelle Element hinzu, wenn es nicht speziell behandelt wird
		new_statements.push_back(std::move(statement));
	}

	// Ersetzen Sie die alte Liste durch die neue
	statements = std::move(new_statements);
}


void NodeBlock::wrap_in_loop_nest(std::vector<std::shared_ptr<NodeDataStructure>> iterators,
								  std::vector<std::unique_ptr<NodeAST>> lower_bounds,
								  std::vector<std::unique_ptr<NodeAST>> upper_bounds) {
	std::unique_ptr<NodeBlock> inner_body = std::make_unique<NodeBlock>(std::move(statements), tok);
	inner_body->scope = true;
	for (int i = (int)iterators.size()-1; i>=0 ; i--) {
		auto node_for = std::make_unique<NodeFor>(
			std::make_unique<NodeSingleAssignment>(iterators[i]->to_reference(), std::move(lower_bounds[i]), Token()),
			token::TO,
			std::make_unique<NodeBinaryExpr>(token::SUB, std::move(upper_bounds[i]), std::make_unique<NodeInt>(1, Token()), Token()),
			std::move(inner_body),
			Token()
		);
		inner_body = std::make_unique<NodeBlock>(Token(), true);
		inner_body->add_as_stmt(std::move(node_for));
	}
	for(auto & iterator : iterators) {
		iterator->is_local = true;
		auto node_decl = std::make_unique<NodeSingleDeclaration>(iterator, nullptr, Token());
		inner_body->prepend_as_stmt(std::move(node_decl));
	}
	statements = std::move(inner_body->statements);
}

void NodeBlock::wrap_in_loop(std::shared_ptr<NodeDataStructure> iterator, std::unique_ptr<NodeAST> lower_bound, std::unique_ptr<NodeAST> upper_bound) {
	std::unique_ptr<NodeBlock> inner_body = std::make_unique<NodeBlock>(std::move(statements), tok);
	inner_body->scope = true;
	auto node_for = std::make_unique<NodeFor>(
		std::make_unique<NodeSingleAssignment>(iterator->to_reference(), std::move(lower_bound), Token()),
		token::TO,
		std::make_unique<NodeBinaryExpr>(token::SUB, std::move(upper_bound), std::make_unique<NodeInt>(1, Token()), Token()),
		std::move(inner_body),
		Token()
	);
	inner_body = std::make_unique<NodeBlock>(Token(), true);
	inner_body->add_as_stmt(std::move(node_for));
	iterator->is_local = true;
	auto node_decl = std::make_unique<NodeSingleDeclaration>(iterator, nullptr, Token());
	inner_body->prepend_as_stmt(std::move(node_decl));
	statements = std::move(inner_body->statements);
}

// ************* NodeFamily ***************
NodeAST *NodeFamily::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeFamily::NodeFamily(const NodeFamily& other)
        : NodeInstruction(other), prefix(other.prefix), members(clone_unique(other.members)) {
    set_child_parents();
}

std::unique_ptr<NodeAST> NodeFamily::clone() const {
    return std::make_unique<NodeFamily>(*this);
}

ASTDesugaring * NodeFamily::get_desugaring(NodeProgram *program) const {
    static DesugaringFamily desugaring(program);
    return &desugaring;
}

// ************* NodeIf ***************
NodeAST *NodeIf::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeIf::NodeIf(const NodeIf& other)
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          if_body(clone_unique(other.if_body)),
          else_body(clone_unique(other.else_body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeIf::clone() const {
    return std::make_unique<NodeIf>(*this);
}
NodeAST *NodeIf::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
}

// ************* NodeFor ***************
NodeAST *NodeFor::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeFor::NodeFor(const NodeFor& other)
        : NodeInstruction(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
          step(clone_unique(other.step)), body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeFor::clone() const {
    return std::make_unique<NodeFor>(*this);
}
NodeAST *NodeFor::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (iterator_end.get() == oldChild) {
        iterator_end = std::move(newChild);
        return iterator_end.get();
    }
    return nullptr;
}

ASTDesugaring * NodeFor::get_desugaring(NodeProgram *program) const {
    static DesugarFor desugaring(program);
    return &desugaring;
}

// ************* NodeForEach ***************
NodeAST *NodeForEach::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeForEach::NodeForEach(const NodeForEach& other)
        : NodeInstruction(other), keys(clone_vector(other.keys)), range(clone_unique(other.range)),
          body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeForEach::clone() const {
    return std::make_unique<NodeForEach>(*this);
}
NodeAST *NodeForEach::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (range.get() == oldChild) {
        range = std::move(newChild);
        return range.get();
    }
    return nullptr;
}

ASTDesugaring * NodeForEach::get_desugaring(NodeProgram *program) const {
    static DesugarForEach desugaring(program);
    return &desugaring;
}

// ************* NodeWhile ***************
NodeAST *NodeWhile::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeWhile::NodeWhile(const NodeWhile& other)
        : NodeInstruction(other), condition(clone_unique(other.condition)),
          body(clone_unique(other.body)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeWhile::clone() const {
    return std::make_unique<NodeWhile>(*this);
}
NodeAST *NodeWhile::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
}
ASTLowering* NodeWhile::get_lowering(struct NodeProgram *program) const {
	static LoweringWhile lowering(program);
	return &lowering;
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

static std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> clone_cases(
        const std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>>& original) {
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cloned_cases;
    cloned_cases.reserve(original.size());
    for (auto& pair : original) {
        cloned_cases.emplace_back(clone_vector(pair.first), clone_unique(pair.second));
    }
    return cloned_cases;
}

// ************* NodeSelect ***************
NodeAST *NodeSelect::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeSelect::NodeSelect(const NodeSelect& other)
        : NodeInstruction(other), expression(clone_unique(other.expression)),
          cases(clone_cases(other.cases)) {
    set_child_parents();
}
std::unique_ptr<NodeAST> NodeSelect::clone() const {
    return std::make_unique<NodeSelect>(*this);
}
NodeAST *NodeSelect::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (expression.get() == oldChild) {
        expression = std::move(newChild);
        return expression.get();
    } else {
        for ( auto & cas : cases) {
            if(cas.first[0].get() == oldChild) {
                cas.first[0] = std::move(newChild);
                return cas.first[0].get();
            }
        }
    }
    return nullptr;
}

// ************* NodeBreak ***************
NodeAST *NodeBreak::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeBreak::NodeBreak(const NodeBreak& other)
	: NodeInstruction(other) {}

std::unique_ptr<NodeAST> NodeBreak::clone() const {
	return std::make_unique<NodeBreak>(*this);
}