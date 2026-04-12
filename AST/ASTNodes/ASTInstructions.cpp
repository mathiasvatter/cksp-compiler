//
// Created by Mathias Vatter on 08.06.24.
//

#include "ASTInstructions.h"
#include "ASTReferences.h"
#include "../../Desugaring/DesugarCompoundAssignment.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/ASTLowering.h"
#include "../../Desugaring/DesugarDeclareAssign.h"
#include "../../Desugaring/DesugarDelete.h"
#include "../../Lowering/LoweringFunctionCall.h"
#include "../../Desugaring/DesugarFamily.h"
#include "../../Lowering/LoweringFor.h"
#include "../../Lowering/LoweringForEach.h"
#include "../../Desugaring/DesugarFunctionCall.h"
#include "../../Lowering/LoweringWhile.h"
#include "../../Lowering/LoweringMemAlloc.h"
#include "../../Lowering/LoweringNumElements.h"
#include "../../Desugaring/DesugarSingleAssignment.h"
#include "../../Desugaring/DesugarUIControlArray.h"
#include "../../Desugaring/DesugarNamespace.h"
#include "../../Lowering/LoweringRange.h"
#include "../../Lowering/PostLowering/PostLoweringNumElements.h"
#include "../../Lowering/LoweringUseCount.h"
#include "../ASTVisitor/ReturnFunctionRewriting/ReturnFunctionCallHoisting.h"
#include "../ASTVisitor/FunctionHandling/FunctionInlining.h"
#include "../../Lowering/LoweringSortSearch.h"
#include "../../Lowering/PostLowering/PostLoweringSortSearch.h"
#include "../../misc/CommandLineOptions.h"
#include "../../Optimization/FreeVarCollector.h"
#include "../ASTVisitor/FunctionHandling/ASTFunctionStrategy.h"
#include "../ASTVisitor/FunctionHandling/BuiltinRestrictionValidator.h"
#include "../ASTVisitor/FunctionHandling/FunctionShortCircuit.h"
#include "../ASTVisitor/GlobalScope/NormalizeArrayAssign.h"

// ************* NodeStatement ***************
NodeAST *NodeStatement::accept(ASTVisitor &visitor) {
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
NodeAST *NodeFunctionCall::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeFunctionCall::NodeFunctionCall(Token tok) : NodeInstruction(NodeType::FunctionCall, std::move(tok)) {}
NodeFunctionCall::NodeFunctionCall(bool is_call, std::unique_ptr<NodeFunctionHeaderRef> function, Token tok)
	: NodeInstruction(NodeType::FunctionCall, std::move(tok)), is_call(is_call), function(std::move(function)) {
	set_child_parents();
}

NodeFunctionCall::~NodeFunctionCall() {
	if(const auto def = definition.lock()) {
		def->call_sites.erase(this);
	}
}

NodeFunctionCall::NodeFunctionCall(const NodeFunctionCall& other)
        : NodeInstruction(other), kind(other.kind), strategy(other.strategy), is_call(other.is_call), is_callable(other.is_callable), is_new(other.is_new),
          is_temporary_constructor(other.is_temporary_constructor), function(clone_unique(other.function)),
		  definition(other.definition) {
    NodeFunctionCall::set_child_parents();
}

std::unique_ptr<NodeAST> NodeFunctionCall::clone() const {
    return std::make_unique<NodeFunctionCall>(*this);
}
ASTLowering* NodeFunctionCall::get_lowering(struct NodeProgram *program) const {
    static LoweringFunctionCall lowering(program);
    return &lowering;
}

std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_definition(NodeProgram *program) {
	if (auto func_def = program->look_up_function(*this->function)) {
		definition = func_def;
		kind = Kind::UserDefined;
		return func_def;
	}
    return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> NodeFunctionCall::find_definition(NodeProgram *program,
	const std::string &name,
	int num_args,
	const Type *ty) {
	if (auto func_def = program->look_up_compatible({name, num_args}, ty)) {
		definition = func_def;
		kind = Kind::UserDefined;
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
	const auto it = program->struct_lookup.find(function->name);
	if(it != program->struct_lookup.end()) {
		const auto constructor = it->second->constructor;
		if(!constructor) return nullptr;
		function->ty = constructor->header->ty;
		definition = constructor;
		function->name = constructor->header->name;
//		constructor->call_sites.emplace(this);
		kind = Kind::Constructor;
		return it->second->constructor;
	}
	return nullptr;
}


bool NodeFunctionCall::bind_definition(NodeProgram* program, const bool fail, const bool force) {
    if (get_definition() and !force) {
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
    	// check for function header reference
    	if (this->function->get_declaration()) {
    		return true;
    	}
        CompileError(ErrorType::SyntaxError,"A function with this signature has not been declared.", tok.line, "", function->name, tok.file).exit();
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
	if(builtin_kind.contains(kind)) return true;
	return false;
}

NodeAST *NodeFunctionCall::do_function_call_hoisting(NodeProgram *program) {
	static ReturnFunctionCallHoisting hoisting;
	return hoisting.do_function_call_hoisting(*this, program);
}

NodeAST *NodeFunctionCall::do_function_inlining(NodeProgram *program) {
	static FunctionInlining inlining(program);
	return inlining.do_function_inlining(*this);
}

bool NodeFunctionCall::is_destructive_builtin_func() const {
	return kind == NodeFunctionCall::Kind::Builtin and destructive_functions.contains(function->name);
}

bool NodeFunctionCall::check_restricted_environment(NodeCallback *current_callback) const {
	return BuiltinRestrictionValidator::check_function_callability(*this, current_callback);
}

void NodeFunctionCall::determine_function_strategy(NodeProgram *program, NodeCallback *current_callback) {
	static ASTFunctionStrategy function_strategy(program, program->compiler_config->parameter_passing);
	function_strategy.determine_function_strategy(*this, current_callback);
}

bool NodeFunctionCall::is_in_access_chain() const {
	return parent and parent->cast<NodeAccessChain>();
}

bool NodeFunctionCall::has_side_effects(const std::unordered_set<std::string> &free_vars) {
	if (BuiltinRestrictionValidator::is_builtin_with_side_effects(this->function->name)) {
		return true;
	}
	static FreeVarCollector free_var;
	const auto& vars = free_var.collect(*this);
	// iterate the smaller set to minimize hash lookups
	if (vars.size() <= free_vars.size()) {
		for (const auto& v : vars) {
			if (free_vars.contains(v)) return true;
		}
	} else {
		for (const auto& fv : free_vars) {
			if (vars.contains(fv)) return true;
		}
	}

	const auto& visited_functions = free_var.get_visited_functions();
	for (const auto* func : visited_functions) {
		if (BuiltinRestrictionValidator::is_builtin_with_side_effects(func->header->name)) {
			return true;
		}
	}
	return false;
}


// ************* NodeSortSearch ***************
NodeAST *NodeSortSearch::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeSortSearch::NodeSortSearch(const NodeSortSearch& other)
	: NodeInstruction(other), array(clone_unique(other.array)), name(other.name),
	  value(clone_unique(other.value)), from(clone_unique(other.from)), to(clone_unique(other.to)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeSortSearch::clone() const {
	return std::make_unique<NodeSortSearch>(*this);
}
NodeAST *NodeSortSearch::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (array.get() == oldChild) {
		if(auto new_ref = cast_node<NodeReference>(newChild.release())) {
			array = std::unique_ptr<NodeReference>(new_ref);
			return array.get();
		}
	} else if (value.get() == oldChild) {
		value = std::move(newChild);
		return value.get();
	} else if (from.get() == oldChild) {
		from = std::move(newChild);
		return from.get();
	} else if (to.get() == oldChild) {
		to = std::move(newChild);
		return to.get();
	}
	return nullptr;
}

ASTLowering* NodeSortSearch::get_lowering(NodeProgram *program) const {
	static LoweringSortSearch lowering(program);
	return &lowering;
}

ASTLowering* NodeSortSearch::get_post_lowering(NodeProgram *program) const {
	static PostLoweringSortSearch lowering(program);
	return &lowering;
}

// ************* NodeNumElements ***************
NodeAST *NodeNumElements::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNumElements::NodeNumElements(const NodeNumElements& other)
	: NodeInstruction(other), array(clone_unique(other.array)),
	  dimension(clone_unique(other.dimension)) {
	NodeNumElements::set_child_parents();
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
          r_value(clone_unique(other.r_value)), has_object(other.has_object), is_parameter_stack(other.is_parameter_stack) {
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
		} else {
			auto error = CompileError(ErrorType::SyntaxError, "", "", oldChild->tok);
			error.m_message = "Tried to assign to a non-reference. Left side of assignment must be a reference.";
			error.exit();
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

NodeAST *NodeSingleAssignment::do_array_normalization(NodeProgram *program) {
	static NormalizeArrayAssign array_assign(program);
	return accept(array_assign);
}

void NodeSingleAssignment::check_for_constant_assignment() const {
	// check if l_value is a constant
	if(auto declaration = l_value->get_declaration()) {
		if (l_value->data_type == DataType::Const) {
			// is ok when we are in a constructor and the variable is a member of the class that has not
			// yet been initialized
			auto strct = l_value->is_member_ref();
			auto curr_func = get_current_function();
			if (strct and curr_func) {
				if(curr_func->header->name == strct->name + OBJ_DELIMITER + NodeStruct::CONSTRUCTOR) {
					if (auto decl = declaration->parent->cast<NodeSingleDeclaration>(); decl->value) {
						auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *this);
						error.m_message = "Cannot assign to constant member <"+ declaration->name +"> in the constructor because it was already given a value at its declaration.";
						error.exit();
						return;
					} else return;
				} else if (curr_func->header->name == strct->name + OBJ_DELIMITER + NodeStruct::DESTRUCTOR) {
					// ok in destructor
					return;
				}
			}
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *this);
			error.m_message = "Cannot reassign value to constant variable <"+ l_value->name +">. Once declared, the value of a constant cannot be changed.";
			error.exit();
			return;
		}
	}
}

// ************* NodeCompoundAssignment ***************
NodeAST *NodeCompoundAssignment::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeCompoundAssignment::NodeCompoundAssignment(const NodeCompoundAssignment& other)
		: NodeInstruction(other), l_value(clone_unique(other.l_value)),
		  r_value(clone_unique(other.r_value)), op(other.op) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeCompoundAssignment::clone() const {
	return std::make_unique<NodeCompoundAssignment>(*this);
}

ASTDesugaring * NodeCompoundAssignment::get_desugaring(NodeProgram *program) const {
	static DesugarCompoundAssignment desugaring(program);
	return &desugaring;
}

// ************* NodeDeclaration ***************
NodeAST *NodeDeclaration::accept(ASTVisitor &visitor) {
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
NodeAST *NodeSingleDeclaration::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeSingleDeclaration::NodeSingleDeclaration(const NodeSingleDeclaration& other)
        : NodeInstruction(other), variable(clone_shared(other.variable)),
          value(clone_unique(other.value)) {
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

ASTDesugaring * NodeSingleDeclaration::get_desugaring(NodeProgram *program) const {
	static DesugarUIControlArray desugaring(program);
	return &desugaring;
}

ASTLowering* NodeSingleDeclaration::get_lowering(NodeProgram *program) const {
	if(variable->cast<NodeUIControl>()) {
		return variable->get_lowering(program);
	} else if(variable->cast<NodeList>()) {
		return variable->get_lowering(program);
	} else if (variable->cast<NodeArray>()) {
		return variable->get_lowering(program);
	} else if (variable->cast<NodeNDArray>()) {
		return variable->get_lowering(program);
	}
	return nullptr;
}

std::unique_ptr<NodeSingleAssignment> NodeSingleDeclaration::to_assign_stmt(NodeDataStructure* var) {
    // if var provided -> turn to reference else turn variable to reference
    auto node_array_var = var ? var->to_reference() : variable->to_reference();
//	node_array_var->ty = var ? var->ty : variable->ty;
	node_array_var->do_type_inference(nullptr);
    // if declare stmt has r_value -> clone it else create new NodeInt with value 0
    auto node_assignee = value ? value->clone() : TypeRegistry::get_neutral_element_from_type(variable->ty);
    auto node_assign_statement = std::make_unique<NodeSingleAssignment>(
            std::move(node_array_var),
            std::move(node_assignee),
            tok
    );
	node_assign_statement->kind = this->kind;
    return node_assign_statement;
}


// ************* NodeFunctionParam ***************
NodeAST *NodeFunctionParam::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeFunctionParam::NodeFunctionParam(const NodeFunctionParam& other)
	: NodeInstruction(other), is_pass_by_ref(other.is_pass_by_ref), variable(clone_shared(other.variable)),
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

size_t NodeFunctionParam::get_index() {
	if (const auto func_header = parent->cast<NodeFunctionHeader>()) {
		for (size_t i = 0; i< func_header->params.size(); i++) {
			if (func_header->params[i].get() == this) {
				return i;
			}
		}
	}
	return -1;
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

void NodeBlock::flatten(const bool force) {
	// Reserviere genug Platz für den schlimmsten Fall, in dem keine Flattening nötig ist
	std::vector<std::unique_ptr<NodeStatement>> new_statements;
	new_statements.reserve(statements.size());

	for (auto& statement : statements) {
		if (const auto inner_body = statement->statement->cast<NodeBlock>()) {
//			node_innner_body->flatten();
			auto& inner_statements = inner_body->statements;

			// Entfernen Sie DeadCode-Nodes in einem Schritt
			auto new_end = std::remove_if(inner_statements.begin(), inner_statements.end(),
										  [](const std::unique_ptr<NodeStatement>& stmt) {
											return stmt->statement->cast<NodeDeadCode>();
										  });
			inner_statements.erase(new_end, inner_statements.end());

			if(inner_body->scope and !force) {
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

bool NodeBlock::determine_scope() {
	if(scope) return true;
	scope = false;
	if (parent->cast<NodeNamespace>()) return false;
	if (parent->cast<NodeStruct>()) return false;
	if(!parent->cast<NodeStatement>()) { // and !is_instance_of<NodeDataStructure>(parent)) {
		scope = true;
		return scope;
	}
	return false;
}

NodeBlock* NodeBlock::wrap_in_loop_nest(std::vector<std::shared_ptr<NodeDataStructure>> iterators,
                                        std::vector<std::unique_ptr<NodeAST>> lower_bounds,
                                        std::vector<std::unique_ptr<NodeAST>> upper_bounds, bool declare) {
	auto inner_body = std::make_unique<NodeBlock>(std::move(statements), tok);
	inner_body->scope = true;
	NodeBlock* for_loop_body = nullptr;
	for (int i = (int)iterators.size()-1; i>=0 ; i--) {
		auto node_for = std::make_unique<NodeFor>(
			std::make_unique<NodeSingleAssignment>(iterators[i]->to_reference(), std::move(lower_bounds[i]), Token()),
			token::TO,
			std::make_unique<NodeBinaryExpr>(token::SUB, std::move(upper_bounds[i]), std::make_unique<NodeInt>(1, Token()), Token()),
			std::move(inner_body),
			Token()
		);
		if(i == (int) iterators.size()-1) for_loop_body = node_for->body.get();
		inner_body = std::make_unique<NodeBlock>(Token(), true);
		inner_body->add_as_stmt(std::move(node_for));
		inner_body->get_last_statement()->lower(nullptr);
	}
	if (declare) {
		for(auto & iterator : iterators) {
			// iterator->is_local = true;
			auto node_decl = std::make_unique<NodeSingleDeclaration>(iterator, nullptr, Token());
			inner_body->prepend_as_stmt(std::move(node_decl));
		}
	}
	statements = std::move(inner_body->statements);
	scope = true;
	this->set_child_parents();
	this->collect_references();
	return for_loop_body;
}

NodeBlock* NodeBlock::wrap_in_loop(const std::shared_ptr<NodeDataStructure>& iterator, std::unique_ptr<NodeAST> lower_bound, std::unique_ptr<NodeAST> upper_bound, const bool declare) {
	auto inner_body = std::make_unique<NodeBlock>(std::move(statements), tok);
	inner_body->scope = true;
	auto node_for = std::make_unique<NodeFor>(
		std::make_unique<NodeSingleAssignment>(iterator->to_reference(), std::move(lower_bound), Token()),
		token::TO,
		std::make_unique<NodeBinaryExpr>(token::SUB, std::move(upper_bound), std::make_unique<NodeInt>(1, Token()), Token()),
		std::move(inner_body),
		Token()
	);
	auto for_loop_body = node_for->body.get();
	inner_body = std::make_unique<NodeBlock>(Token(), true);
	inner_body->add_as_stmt(std::move(node_for));
	inner_body->get_last_statement()->lower(nullptr);
	// iterator->is_local = true;
	if(declare) {
		auto node_decl = std::make_unique<NodeSingleDeclaration>(iterator, nullptr, Token());
		inner_body->prepend_as_stmt(std::move(node_decl));
	}
	statements = std::move(inner_body->statements);
	scope = true;
	this->set_child_parents();
	this->collect_references();
	return for_loop_body;
}

// ************* NodeFamily ***************
NodeAST *NodeFamily::accept(ASTVisitor &visitor) {
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
    static DesugarFamily desugaring(program);
    return &desugaring;
}

// ************* NodeIf ***************
NodeAST *NodeIf::accept(ASTVisitor &visitor) {
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

NodeAST * NodeIf::do_short_circuit_transform(NodeProgram *program) {
	if (auto node_binary = condition->cast<NodeBinaryExpr>()) {
		if (node_binary->needs_short_circuiting()) {
			static FunctionShortCircuit transform(program);
			return transform.do_short_circuit(*this);
		}
	}
	if (auto node_unary = condition->cast<NodeUnaryExpr>()) {
		if (node_unary->needs_short_circuiting()) {
			static FunctionShortCircuit transform(program);
			return transform.do_short_circuit(*this);
		}
	}
	return this;
}

NodeAST *NodeIf::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (condition.get() == oldChild) {
        condition = std::move(newChild);
        return condition.get();
    }
    return nullptr;
}

// ************* NodeTernary ***************
NodeAST *NodeTernary::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeTernary::NodeTernary(const NodeTernary& other)
		: NodeInstruction(other), condition(clone_unique(other.condition)),
		  if_branch(clone_unique(other.if_branch)),
		  else_branch(clone_unique(other.else_branch)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeTernary::clone() const {
	return std::make_unique<NodeTernary>(*this);
}

NodeAST *NodeTernary::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (condition.get() == oldChild) {
		condition = std::move(newChild);
		return condition.get();
	} else if (if_branch.get() == oldChild) {
		if_branch = std::move(newChild);
		return if_branch.get();
	} else if (else_branch.get() == oldChild) {
		else_branch = std::move(newChild);
		return else_branch.get();
	}
	return nullptr;
}

// ************* NodeFor ***************
NodeAST *NodeFor::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeFor::NodeFor(const NodeFor& other)
        : NodeLoop(other), iterator(clone_unique(other.iterator)), to(other.to), iterator_end(clone_unique(other.iterator_end)),
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
    } else if (step.get() == oldChild) {
		step = std::move(newChild);
		return step.get();
	}
    return nullptr;
}

//ASTDesugaring * NodeFor::get_desugaring(NodeProgram *program) const {
//    static LoweringFor desugaring(program);
//    return &desugaring;
//}

ASTLowering * NodeFor::get_lowering(NodeProgram *program) const {
	static LoweringFor lowering(program);
	return &lowering;
}

bool NodeFor::determine_linear() {
	bool is_linear = true;
	is_linear &= iterator->r_value->is_constant();
	is_linear &= iterator_end->is_constant();
	if(step) is_linear &= step->is_constant();
	return is_linear;
}

std::unique_ptr<NodeRange> NodeFor::determine_loop_range() {
	auto start = iterator->r_value->clone();
	auto stop = iterator_end->clone();
	auto step = this->step ? this->step->clone() : std::make_unique<NodeInt>(1, tok);
	if(to == token::DOWNTO) {
		step = std::make_unique<NodeUnaryExpr>(token::SUB, std::move(step), tok);
	}
	return std::make_unique<NodeRange>(std::move(start), std::move(stop), std::move(step), tok);
}

std::unique_ptr<NodeAST> NodeFor::get_num_iterations() {
	// (end-start)/step
	auto start = iterator->r_value->clone();
	auto end = iterator_end->clone();
	auto node_range = std::make_unique<NodeRange>(std::move(start), std::move(end), nullptr, tok);
	return node_range->get_num_iterations();
}

// ************* NodeForEach ***************
NodeAST *NodeForEach::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeForEach::NodeForEach(const NodeForEach& other)
        : NodeLoop(other), key(clone_unique(other.key)), value(clone_unique(other.value)),
		range(clone_unique(other.range)), body(clone_unique(other.body)) {
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

//ASTDesugaring * NodeForEach::get_desugaring(NodeProgram *program) const {
//    static LoweringForEach desugaring(program);
//    return &desugaring;
//}

ASTLowering * NodeForEach::get_lowering(NodeProgram *program) const {
	static LoweringForEach lowering(program);
	return &lowering;
}

bool NodeForEach::determine_linear() {
	return range->is_constant();
}

std::unique_ptr<NodeRange> NodeForEach::determine_loop_range() {
	NodeAST* r = range.get();
	if(auto pairs = range->cast<NodePairs>()) {
		r = pairs->range.get();
	}

	if(auto node_range = r->cast<NodeRange>()) {
		return clone_as<NodeRange>(node_range);
	} else if (auto init_list = r->cast<NodeInitializerList>()) {
		if(auto transformed_range = init_list->transform_to_range()) {
			return std::move(transformed_range.value());
		}
	} else if (auto comp_ref = cast_node<NodeCompositeRef>(r)) {
		auto start = std::make_unique<NodeInt>(0, r->tok);
		auto stop = comp_ref->get_size();
		auto step = std::make_unique<NodeInt>(1, r->tok);
		return std::make_unique<NodeRange>(std::move(start), std::move(stop), std::move(step), r->tok);
	}
	return nullptr;
}

std::unique_ptr<NodeAST> NodeForEach::get_num_iterations() {
	if(auto node_range = determine_loop_range()) {
		return node_range->get_num_iterations();
	} else if(auto init_list = range->cast<NodeInitializerList>()) {
		auto dimensions = init_list->get_dimensions();
		int size = 0;
		for(auto &dim : dimensions) {
			size += dim;
		}
		return std::make_unique<NodeInt>(size, tok);
	}
	return nullptr;
}

// ************* NodePairs ***************
NodeAST *NodePairs::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodePairs::NodePairs(const NodePairs& other)
	: NodeInstruction(other), range(clone_unique(other.range)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodePairs::clone() const {
	return std::make_unique<NodePairs>(*this);
}

bool NodePairs::check_environment() const {
	// check if in for each loop
	if (parent->cast<NodeForEach>()) {
		return true;
	}
	auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
	error.m_message = "As of v"+COMPILER_VERSION+", <pairs> can only be used in for-each loops.";
	error.exit();
	return false;
}

NodeAST *NodePairs::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (range.get() == oldChild) {
		range = std::move(newChild);
		return range.get();
	}
	return nullptr;
}

// ************* NodeRange ***************
NodeAST *NodeRange::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeRange::NodeRange(const NodeRange& other)
	: NodeInstruction(other), start(clone_unique(other.start)), stop(clone_unique(other.stop)),
	  step(clone_unique(other.step)) {
	set_child_parents();
}
std::unique_ptr<NodeAST> NodeRange::clone() const {
	return std::make_unique<NodeRange>(*this);
}
NodeAST *NodeRange::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (start.get() == oldChild) {
		start = std::move(newChild);
		return start.get();
	} else if (stop.get() == oldChild) {
		stop = std::move(newChild);
		return stop.get();
	} else if (step.get() == oldChild) {
		step = std::move(newChild);
		return step.get();
	}
	return nullptr;
}

std::unique_ptr<NodeAST> NodeRange::get_num_iterations() {
	auto start = this->start->clone();
	auto stop = this->stop->clone();
	std::unique_ptr<NodeAST> expr = std::make_unique<NodeBinaryExpr>(
		token::SUB,
		std::move(stop),
		std::move(start),
		tok
	);
	expr->ty = TypeRegistry::Integer;
	if(step) {
		expr = std::make_unique<NodeBinaryExpr>(
		token::DIV,
		std::move(expr),
		std::move(step),
			tok
		);
	}
	expr->ty = TypeRegistry::Integer;
	return DefinitionProvider::create_builtin_call("abs", std::move(expr));
}

std::unique_ptr<NodeInitializerList> NodeRange::to_initializer_list() const {
	if (!start or !stop) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Cannot convert <range> to <initializer_list> because it is missing start or stop.";
		error.exit();
		return nullptr;
	}
	if (!all_literals()) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Cannot convert <range> to <initializer_list> because not all elements are literals.";
		error.exit();
	}
	auto initializer_list = std::make_unique<NodeInitializerList>(tok);
	auto start_int = start->cast<NodeInt>();
	auto stop_int = stop->cast<NodeInt>();
	auto step_int = step->cast<NodeInt>();
	if (start_int && stop_int && step_int) {
		for (int32_t val = start_int->value; val < stop_int->value; val += step_int->value) {
			initializer_list->add_element(std::make_unique<NodeInt>(val, tok));
		}
		return initializer_list;
	}

	auto start_real = start->cast<NodeReal>();
	auto stop_real = stop->cast<NodeReal>();
	auto step_real = step->cast<NodeReal>();
	if (start_real && stop_real && step_real) {
		for (double val = start_real->value; val < stop_real->value; val += step_real->value) {
			initializer_list->add_element(std::make_unique<NodeReal>(val, tok));
		}
		return initializer_list;
	}

	auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
	error.m_message = "Cannot convert <range> to <initializer_list> because it contains non-integer or non-real values.";
	error.exit();
	return nullptr;

}

bool NodeRange::check_environment() const {
	// check if in for each loop
	if (parent->cast<NodeForEach>() or parent->cast<NodePairs>()) {
		return true;
	}
	// check if this should be lowered to initializer list
	if (parent->cast<NodeSingleAssignment>() or parent->cast<NodeSingleDeclaration>()) {
		return true;
	}
	auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
	error.m_message = "As of v"+COMPILER_VERSION+", <range> can only be used in for-each loops, assignments or declarations.";
	error.exit();
	return false;
}

ASTLowering * NodeRange::get_lowering(NodeProgram *program) const {
	static LoweringRange lowering(program);
	return &lowering;
}

// ************* NodeWhile ***************
NodeAST *NodeWhile::accept(struct ASTVisitor &visitor) {
    return visitor.visit(*this);
}
NodeWhile::NodeWhile(const NodeWhile& other)
        : NodeLoop(other), condition(clone_unique(other.condition)),
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
ASTLowering* NodeWhile::get_lowering(NodeProgram *program) const {
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
    NodeSelect::set_child_parents();
}
std::unique_ptr<NodeAST> NodeSelect::clone() const {
    return std::make_unique<NodeSelect>(*this);
}
NodeAST *NodeSelect::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
    if (expression.get() == oldChild) {
        expression = std::move(newChild);
        return expression.get();
    }
    for ( auto & cas : cases) {
        if(cas.first[0].get() == oldChild) {
            cas.first[0] = std::move(newChild);
            return cas.first[0].get();
        } else if(cas.first[1].get() == oldChild) {
        	cas.first[1] = std::move(newChild);
        	return cas.first[1].get();
        }
    }
    return nullptr;
}

// ************* NodeBreak ***************
NodeAST *NodeBreak::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeBreak::NodeBreak(const NodeBreak& other)
	: NodeInstruction(other) {}

std::unique_ptr<NodeAST> NodeBreak::clone() const {
	return std::make_unique<NodeBreak>(*this);
}

// ************* NodeNamespace ***************
NodeAST *NodeNamespace::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeNamespace::NodeNamespace(const NodeNamespace& other)
	: NodeInstruction(other), prefix(other.prefix), members(clone_unique(other.members)) {
	for (auto &def: other.function_definitions) {
		function_definitions.emplace_back(clone_shared(def));
	}
}

std::unique_ptr<NodeAST> NodeNamespace::clone() const {
	return std::make_unique<NodeNamespace>(*this);
}

ASTDesugaring * NodeNamespace::get_desugaring(NodeProgram *program) const {
	static DesugarNamespace desugaring(program);
	return &desugaring;
}

void NodeNamespace::inline_namespace(NodeProgram *program) {
	// add struct methods to program functions
	for(auto & func: function_definitions) {
		func->parent = program;
		program->function_definitions.push_back(func);
	}
	function_definitions.clear();
	program->global_declarations->append_body(std::move(members));
	members = std::make_unique<NodeBlock>(Token());
	set_child_parents();
}
