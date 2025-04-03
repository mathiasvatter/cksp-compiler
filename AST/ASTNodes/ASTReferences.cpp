//
// Created by Mathias Vatter on 25.04.24.
//

#include <regex>

#include "ASTReferences.h"
#include "../ASTVisitor/ASTVisitor.h"
#include "../../Lowering/LoweringList.h"
#include "../../Lowering/DataLowering/DataLoweringNDArray.h"
#include "../../Lowering/LoweringPointer.h"
#include "../../Lowering/LoweringAccessChain.h"
#include "../../Lowering/LoweringNil.h"
#include "../../Lowering/LoweringGetControl.h"

// ************* NodeVariableRef ***************
NodeAST *NodeVariableRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeVariableRef::NodeVariableRef(const NodeVariableRef& other)
	: NodeReference(other) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeVariableRef::clone() const {
	return std::make_unique<NodeVariableRef>(*this);
}

std::unique_ptr<NodeArrayRef> NodeVariableRef::to_array_ref(std::unique_ptr<NodeAST> index) {
	return std::make_unique<NodeArrayRef>(name, std::move(index), tok);
}

std::unique_ptr<NodePointerRef> NodeVariableRef::to_pointer_ref() {
	return std::make_unique<NodePointerRef>(name, tok);
}


std::unique_ptr<NodeAccessChain> NodeVariableRef::to_method_chain() {
	// split into variable references
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->parent = this->parent;
	return method_chain;
}

//bool NodeVariableRef::is_array_constant() {
//	if(kind == NodeReference::Kind::Builtin) return false;
//	if (declaration && (declaration->get_node_type() == NodeType::Array || declaration->get_node_type() == NodeType::List)) {
//		auto list = static_cast<NodeList*>(declaration);
//		const std::string& prefix = list->name + ".SIZE";
//		if (name.compare(0, prefix.length(), prefix) == 0) {
//			return true;
//		}
//	}
//	return false;
//}

//bool NodeVariableRef::is_ndarray_constant() {
//	if(kind == NodeReference::Kind::Builtin) return false;
//	size_t pos = name.find(".SIZE_D");
//	if(pos == std::string::npos) return false;
//	auto n = name.substr(0, pos);
//	auto dimension = name.substr(pos+7, name.length());
//	if (declaration && declaration->get_node_type() == NodeType::NDArray) {
//		auto ndarray = static_cast<NodeNDArray*>(declaration);
//		const std::string& prefix = ndarray->name + ".SIZE_D";
//		if (name.compare(0, prefix.length(), prefix) == 0) {
//			std::string number_str = name.substr(prefix.length());
//			try {
//				int number = std::stoi(number_str);
//				return number >= 1 && number <= ndarray->dimensions;
//			} catch (const std::invalid_argument&) {
//				return false;
//			}
//		}
//	}
//	return false;
//}

std::unique_ptr<NodeNumElements> NodeVariableRef::transform_ndarray_constant() {
	if(kind == NodeReference::Kind::Builtin) return nullptr;
	size_t pos = name.find(".SIZE_D");
	if(pos == std::string::npos) return nullptr;
	auto array_name = name.substr(0, pos);
	auto dimension = name.substr(pos+7, name.length());
	auto nd_array_ref = std::make_unique<NodeNDArrayRef>(array_name, nullptr, tok);
	try {
		int dim_int = std::stoi(dimension);
		return std::make_unique<NodeNumElements>(
			std::move(nd_array_ref),
			std::make_unique<NodeInt>(dim_int, tok),
			tok
		);
	} catch (const std::invalid_argument&) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Invalid dimension number in NDArray reference: " + name + ".";
		error.m_got = dimension;
		error.exit();
	}
	return nullptr;
}

std::unique_ptr<NodeNumElements> NodeVariableRef::transform_array_constant() {
	if(kind == NodeReference::Kind::Builtin) return nullptr;
	size_t pos = name.find(".SIZE");
	if(pos == std::string::npos) return nullptr;
	auto array_name = name.substr(0, pos);
	auto array_ref = std::make_unique<NodeArrayRef>(array_name, nullptr, tok);
	return std::make_unique<NodeNumElements>(
		std::move(array_ref),
		nullptr,
		tok
	);
}


std::unique_ptr<NodeReference> NodeVariableRef::expand_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_array_ref = to_array_ref(std::move(new_index));
	node_array_ref->match_data_structure(get_declaration());
	node_array_ref->ty = ty;
	return node_array_ref;
}

// ************* NodeArrayRef ***************
NodeAST *NodeArrayRef::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeArrayRef::NodeArrayRef(const NodeArrayRef& other)
	: NodeCompositeRef(other), index(clone_unique(other.index)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeArrayRef::clone() const {
	return std::make_unique<NodeArrayRef>(*this);
}
NodeAST *NodeArrayRef::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (index.get() == oldChild) {
		index = std::move(newChild);
		return index.get();
	}
	return nullptr;
}

//ASTLowering* NodeArrayRef::get_lowering(NodeProgram *program) const {
//    static LoweringArray lowering(program);
//    return &lowering;
//}

std::unique_ptr<NodeNDArrayRef> NodeArrayRef::to_ndarray_ref() {
	return std::make_unique<NodeNDArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, index->clone()) : nullptr, tok);
}

std::unique_ptr<NodeAccessChain> NodeArrayRef::to_method_chain() {
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	auto array_ref = clone_as<NodeArrayRef>(this);
	array_ref->name = ptr_strings.back();
	ptr_strings.pop_back();
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->add_method(std::move(array_ref));
	method_chain->parent = this->parent;
	return method_chain;
}

std::unique_ptr<NodeAST> NodeArrayRef::get_size() {
	auto new_ref = clone_as<NodeArrayRef>(this);
	new_ref->index = nullptr;
	new_ref->ty = get_declaration()->ty;
	return DefinitionProvider::num_elements(std::move(new_ref));
}

bool NodeArrayRef::is_list_sizes() const {
	if (get_declaration()->get_node_type() == NodeType::List) {
		const auto list = static_pointer_cast<NodeList>(get_declaration());
		const std::string& prefix = list->name + ".sizes";
		if (name.compare(0, prefix.length(), prefix) == 0) {
			return true;
		}
	}
	return false;
}

std::unique_ptr<NodeReference> NodeArrayRef::expand_dimension(std::unique_ptr<NodeAST> new_index) {
	// if array has no indexes -> wildcard
	if(!index) {
		set_index(std::make_unique<NodeWildcard>("*", tok));
	}
	auto node_ndarray_ref = to_ndarray_ref();
	if(new_index) node_ndarray_ref->indexes->prepend_param(std::move(new_index));
	node_ndarray_ref->match_data_structure(get_declaration());
	node_ndarray_ref->ty = ty;
	node_ndarray_ref->determine_sizes();
	return node_ndarray_ref;
}

NodeBlock* NodeArrayRef::iterate_over(std::unique_ptr<NodeBlock>& for_loop_block, NodeProgram* program) {
	for_loop_block->scope = true;
	auto iterator = ASTVisitor::get_iterator_var(tok, program->def_provider->get_fresh_name("_iter"));
	set_index(iterator->to_reference());
	ty = ty->get_element_type();
	return for_loop_block->wrap_in_loop(std::move(iterator), std::make_unique<NodeInt>(0, tok), get_size());
}

// ************* NodeNDArrayRef ***************
NodeAST *NodeNDArrayRef::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeNDArrayRef::NodeNDArrayRef(const NodeNDArrayRef& other)
	: NodeCompositeRef(other), indexes(clone_unique(other.indexes)),
	sizes(clone_unique(other.sizes)) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeNDArrayRef::clone() const {
	return std::make_unique<NodeNDArrayRef>(*this);
}

ASTLowering* NodeNDArrayRef::get_data_lowering(NodeProgram *program) const {
    static DataLoweringNDArray lowering(program);
    return &lowering;
}

std::unique_ptr<NodeAST> NodeNDArrayRef::get_size() {
	// if indexes are set and no wildcards -> size is 1
	if(indexes and num_wildcards() == 0) {
		return std::make_unique<NodeInt>(1, tok);
	}
	auto ref = clone_as<NodeNDArrayRef>(this);
	ref->ty = get_declaration()->ty;
	return std::make_unique<NodeNumElements>(std::move(ref), nullptr, tok);
}

std::unique_ptr<NodeAST> NodeNDArrayRef::get_size(std::unique_ptr<NodeAST> dim) {
	auto ref = clone_as<NodeNDArrayRef>(this);
	ref->indexes = nullptr;
	ref->ty = get_declaration()->ty;
	return std::make_unique<NodeNumElements>(std::move(ref), std::move(dim), tok);
}


std::unique_ptr<NodeArrayRef> NodeNDArrayRef::to_array_ref(std::unique_ptr<NodeAST> index) {
	return std::make_unique<NodeArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, std::move(index)) : nullptr, tok);
}

bool NodeNDArrayRef::determine_sizes() {
	if(!get_declaration()) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "NDArray reference has no declaration.";
		error.exit();
		return false;
	}
	if(get_declaration()->get_node_type() != NodeType::NDArray) {
		if(get_declaration()->get_node_type() == NodeType::List) return false;
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "NDArray reference has to be declared as NDArray.";
		error.m_got = get_declaration()->get_string();
		error.exit();
		return false;
	}
	auto node_ndarray = static_pointer_cast<NodeNDArray>(get_declaration());
	// has no size if function definition parameter
	if(!node_ndarray->sizes) {
		if(indexes) {
			auto n_sizes = std::make_unique<NodeParamList>(tok);
			for(int i = 0; i<indexes->size(); i++) {
				n_sizes->add_param(get_size(std::make_unique<NodeInt>(i+1, tok)));
			}
			set_sizes(std::move(n_sizes));
		}
//		return false;
	} else {
		set_sizes(clone_as<NodeParamList>(node_ndarray->sizes.get()));
	}
	return true;
}

std::unique_ptr<NodeAccessChain> NodeNDArrayRef::to_method_chain() {
	auto ptr_strings = this->get_ptr_chain();
	auto method_chain = std::make_unique<NodeAccessChain>(tok);
	auto array_ref = clone_as<NodeNDArrayRef>(this);
	array_ref->name = ptr_strings.back();
	ptr_strings.pop_back();
	for(auto &str : ptr_strings) {
		method_chain->add_method(std::make_unique<NodeVariableRef>(str, tok));
	}
	method_chain->add_method(std::move(array_ref));
	method_chain->parent = this->parent;
	return method_chain;
}

std::unique_ptr<NodeReference> NodeNDArrayRef::expand_dimension(std::unique_ptr<NodeAST> new_index) {
	determine_sizes();
	// if array has no indexes -> everything should be copied -> wildcards for every index of size
	if (!indexes) {
		add_wildcards();
	}
	if(new_index) indexes->prepend_param(std::move(new_index));
	return clone_as<NodeReference>(this);
}

int NodeNDArrayRef::num_wildcards() const {
	int count = 0;
	if(indexes) {
		for(auto & idx: indexes->params)
			if(idx->cast<NodeWildcard>()) count++;
	}
	return count;
}

bool NodeNDArrayRef::add_wildcards() {
	if(this->indexes) return false;
	if(!this->sizes and !this->ty) return false;
	this->indexes = std::make_unique<NodeParamList>(this->tok);
	this->indexes->parent = this->indexes.get();
	// get dimensions either from type or from sizes
	auto num_dimensions = ty ? ty->get_dimensions() : sizes->params.size();
	for(int i=0; i<num_dimensions; i++) {
		auto wildcard = std::make_unique<NodeWildcard>("*", Token());
		this->indexes->add_param(std::move(wildcard));
	}
	return true;
}

std::pair<int, int> NodeNDArrayRef::get_wildcard_dimensions() const {
	auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
	std::vector<bool> wildcards;
	// get wildcard dimension
	int wildcard_start = -1;
	int wildcard_end = -1;
	if(!indexes) {
		return {0, 0};
	}
	for(int i = 0; i<indexes->params.size(); i++) {
		const auto &idx = indexes->params[i];
		bool is_wildcard = idx->cast<NodeWildcard>();
		wildcards.push_back(is_wildcard);
		if(wildcards.size() > 2) {
			// (1,0,1....)
			if(wildcards[wildcards.size()-2] and !wildcards[wildcards.size()-1]) {
				error.m_message = "Wildcards must be contiguous in <NDArray> index when assigning list.";
				error.exit();
			}
		}
		if (is_wildcard) {
			if (wildcard_start == -1) {
				wildcard_start = i;
			}
			wildcard_end = i;
		}
	}
	if(wildcard_start == -1) {
		error.m_message = "No wildcard found in <NDArray> index when assigning list.";
		error.exit();
	}
	// if more than one wildcard and it is not right aligned:
	if(wildcard_end-wildcard_start > 1 and wildcard_end < wildcards.size()-1) {
		error.m_message = "Wildcards have to be right aligned in index list.";
		error.exit();
	}
	return {wildcard_start, wildcard_end};
}

NodeBlock* NodeNDArrayRef::iterate_over(std::unique_ptr<NodeBlock> &body, NodeProgram* program) {
	if(!indexes) {
		determine_sizes();
		add_wildcards();
	}
	// if whole ndarray should be iterated over
	if(num_wildcards() == 0) {
		for(int i = 0; i<indexes->size(); i++) {
			indexes->set_param(i, std::make_unique<NodeWildcard>("*", tok));
		}
	}

	// replace every wildcard in index with iterator
	std::vector<std::shared_ptr<NodeDataStructure>> iterators;
	std::vector<std::unique_ptr<NodeAST>> lower_bounds;
	std::vector<std::unique_ptr<NodeAST>> upper_bounds;
	for(int i = 0; i< indexes->size(); i++) {
		if(indexes->param(i)->cast<NodeWildcard>()) {
			auto node_iterator = ASTVisitor::get_iterator_var(indexes->param(i)->tok, program->def_provider->get_fresh_name("_iter"));
			iterators.push_back(node_iterator);
			auto lower_bound = std::make_unique<NodeInt>(0, tok);
			lower_bounds.push_back(std::move(lower_bound));
			auto upper_bound = get_size(std::make_unique<NodeInt>(i+1, tok));
			upper_bounds.push_back(std::move(upper_bound));
			indexes->set_param(i, node_iterator->to_reference());
		}
	}
	ty = ty->get_element_type();
	// wrap body in loop nest
	return body->wrap_in_loop_nest(iterators, std::move(lower_bounds), std::move(upper_bounds));

}

void NodeNDArrayRef::replace_next_wildcard_with_index(std::unique_ptr<NodeInt> new_index) const {
	if(!indexes) throw_missing_indexes_error().exit();
	if(!sizes) throw_missing_sizes_error().exit();
	if(num_wildcards() == 0) return;

	auto [fst, snd] = get_wildcard_dimensions();
	indexes->param(fst)->replace_with(std::move(new_index));
	if(snd != fst) {
		for(int i = fst+1; i<=snd; i++) {
			indexes->param(i)->replace_with(std::make_unique<NodeInt>(0, indexes->param(i)->tok));
		}
	}
}

// ************* NodeFunctionHeaderRef ***************
NodeFunctionHeaderRef::NodeFunctionHeaderRef(std::string name, Token tok)
	: NodeReference(std::move(name), NodeType::FunctionHeaderRef, std::move(tok)) {}

NodeFunctionHeaderRef::NodeFunctionHeaderRef(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
	: NodeReference(std::move(name), NodeType::FunctionHeaderRef, std::move(tok)), args(std::move(args)) {
	set_child_parents();
}
NodeFunctionHeaderRef::~NodeFunctionHeaderRef() = default;

NodeFunctionHeaderRef::NodeFunctionHeaderRef(const NodeFunctionHeaderRef& other)
	: NodeReference(other), args(clone_unique(other.args)), has_forced_parenth(other.has_forced_parenth) {
	set_child_parents();
}

NodeAST *NodeFunctionHeaderRef::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}

std::unique_ptr<NodeAST> NodeFunctionHeaderRef::clone() const {
	return std::make_unique<NodeFunctionHeaderRef>(*this);
}

void NodeFunctionHeaderRef::update_parents(NodeAST *new_parent) {
	parent = new_parent;
	if(args) args->update_parents(this);
}

void NodeFunctionHeaderRef::set_child_parents() {
	if(args) args->parent = this;
}

int NodeFunctionHeaderRef::get_num_args() const {
	return (int)args->params.size();
}

bool NodeFunctionHeaderRef::has_no_args() const {
	return args->params.empty();
}

std::unique_ptr<NodeAST>& NodeFunctionHeaderRef::get_arg(const int i) const {
	if(get_num_args() <= i) {
		CompileError(ErrorType::InternalError, "Index out of bounds", "Function call argument index out of bounds", tok).exit();
	}
	return args->params[i];
}

void NodeFunctionHeaderRef::prepend_arg(std::unique_ptr<NodeAST> arg) const {
	args->prepend_param(std::move(arg));
}

void NodeFunctionHeaderRef::add_arg(std::unique_ptr<NodeAST> arg) const {
	args->add_param(std::move(arg));
}

void NodeFunctionHeaderRef::set_args(std::unique_ptr<NodeParamList> new_args) {
	args = std::move(new_args);
	args->parent = this;
}

// ************* NodeListRef ***************
NodeAST *NodeListRef::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeListRef::NodeListRef(const NodeListRef& other)
	: NodeReference(other), indexes(clone_unique(other.indexes)), sizes(other.sizes),
	pos(other.pos) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeListRef::clone() const {
	return std::make_unique<NodeListRef>(*this);
}

ASTLowering* NodeListRef::get_lowering(NodeProgram *program) const {
	static LoweringList lowering(program);
	return &lowering;
}


// ************* NodePointerRef ***************
NodeAST *NodePointerRef::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodePointerRef::NodePointerRef(const NodePointerRef& other)
	: NodeReference(other) {
	NodePointerRef::set_child_parents();
}

std::unique_ptr<NodeAST> NodePointerRef::clone() const {
	return std::make_unique<NodePointerRef>(*this);
}

std::unique_ptr<NodeArrayRef> NodePointerRef::to_array_ref(std::unique_ptr<NodeAST> index) {
	return std::make_unique<NodeArrayRef>(name, index ? std::make_unique<NodeParamList>(tok, std::move(index)) : nullptr, tok);
}

std::unique_ptr<NodeVariableRef> NodePointerRef::to_variable_ref() {
	return std::make_unique<NodeVariableRef>(name, tok);
}

ASTLowering* NodePointerRef::get_lowering(NodeProgram *program) const {
	static LoweringPointer lowering(program);
	return &lowering;
}

std::unique_ptr<NodeReference> NodePointerRef::expand_dimension(std::unique_ptr<NodeAST> new_index) {
	auto node_array_ref = to_array_ref(std::move(new_index));
	node_array_ref->match_data_structure(get_declaration());
	node_array_ref->ty = ty;
	return node_array_ref;
}

// ************* NodeNil ***************
NodeAST *NodeNil::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}
std::unique_ptr<NodeAST> NodeNil::clone() const {
	return std::make_unique<NodeNil>(*this);
}

ASTLowering* NodeNil::get_lowering(NodeProgram *program) const {
	static LoweringNil lowering(program);
	return &lowering;
}

// ************* NodeAccessChain ***************
NodeAST *NodeAccessChain::accept(struct ASTVisitor &visitor) {
	return visitor.visit(*this);
}

NodeAccessChain::NodeAccessChain(const NodeAccessChain& other)
	: NodeReference(other), chain(clone_vector(other.chain)), types(other.types) {
	set_child_parents();
}

std::unique_ptr<NodeAST> NodeAccessChain::clone() const {
	return std::make_unique<NodeAccessChain>(*this);
}

NodeAST *NodeAccessChain::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	for (auto& c : chain) {
		if (c.get() == oldChild) {
			c = std::move(newChild);
			return c.get();
		}
	}
	return nullptr;
}

ASTLowering* NodeAccessChain::get_lowering(NodeProgram *program) const {
	static LoweringAccessChain lowering(program);
	return &lowering;
}

//std::unique_ptr<NodeVariableRef> NodeAccessChain::is_size_constant() {
//	auto base = static_cast<NodeReference*>(chain[0].get());
//	if(base->declaration->get_node_type() == NodeType::NDArray
//		|| base->declaration->get_node_type() == NodeType::Array || base->declaration->get_node_type() == NodeType::List) {
//		if(chain.size() == 2) {
//			auto node_var_ref = std::make_unique<NodeVariableRef>(
//				this->get_string(),
//				tok);
//			node_var_ref->data_type = DataType::Const;
//			node_var_ref->declaration = base->declaration;
//			if(node_var_ref->is_ndarray_constant() || node_var_ref->is_array_constant()) {
//				node_var_ref->declaration = nullptr;
//				return node_var_ref;
//			}
//		}
//	}
//	return nullptr;
//}


// ************* NodeGetControl ***************
NodeAST *NodeGetControl::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeGetControl::NodeGetControl(const NodeGetControl& other)
	: NodeReference(other), ui_id(clone_unique(other.ui_id)), control_param(other.control_param) {
	NodeGetControl::set_child_parents();
}
std::unique_ptr<NodeAST> NodeGetControl::clone() const {
	return std::make_unique<NodeGetControl>(*this);
}
NodeAST *NodeGetControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ui_id.get() == oldChild) {
		ui_id = std::move(newChild);
		return ui_id.get();
	}
	return nullptr;
}

ASTLowering* NodeGetControl::get_lowering(NodeProgram *program) const {
	static LoweringGetControl lowering(program);
	return &lowering;
}

//std::unique_ptr<NodeReference> NodeGetControl::get_full_control_param(DefinitionProvider *def_provider) {
//	std::string control_par = to_lower(control_param);
//	if(control_par == "x") control_par = "pos_x";
//	if(control_par == "y") control_par = "pos_y";
//	if(control_par == "default") control_par += "_value";
//	if(auto builtin_var = def_provider->get_builtin_variable(to_upper("control_par_"+control_par))) {
//		return builtin_var->to_reference();
//	}
//	return nullptr;
//}

Type *NodeGetControl::get_control_type() const {
	const std::string control_par = to_lower(control_param);
	static const std::unordered_set<std::string> str_substrings{"name", "path", "picture", "help", "identifier", "label", "text"};
	static const std::unordered_set<std::string> int_substrings{"state", "alignment", "pos", "shifting"};
	Type* type = TypeRegistry::Integer;
	for (auto const &substring : str_substrings) {
		if(contains(control_par, substring)) {
			type = TypeRegistry::String;
			break;
		}
	}
	for (auto const &substring : int_substrings) {
		if(contains(control_par, substring)) {
			type = TypeRegistry::Integer;
			break;
		}
	}
	return type;
}

// ************* NodeSetControl ***************
NodeAST *NodeSetControl::accept(ASTVisitor &visitor) {
	return visitor.visit(*this);
}
NodeSetControl::NodeSetControl(const NodeSetControl& other)
	: NodeReference(other), ui_id(clone_unique(other.ui_id)),
	control_param(other.control_param), value(clone_unique(other.value)) {
	NodeSetControl::set_child_parents();
}
std::unique_ptr<NodeAST> NodeSetControl::clone() const {
	return std::make_unique<NodeSetControl>(*this);
}
NodeAST *NodeSetControl::replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {
	if (ui_id.get() == oldChild) {
		ui_id = std::move(newChild);
		return ui_id.get();
	}
	if(value.get() == oldChild) {
		value = std::move(newChild);
		return value.get();
	}
	return nullptr;
}

ASTLowering* NodeSetControl::get_lowering(NodeProgram *program) const {
	static LoweringGetControl lowering(program);
	return &lowering;
}

