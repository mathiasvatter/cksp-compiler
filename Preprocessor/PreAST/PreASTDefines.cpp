//
// Created by Mathias Vatter on 23.01.24.
//

#include "PreASTDefines.h"
#include "../../Interpreter/SimpleExprInterpreter.h"
#include <sstream>

#include "PreASTPragma.h"

PreNodeAST *PreASTDefines::visit(PreNodeProgram &node) {
	m_program = &node;
	m_substitution_stack = {};

	for (const auto& def : node.define_statements) {
		m_program->add_to_define_lookup(def);
	}

	m_builtin_defines = get_builtin_defines();
	visit_all(node.define_statements, *this);
	node.program->accept(*this);
	visit_all(node.macro_definitions, *this);
	return &node;
}

PreNodeAST *PreASTDefines::do_substitution(PreNodeLiteral &node) {
	if(m_program->define_call_stack.empty()) return &node;
	if (!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(node.tok.val)) {
			return node.replace_with(std::move(substitute));
		} else if(node.cast<PreNodeKeyword>()) {
			// in case there are more # substitutions in one word
			if (StringUtils::count_char(node.tok.val, '#') >= 2) {
				node.tok.val = get_text_replacement(node.tok);
			}
		}
	}
	return &node;
}

PreNodeAST *PreASTDefines::visit(PreNodeFunctionCall &node) {
	if (auto def = m_program->get_define_definition(node)) {
		auto node_define_call = node.transform_to_define_call();
		node_define_call->definition = def;
		return node.replace_with(std::move(node_define_call))->accept(*this);
	}
	return PreASTVisitor::visit(node);
}

PreNodeAST *PreASTDefines::visit(PreNodeNumber &node) {
	// substitution
	return do_substitution(node);
}

PreNodeAST *PreASTDefines::visit(PreNodeInt &node) {
	// substitution
	return do_substitution(node);
}

PreNodeAST *PreASTDefines::visit(PreNodeKeyword &node) {
	if (auto def = m_program->get_define_definition(node)) {
		auto node_define_call = node.transform_to_define_call();
		node_define_call->definition = def;
		return node.replace_with(std::move(node_define_call))->accept(*this);
	}

	if(auto builtin_define = get_builtin_define(node.get_name())) {
		builtin_define->update_token_data(node.tok);
		return node.replace_with(std::move(builtin_define));
	}
	// substitution
	return do_substitution(node);
}

std::unique_ptr<PreNodeAST> PreASTDefines::get_builtin_define(const std::string& keyword) {
	if (!StringUtils::starts_with(keyword, "__")) {
		return nullptr;
	}
	const auto it = m_builtin_defines.find(keyword);
	if(it != m_builtin_defines.end()) {
		return it->second->clone();
	}
	return nullptr;
}

PreNodeAST *PreASTDefines::visit(PreNodeOther &node) {
	return &node;
}

PreNodeAST *PreASTDefines::visit(PreNodeChunk &node) {
	visit_all(node.chunk, *this);
	node.flatten();
	return &node;
}

PreNodeAST *PreASTDefines::visit(PreNodeDefineStatement &node) {
	node.header->accept(*this);
	node.body->accept(*this);

	auto node_body = clone_as<PreNodeChunk>(node.body.get());
	SimpleExprInterpreter eval(node.body->tok);
	auto eval_result = eval.parse_and_evaluate(std::move(node_body->chunk));
	if(!eval_result.is_error()) {
		auto tok = node.body->tok;
		tok.set_type(token::INT); tok.set_val(std::to_string(eval_result.unwrap()));
		auto int_token = std::make_unique<PreNodeInt>(eval_result.unwrap(), tok, nullptr);
		auto node_statement = std::make_unique<PreNodeStatement>(std::move(int_token), Token(), nullptr);
		node.body->chunk.clear();
		node.body->add_chunk(std::move(node_statement));
	}
	return &node;
}

void PreASTDefines::check_recursion(const Token &tok) const {
	if(m_defines_used.contains(tok.val)) {
		// recursive function call detected
		auto error = CompileError(ErrorType::PreprocessorError, "", "", tok);
		error.m_message = "Recursive <define> call detected. Calling <defines> inside their definition is not allowed.";
		error.m_got = tok.val;
		error.exit();
	}
}

PreNodeAST *PreASTDefines::visit(PreNodeDefineCall &node) {
	const Token token_name = node.define->name->tok;
	check_recursion(token_name);

	node.define->accept(*this);
	//substitution
	// auto node_new_chunk = std::make_unique<PreNodeChunk>(Token(), node.parent);
	if (!node.definition) {
		auto error = CompileError(ErrorType::InternalError, "", "", token_name);
		error.m_message = "Undefined <define> called. This construct was marked as a <define-call> during parsing, but no definition was found during preprocessing. This is likely a bug in the preprocessor.";
		error.exit();
	}

	m_defines_used.insert(token_name.val);
	m_program->define_call_stack.push(node.definition);

	auto define_header = clone_as<PreNodeDefineHeader>(node.definition->header.get());
	auto define_body = clone_as<PreNodeChunk>(node.definition->body.get());
	// node_define_definition->parent = node.parent;
	auto substitution_map = get_substitution_map(*define_header, *node.define);
	m_substitution_stack.push(std::move(substitution_map));
	define_body->accept(*this);
	define_body->parent = node.parent;
	m_substitution_stack.pop();
	m_program->define_call_stack.pop();
	m_defines_used.erase(token_name.val);
	return node.replace_with(std::move(define_body));
}

std::unique_ptr<PreNodeAST> PreASTDefines::get_substitute(const std::string& name) {
	for (const auto &pair: m_substitution_stack.top()) {
		if (pair.first == name) {
			return pair.second->clone();
		}
	}
	return nullptr;
}

std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> PreASTDefines::get_substitution_map(PreNodeDefineHeader& definition, const PreNodeDefineHeader& call) {
	std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> map;
	for(int i= 0; i<definition.num_args(); i++) {
		const auto &var = definition.get_arg(i)->get_chunk(0);
		if(definition.get_arg(i)->num_chunks() > 1) {
			auto error = CompileError(ErrorType::SyntaxError,"", "", definition.name->tok);
			error.m_message = "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-header>.";
			error.m_got = definition.get_string();
			error.exit();
		}
		map[var->get_string()] = std::move(call.args->params[i]);
	}
	return map;
}

std::unordered_map<std::string, std::unique_ptr<PreNodeAST>>
PreASTDefines::get_builtin_defines() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    std::time_t now_c = clock::to_time_t(now);

    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &now_c);
#else
    localtime_r(&now_c, &local_tm);
#endif

    std::locale loc = std::locale::classic();
	auto fmt = [&](const char* pattern) {
        std::ostringstream ss;
        ss.imbue(loc);
        ss << std::put_time(&local_tm, pattern);
        return ss.str();
    };

    // Lokalisierte Strings (mit möglichem Fallback auf "C")
    std::string locale_month       = fmt("%B"); // Monat (lokalisiert oder englisch bei "C")
    std::string locale_month_abbr  = fmt("%b");
    std::string locale_date        = fmt("%x");
    std::string locale_time        = fmt("%X");

    // ... Rest wie gehabt:
    std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> builtins;
    builtins.insert({"__SEC__",   std::make_unique<PreNodeInt>(local_tm.tm_sec,   Token(token::INT, std::to_string(local_tm.tm_sec),   0,0,""), nullptr)});
    builtins.insert({"__MIN__",   std::make_unique<PreNodeInt>(local_tm.tm_min,   Token(token::INT, std::to_string(local_tm.tm_min),   0,0,""), nullptr)});
    builtins.insert({"__HOUR__",  std::make_unique<PreNodeInt>(local_tm.tm_hour,  Token(token::INT, std::to_string(local_tm.tm_hour),  0,0,""), nullptr)});
    builtins.insert({"__HOUR12__",std::make_unique<PreNodeInt>(local_tm.tm_hour % 12, Token(token::INT, std::to_string(local_tm.tm_hour % 12), 0,0,""), nullptr)});
    builtins.insert({"__AMPM__",  std::make_unique<PreNodeKeyword>(Token(token::STRING, (local_tm.tm_hour >= 12 ? "\"PM\"" : "\"AM\""), 0,0,""), nullptr)});
    builtins.insert({"__DAY__",   std::make_unique<PreNodeInt>(local_tm.tm_mday,  Token(token::INT, std::to_string(local_tm.tm_mday),  0,0,""), nullptr)});
    builtins.insert({"__MONTH__", std::make_unique<PreNodeInt>(local_tm.tm_mon+1, Token(token::INT, std::to_string(local_tm.tm_mon+1), 0,0,""), nullptr)});
    builtins.insert({"__YEAR__",  std::make_unique<PreNodeInt>(local_tm.tm_year+1900, Token(token::INT, std::to_string(local_tm.tm_year+1900), 0,0,""), nullptr)});
    builtins.insert({"__YEAR2__", std::make_unique<PreNodeInt>(local_tm.tm_year % 100, Token(token::INT, std::to_string(local_tm.tm_year % 100), 0,0,""), nullptr)});

    builtins.insert({"__LOCALE_MONTH__",       std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_month+"\"", 0,0,""), nullptr)});
    builtins.insert({"__LOCALE_MONTH_ABBR__",  std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_month_abbr+"\"", 0,0,""), nullptr)});
    builtins.insert({"__LOCALE_DATE__",        std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_date+"\"", 0,0,""), nullptr)});
    builtins.insert({"__LOCALE_TIME__",        std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_time+"\"", 0,0,""), nullptr)});
    builtins.insert({"__CKSP_VER__",           std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+COMPILER_VERSION+"\"", 0,0,""), nullptr)});
    return builtins;
}
