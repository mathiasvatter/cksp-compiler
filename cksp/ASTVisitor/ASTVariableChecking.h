//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTVariableChecking final : public ASTVisitor {
	std::mutex mutex;
public:
	// in PreUIControlLowering we do not do access chain transform if no declaration is found
	// and no ui control data type checking in callback id
	// PostLowering fails hard if no declaration is found
	enum class Pass {
		PreUIControlLowering,
		PostUIControlLowering,
		PostLowering
	};
	void set_pass(const Pass pass) {
		this->pass = pass;
	}

	explicit ASTVariableChecking(NodeProgram* main, Pass pass = Pass::PreUIControlLowering);

	NodeAST* do_complete_traversal(NodeProgram& node) {
		// update function lookup map because of altered param counts after lambda lifting
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// erase all previously saved scopes
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_declaration_candidates();
		m_def_provider->refresh_data_vectors();
        node.accept(*this);
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		m_def_provider->refresh_scopes();
		// FunctionParamDataTypeGetter data_type_getter(m_program);
		// node.accept(data_type_getter);
		return &node;
    }

	NodeAST* do_reachable_traversal(NodeProgram& node) {
		// update function lookup map because of altered param counts after lambda lifting
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// erase all previously saved scopes
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_data_vectors();
		node.accept(*this);
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited and func_def->is_used) {
				func_def->accept(*this);
			}
		}
		node.reset_function_visited_flag();
		m_def_provider->refresh_scopes();
		return &node;
	}

	/// Resolves `node` to the declaration it names in a different case, or ends the
	/// compilation with the declaration error. `add_msg` carries whatever the call site knows
	/// beyond "not declared". See DefinitionProvider::report_declaration_error.
	NodeAST* fail_or_recover(NodeReference& node, const std::string& add_msg = "");

	/// The hint for a name that did not resolve inside a struct body, where forgetting
	/// <self> is what it nearly always is. `kind` is the word for what was referenced.
	static std::string self_hint(const Token& token, const std::string& kind);

	NodeAST * visit(NodeProgram& node) override;
	/// check if on init callback currently
	NodeAST * visit(NodeCallback& node) override;
	/// Check for correct variable types and parameter number
	NodeAST * visit(NodeUIControl& node) override;
	/// Scoping
	NodeAST * visit(NodeBlock& node) override;
    /// decide if declaration is local or global
	NodeAST * visit(NodeSingleDeclaration& node) override;
	/// check for reassignments of Function Parameters that are immutable
	NodeAST * visit(NodeSingleAssignment& node) override;
	/// Check if correctly declared and save declaration
	NodeAST * visit(NodeArray& node) override;
    /// get declaration
	NodeAST * visit(NodeArrayRef& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	NodeAST * visit(NodeVariable& node) override;
    /// get declaration
	NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	NodeAST * visit(NodeNDArray& node) override;
	NodeAST * visit(NodeNDArrayRef& node) override;
	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;
	NodeAST * visit(NodeList& node) override;
	NodeAST * visit(NodeListRef& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	NodeAST * visit(NodeFunctionCall& node) override;
    NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;

	NodeAST * visit(NodeAccessChain& node) override;

	NodeAST * visit(NodeForEach& node) override;
	// needs to be here, otherwise it gets seen as local declaration inside a block
	NodeAST * visit(NodeConst& node) override;
	NodeAST * visit(NodeStruct& node) override;

private:
	// boolean to continue after not finding declaration or fail
	Pass pass = Pass::PreUIControlLowering;
	NodeStruct* m_current_struct = nullptr;
	std::stack<NodeAccessChain*> m_current_access;
    std::stack<NodeBlock*> m_current_block;
	DefinitionProvider* m_def_provider = nullptr;

	[[nodiscard]] NodeBlock* get_current_block() const {
		if (m_current_block.empty()) return nullptr;
		return m_current_block.top();
	}



	/// An initializer that reads the very variable it declares:
	/// <declare last_idx := non_rpt_random(last_idx, 0, 5)>. Nothing of that name is declared at
	/// that point, so the read can only see the declaration's own zero-initialization. Whether
	/// the read stays unresolved long enough to be noticed depends on the pipeline, which is why
	/// this is diagnosed here rather than left to the missing-declaration error:
	/// ASTReturnFunctionRewriting splits <declare x := f(...)> into a declaration and an
	/// assignment, after which the reference resolves and the compiler falls silent, while the
	/// language server runs its final variable check before any rewriting and reports the name as
	/// undeclared. Called from the PostUIControlLowering pass, the last one both pipelines reach
	/// before any rewriting.
	void check_read_in_own_declaration(NodeSingleDeclaration& node) const;

	/// node can be NodeFunctionCall or NodeReference
	/// transformation when first object is clearly a reference this_list.next.next()
	/// tries to get declaration of first object and if there is one, replaces it with method chain
	/// The qualifier may span several dotted segments: anything declared inside a namespace
	/// carries the namespace in its name, so <audio.inst.idx> hangs off the variable
	/// <audio.inst> and <audio.Envelope.MAX> off the struct <audio.Envelope>. The shortest
	/// qualifier that resolves wins, and at equal length a variable beats a struct - that
	/// keeps the single-segment case behaving exactly as before.
	std::unique_ptr<NodeAccessChain> try_access_chain_transform(const std::string& name, NodeAST* node) const {
		const auto segments = StringUtils::split(name, '.');
		if (segments.size() < 2) return nullptr;

		std::string qualifier;
		// The last segment is the member being accessed and can never be part of the qualifier.
		for (size_t count = 1; count < segments.size(); ++count) {
			if (count > 1) qualifier += '.';
			qualifier += segments[count - 1];

			if (const auto node_declaration = m_def_provider->get_declared_data_structure(qualifier)) {
				// eq.lbl_param0 -> a reference originally recognized as a variable cannot have a
				// variable or function declaration (eq)
				if (node->cast<NodeVariableRef>() && node_declaration->cast<NodeFunctionHeader>()) {
					return nullptr;
				}
				auto method_chain = node->to_method_chain();
				if (!method_chain) return nullptr;
				method_chain->merge_members(0, count - 1);
				const auto object = static_cast<NodeReference*>(method_chain->chain[0].get());
				object->declaration = node_declaration;
				method_chain->declaration = node_declaration;
				return method_chain;
			}

			// no instance of that name -> the qualifier may name a struct instead: <Foo.MAX>.
			if (auto type_qualified = try_type_qualified_transform(qualifier, count, node)) {
				return type_qualified;
			}
		}
		return nullptr;
	}

	/// <Foo.MAX>: the chain is qualified by a struct name rather than by an instance. The leading
	/// element gets no declaration - it only carries the type so the member can be looked up.
	/// `count` is how many leading segments the struct name spans.
	std::unique_ptr<NodeAccessChain> try_type_qualified_transform(
		const std::string& struct_name, const size_t count, NodeAST* node) const {
		if(!NodeReference::get_object_ptr(m_program, struct_name)) return nullptr;
		auto method_chain = node->to_method_chain();
		if(!method_chain) return nullptr;
		method_chain->merge_members(0, count - 1);
		const auto object = method_chain->member(0)->is_reference();
		if (!object) {
			auto error = ASTVisitor::make_diagnostic(ErrorType::InternalError, *method_chain);
			error.set_message("node was flagged as access chain with TypeQualifier but has no NodeReference at idx 0");
			error.exit();
		}
		object->kind = NodeReference::Kind::TypeQualifier;
		object->ty = TypeRegistry::get_object_type(struct_name);
		return method_chain;
	}
	
	/// checks if given callback id is of type ui_control
	static bool check_callback_id_data_type(NodeAST* callback_id) {
		std::string id_node_type = "<Array>";
		if(callback_id->get_node_type() == NodeType::VariableRef) {
			id_node_type = "<Variable>";
		}
		const auto node_reference = static_cast<NodeReference*>(callback_id);
		// return prematurely if no declaration yet provided
		const auto declaration = node_reference->get_declaration();
		if(!declaration) return false;
		// check if callback id reference is ui_control
		auto error = Diagnostic(ErrorType::TypeError, "", "", callback_id->tok);
		if(node_reference->data_type != DataType::UIControl) {
			error.message = id_node_type+" needs to be of type <UI Control> to be referenced in <UI Callback>.";
			error.exit();
		} else {
			// var ref is ui control -> check if it is ui_label
			if(const auto ui_control = declaration->parent->cast<NodeUIControl>()) {
				if(ui_control->name == "ui_label") {
					error.message = "<UI Label> cannot be referenced in <UI Callback>.";
					error.exit();
				}
			}
		}
		return true;
	}
};
