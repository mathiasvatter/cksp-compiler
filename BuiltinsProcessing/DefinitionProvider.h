//
// Created by Mathias Vatter on 05.04.24.
//

#pragma once

#include <ranges>
#include <utility>

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../misc/Gensym.h"
#include "../misc/HashFunctions.h"

/**
 * @class DefinitionProvider
 *
 * @brief Collects all definitions of builtin variables, arrays, functions, widgets and external variables as well as
 * declared data structures and provides them to the ASTVisitors including dedicated methods to search for the
 * definitions.
 *
 * This class is responsible for providing definitions for builtin ksp definitions as well as
 * keeping track of declared variables, arrays, data structures and controls by adding them to the respective maps/scopes
 * and returning their declaration when needed. It also provides methods to search for declared variables, arrays,
 * data structures and controls.
 */
class DefinitionProvider {
	/// holds all in this program defined variable names for safely issuing new ones that do not get captured
	Gensym m_gensym;
public:
    DefinitionProvider(
			std::unordered_map<std::string, std::shared_ptr<NodeVariable>> m_builtin_variables,
			std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_builtin_functions,
			std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_boolean_functions,
			std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> m_property_functions,
			std::unordered_map<std::string, std::shared_ptr<NodeArray>> m_builtin_arrays,
			std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> m_builtin_widgets,
			std::vector<std::shared_ptr<NodeDataStructure>> m_external_variables);
	explicit DefinitionProvider();

	bool add_external_variables_to_global_scope() {
		if (m_declared_data_structures.empty()) {
			auto compile_error = CompileError(ErrorType::InternalError, "", -1, "", "", "");
			compile_error.m_message = "Tried to add external variables to global scope, but there is no global scope.";
			compile_error.exit();
			return false;
		}
		for(const auto& var : external_variables) {
			if (const auto ui_control = var->cast<NodeUIControl>()) {
				m_declared_data_structures[0].insert({ui_control->control_var->name, ui_control->control_var});
			} else {
				m_declared_data_structures[0].insert({var->name, var});
			}
		}
		return true;
	}

	bool add_scope();
	std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual> remove_scope();
	/// removes all scopes and initializes again
	bool refresh_scopes();
    /// removes variable from current scope by their name value
	std::shared_ptr<NodeDataStructure> remove_from_current_scope(const std::string& name);
	/// copies last scope in current scope
	bool copy_last_scope();
	/// returns the definition of a data structure, if it exists. If datastructure itself is
	/// definition -> return nullptr. If datastructure is reference -> return declaration. If global_scope is true,
	/// adds declaration to global scope.
	/// only called by references -> only gets declaration does not add existing declarations to map
	std::shared_ptr<NodeDataStructure> get_declaration(NodeReference& var);
	/// adds existing declaration to declaration map for look up. Always returns nullptr.
	std::shared_ptr<NodeDataStructure> set_declaration(const std::shared_ptr<NodeDataStructure>& var, bool global_scope);

	std::string get_fresh_name(const std::string& name) {
		return m_gensym.fresh(name);
	}

	/// detects if user-variable is declared throwaway and throws error if it is
	static void handle_throwaway_var(const NodeDataStructure& var) {
		if(var.name == "_") {
			auto error = CompileError(ErrorType::VariableError, "", "", var.tok);
			error.m_message = "Throwaway variable cannot be declared.";
			error.exit();
		}
	}
	/// returns a static global dummy datastructure that can be used for declarations of throwaway vars
	static std::shared_ptr<NodeDataStructure> get_throwaway_declaration(NodeReference& var) {
		if(var.name == "_") {
			var.kind = NodeReference::Kind::Throwaway;
		}
		if(var.kind != NodeReference::Kind::Throwaway)
			return nullptr;
		static auto node_var = std::make_shared<NodeVariable>(
			std::nullopt,
			"_",
			TypeRegistry::Unknown,
			Token(),
			DataType::Mutable
		);
		return node_var;
	}
	/// returns a static global dummy datastructure that can be used for declarations of compiler vars
	static std::shared_ptr<NodeDataStructure> get_compiler_declaration(const NodeReference& var) {
		if(var.kind != NodeReference::Kind::Compiler)
			return nullptr;
		static auto node_var = std::make_shared<NodeVariable>(
			std::nullopt,
			"compiler$dummy",
			TypeRegistry::Unknown,
			Token(),
			DataType::Mutable
		);
		return node_var;
	}
	/// returns a static global dummy datastructure that can be used for declarations of pgs vars
	static std::shared_ptr<NodeDataStructure> get_pgs_declaration(const NodeReference& var) {
		if(var.ty != TypeRegistry::PGS) return nullptr;
		static auto node_var = std::make_shared<NodeVariable>(
			std::nullopt,
			"pgs$dummy",
			TypeRegistry::PGS,
			Token(),
			DataType::Mutable
		);
		return node_var;
	}

	/// All references to variables, arrays, data structures and controls can be saved here
	std::vector<NodeReference*> m_all_references;
	void add_to_references(NodeReference* reference) {
		m_all_references.push_back(reference);
	}
	[[nodiscard]] const std::vector<NodeReference *> &get_all_references() const {
		return m_all_references;
	}
	std::vector<std::weak_ptr<NodeDataStructure>> m_all_data_structures;
	void add_to_data_structures(const std::weak_ptr<NodeDataStructure> &data_struct) {
		m_all_data_structures.push_back(data_struct);
	}
	[[nodiscard]] const std::vector<std::weak_ptr<NodeDataStructure>> &get_all_data_structures() const {
		return m_all_data_structures;
	}
	/// All declaration statements
    std::vector<NodeSingleDeclaration*> m_all_declarations;
	void add_to_declarations(NodeSingleDeclaration* decl) {
		m_all_declarations.push_back(decl);
	}
    [[nodiscard]] const std::vector<NodeSingleDeclaration *> &get_all_declarations() const {
        return m_all_declarations;
    }
	/// All assignments statements
	std::vector<NodeSingleAssignment*> m_all_assignments;
	void add_to_assignments(NodeSingleAssignment* ass) {
		m_all_assignments.push_back(ass);
	}
	[[nodiscard]] const std::vector<NodeSingleAssignment *> &get_all_assignments() const {
		return m_all_assignments;
	}

	/// clears all static pointer vectors
	bool refresh_data_vectors() {
		m_all_declarations.clear();
		m_all_references.clear();
		m_all_data_structures.clear();
		m_all_assignments.clear();
		return true;
	}

    /// dynamic vector containing every data structure; scoped
    std::vector<std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual>> m_declared_data_structures;
	/// returns data structure declaration searching all scopes
	std::shared_ptr<NodeDataStructure> get_declared_data_structure(const std::string& data);
	/// only returns data structure declaration in current scope or global_scope
	[[nodiscard]] std::shared_ptr<NodeDataStructure> get_scoped_data_structure(const std::string& data, bool global_scope) const;

	/// variable error handling
	static CompileError throw_declaration_error(NodeReference &node, const std::string& add_msg="", const DefinitionProvider* ctx = nullptr) {
		auto compile_error = CompileError(ErrorType::VariableError, "", "", node.tok);
		std::string type = "<Variable>";
		if(node.cast<NodeArrayRef>()) type = "<Array>";
		if(node.cast<NodeNDArrayRef>()) type = "<NDArray>";
		if(node.cast<NodePointerRef>()) type = "<Pointer>";
		if(node.cast<NodeListRef>()) type = "<List>";
		if (node.cast<NodeFunctionHeaderRef>()) type = "<Function> Variable of this name";
		compile_error.m_message = type+" has not been declared: " + node.tok.val+". "+add_msg;
		compile_error.m_expected = "Valid declaration";

		// check if reference is assignment in declaration of global variable
		// like : declare global ctrl := control <- where control is a lokal variable
		if (auto single_decl = node.parent->cast<NodeSingleDeclaration>()) {
			if (single_decl->variable->is_global) {
				compile_error.m_message += "This <Reference> is assigned in a global declaration. Local variables should "
							   "not be assigned in global declarations. \nTry splitting this into a declaration and an assignment.";
			}
		}

		if (ctx) {
			auto suggestions = ctx->misspelled_suggestions(node.name);
			if (!suggestions.empty()) {
				compile_error.m_message += " Did you mean: "
					+ StringUtils::join(suggestions, ',') + "?";
			}
		}

		return compile_error;
	};

	[[nodiscard]] std::vector<std::string> misspelled_suggestions(const std::string& name, size_t max_results = 4) const {
	    // Heuristik: dynamische Distanz-Schranke relativ zur Länge
	    const size_t L = name.size();
	    const size_t max_dist = std::clamp<size_t>((L <= 4 ? 1 : (L <= 8 ? 2 : 3)), 1, 4);

	    // Wenn nur die Groß-/Kleinschreibung falsch ist → genau diesen Kandidaten vorschlagen und fertig
	    {
	        const auto lname = StringUtils::to_lower(name);
	        for (const auto& scope : std::ranges::reverse_view(m_declared_data_structures)) {
	            if (auto it = std::ranges::find_if(scope, [&](auto& kv){
	                return StringUtils::to_lower(kv.first) == lname;
	            }); it != scope.end()) {
	                return { it->first };
	            }
	        }
	    }

	    struct Cand { std::string key; int score; };
	    std::vector<Cand> cands;
	    cands.reserve(32);

	    // Scopes von innen nach außen: kleinere Boni durch Score wirken implizit
	    for (const auto& scope : std::ranges::reverse_view(m_declared_data_structures)) {
	        for (const auto& key : scope | std::views::keys) {
	            // Grobfilter: Längendifferenz > (max_dist+1) überspringen
	            if (key.size() + (max_dist + 1) < L || L + (max_dist + 1) < key.size()) continue;

	            size_t d = levenshtein_ci(name, key);
	            if (d == 0) continue; // wäre „gleich“ – dann gäbe es keinen Fehler
	            if (d <= max_dist) {
	                int score = suggestion_score(name, key, d);
	                cands.push_back({key, score});
	            }
	        }
	    }

	    // Optional: Builtins & externals auch vorschlagen (kostenfrei dranhängen)
	    auto consider_map = [&](const auto& mp) {
	        for (const auto& [key, _] : mp) {
	            if (key.size() + (max_dist + 1) < L || L + (max_dist + 1) < key.size()) continue;
	            size_t d = levenshtein_ci(name, key);
	            if (d > 0 && d <= max_dist) {
	                int score = suggestion_score(name, key, d) + 2; // leichter Malus ggü. lokalen Scopes
	                cands.push_back({key, score});
	            }
	        }
	    };
	    consider_map(builtin_variables);
	    consider_map(builtin_arrays);
	    consider_map(builtin_widgets);
	    // property_functions / builtin_functions haben Signaturen – fürs Var-Suggest meist weglassen

	    // Deduplizieren und sortieren
	    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b){
	        if (a.score != b.score) return a.score < b.score;
	        return a.key < b.key;
	    });
	    cands.erase(std::ranges::unique(cands, [](const Cand& a, const Cand& b){ return a.key == b.key; }).begin(), cands.end());

	    std::vector<std::string> out;
	    out.reserve(std::min(max_results, cands.size()));
	    for (size_t i = 0; i < cands.size() && out.size() < max_results; ++i)
	        out.push_back(cands[i].key);
	    return out;
	}


	// static CompileError throw_declaration_type_error(NodeReference* node) {
	// 	auto compile_error = CompileError(ErrorType::VariableError, "", "", node->tok);
	// 	if(!node->get_declaration()) throw_declaration_error(*node).exit();
	// 	if(node->get_declaration()->get_node_type() == NodeType::Array && node->get_node_type() == NodeType::Variable) {
	// 		compile_error.m_message = "Incorrect Reference type. Reference was declared as <Array>: " + node->tok.val+".";
	// 		compile_error.m_expected = "<Array>";
	// 		compile_error.m_got = "<Variable>";
	// 		compile_error.exit();
	// 	}
	// 	if(node->get_declaration()->get_node_type() == NodeType::Variable && node->get_node_type() == NodeType::Array) {
	// 		compile_error.m_message = "Incorrect Reference type. Reference was declared as <Variable>: " + node->tok.val+".";
	// 		compile_error.m_expected = "<Variable>";
	// 		compile_error.m_got = "<Array>";
	// 		compile_error.exit();
	// 	}
	// 	return compile_error;
	// }



    /// external variables from eg nckp file
    std::vector<std::shared_ptr<NodeDataStructure>> external_variables{};
    void set_external_variables(std::vector<std::shared_ptr<NodeDataStructure>> external_variables);
    /// builtin engine variables
    std::unordered_map<std::string, std::shared_ptr<NodeVariable>> builtin_variables{};
	std::shared_ptr<NodeVariable> get_builtin_variable(const std::string& var);
    void set_builtin_variables(std::unordered_map<std::string, std::shared_ptr<NodeVariable>> builtin_variables);
    /// builtin engine arrays
    std::unordered_map<std::string, std::shared_ptr<NodeArray>> builtin_arrays{};
	std::shared_ptr<NodeArray> get_builtin_array(const std::string& arr);
    void set_builtin_arrays(std::unordered_map<std::string, std::shared_ptr<NodeArray>> builtin_arrays);
    /// builtin engine widgets
    std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> builtin_widgets{};
	std::shared_ptr<NodeUIControl> get_builtin_widget(const std::string &ui_control);
    void set_builtin_widgets(std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> builtin_widgets);
    /// builtin engine functions
    std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> builtin_functions{};
    std::shared_ptr<NodeFunctionDefinition> get_builtin_function(const NodeFunctionHeaderRef* function);
    void set_builtin_functions(std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> builtin_functions);
	/// boolean functions like and, or, not
	std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> boolean_functions{};
	std::shared_ptr<NodeFunctionDefinition> get_boolean_function(const std::string &name, int arg_count);
	void set_boolean_functions(std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> boolean_functions);
    /// predefined property functions like set_label_properties etc
    std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> property_functions{};
    std::shared_ptr<NodeFunctionDefinition> get_property_function(const NodeFunctionHeaderRef* function);
    void set_property_functions(std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> property_functions);

	template<typename... Args>
	static std::unique_ptr<NodeFunctionCall> create_builtin_call(const std::string &name, Args&&... args) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				name,
				std::make_unique<NodeParamList>(
					Token(),
					std::forward<Args>(std::move(args))... // Pack expansion hier
				),
				Token()
			),
			Token()
		);
		func_call->ty = TypeRegistry::Integer;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return func_call;
	}

	static std::unique_ptr<NodeFunctionCall> num_elements(std::unique_ptr<NodeReference> ref) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"num_elements",
				std::make_unique<NodeParamList>(
					ref->tok,
					std::move(ref)
					),
				Token()
				),
			Token()
		);
		func_call->ty = TypeRegistry::Integer;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	static std::unique_ptr<NodeFunctionCall> get_ui_id(std::unique_ptr<NodeReference> ref) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"get_ui_id",
				std::make_unique<NodeParamList>(ref->tok, std::move(ref)),
				Token()
				),
			Token()
		);
		func_call->ty = TypeRegistry::Integer;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	static std::unique_ptr<NodeFunctionCall> inc(std::unique_ptr<NodeReference> ref) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"inc",
				std::make_unique<NodeParamList>(ref->tok, std::move(ref)),
				Token()
			),
			Token()
		);
		func_call->ty = TypeRegistry::Void;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	static std::unique_ptr<NodeFunctionCall> dec(std::unique_ptr<NodeReference> ref) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"dec",
				std::make_unique<NodeParamList>(ref->tok, std::move(ref)),
				Token()
			),
			Token()
		);
		func_call->ty = TypeRegistry::Void;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	/// continue
	static std::unique_ptr<NodeFunctionCall> continu(Token &tok) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"continue",
				std::make_unique<NodeParamList>(
					tok
				),
				tok
			),
			tok
		);
		func_call->function->has_forced_parenth = false;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		func_call->ty = TypeRegistry::Void;
		return func_call;
	}

	/// exit
	static std::unique_ptr<NodeFunctionCall> exit(Token &tok) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"exit",
				std::make_unique<NodeParamList>(
					tok
				),
				tok
			),
			tok
		);
		func_call->function->has_forced_parenth = false;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		func_call->ty = TypeRegistry::Void;
		return func_call;
	}

	/// sh_right(a, b)
	static std::unique_ptr<NodeFunctionCall> sh_right(std::unique_ptr<NodeAST> a, std::unique_ptr<NodeAST> b) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"sh_right",
				std::make_unique<NodeParamList>(a->tok, std::move(a), std::move(b)),
				Token()
			),
			Token()
		);
		func_call->ty = TypeRegistry::Integer;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	/// sh_left(a, b)
	static std::unique_ptr<NodeFunctionCall> sh_left(std::unique_ptr<NodeAST> a, std::unique_ptr<NodeAST> b) {
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"sh_left",
				std::make_unique<NodeParamList>(a->tok, std::move(a), std::move(b)),
				Token()
			),
			Token()
		);
		func_call->ty = TypeRegistry::Integer;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		return std::move(func_call);
	}

	// Case-insensitive Distance
	static size_t levenshtein_ci(std::string a, std::string b) {
		a = StringUtils::to_lower(std::move(a));
		b = StringUtils::to_lower(std::move(b));
		return StringUtils::get_levenshtein_distance(a, b);
	}

	// Einfaches Score-Ranking: Levenshtein + kleine Boni/Mali
	static int suggestion_score(std::string_view needle, std::string_view cand, size_t dist_ci) {
		int score = static_cast<int>(dist_ci) * 10; // Basis: Distanz zählt am meisten

		// Bonus: gleicher erster Buchstabe (case-insensitive)
		if (!needle.empty() && !cand.empty()
			&& std::tolower((unsigned char)needle.front()) == std::tolower((unsigned char)cand.front()))
			score -= 2;

		// Bonus: gemeinsames Präfix >=2 (case-insensitive)
		size_t pref = 0;
		const size_t n = std::min(needle.size(), cand.size());
		for (; pref < n; ++pref) {
			if (std::tolower((unsigned char)needle[pref]) != std::tolower((unsigned char)cand[pref])) break;
		}
		if (pref >= 2) score -= 2;

		// Leichter Malus bei stark unterschiedlicher Länge
		const auto len_diff = (int)std::abs((int)needle.size() - (int)cand.size());
		score += std::max(0, len_diff - 1);

		return score;
	}


};


