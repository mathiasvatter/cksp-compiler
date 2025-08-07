//
// Created by Mathias Vatter on 23.01.24.
//

#include "PreASTDefines.h"
#include "../../Interpreter/SimpleExprInterpreter.h"
#include <sstream>

#include "PreASTPragma.h"

PreNodeAST *PreASTDefines::visit(PreNodeProgram &node) {
	m_program = &node;
	for(const auto & def : node.define_statements) {
		m_define_lookup.insert({def->header->get_name(), def.get()});
	}

	m_builtin_defines = get_builtin_defines();
	visit_all(node.define_statements, *this);
	visit_all(node.program, *this);
	return &node;
}

// void PreASTDefines::visit(PreNodePragma &node) {
// 	static PreASTPragma pragma(m_program);
// 	node.accept(pragma);
// }

PreNodeAST *PreASTDefines::do_substitution(PreNodeLiteral &node) {
	if(m_program->define_call_stack.empty()) return &node;
	if (!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(node.tok.val)) {
//			substitute->update_token_data(tok);
			return node.replace_with(std::move(substitute));
		}
	}
	return &node;
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
	if(auto builtin_define = get_builtin_define(node.tok.val)) {
		builtin_define->update_token_data(node.tok);
		return node.replace_with(std::move(builtin_define));
	}
	// substitution
	return do_substitution(node);
}

std::unique_ptr<PreNodeAST> PreASTDefines::get_builtin_define(const std::string& keyword) {
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

PreNodeAST *PreASTDefines::visit(PreNodeDefineHeader &node) {
	if(node.args) node.args->accept(*this);
	return &node;
}

PreNodeAST *PreASTDefines::visit(PreNodeList &node) {
	visit_all(node.params, *this);
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
	auto node_new_chunk = std::make_unique<PreNodeChunk>(Token(), node.parent);
	if( auto node_define_definition = get_define_definition(*node.define)) {
		m_defines_used.insert(token_name.val);
		m_program->define_call_stack.push(node_define_definition.get());
		node_define_definition->parent = node.parent;
		auto substitution_vec = get_substitution_map(*node_define_definition->header, *node.define);
		m_substitution_stack.push(std::move(substitution_vec));
		node_define_definition->body->accept(*this);
		node_new_chunk = std::move(node_define_definition->body);
		node_new_chunk->parent = node.parent;
		m_substitution_stack.pop();
		m_program->define_call_stack.pop();
		m_defines_used.erase(token_name.val);
	}
	return node.replace_with(std::move(node_new_chunk));
}

std::unique_ptr<PreNodeDefineStatement> PreASTDefines::get_define_definition(const PreNodeDefineHeader& define_header) {
	const auto it = m_define_lookup.find(define_header.name->tok.val);
	if(it != m_define_lookup.end()) {
		return clone_as<PreNodeDefineStatement>(it->second);
	}
	return nullptr;
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

std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> PreASTDefines::get_builtin_defines() {
	auto now = std::chrono::system_clock::now();
	// Umwandlung in time_t für einfacheren Zugriff auf Kalenderdaten
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	// Konvertiere time_t zu tm-Struktur für lokale Zeit
	std::tm* local_time = std::localtime(&now_c);

	// Lokalisierung einstellen
	std::locale loc(""); // System-Standard-Locale
	std::ostringstream ss;

	// Lokalisierter Monatsname
	ss.imbue(loc);
	ss << std::put_time(local_time, "%B");
	std::string locale_month = ss.str();
	ss.str(""); ss.clear(); // String-Stream zurücksetzen

	// Lokalisierter abgekürzter Monatsname
	ss << std::put_time(local_time, "%b");
	std::string locale_month_abbr = ss.str();
	ss.str(""); ss.clear(); // String-Stream zurücksetzen

	// Lokalisiertes Datum
	ss << std::put_time(local_time, "%x");
	std::string locale_date = ss.str();
	ss.str(""); ss.clear(); // String-Stream zurücksetzen

	// Lokalisierte Uhrzeit
	ss << std::put_time(local_time, "%X");
	std::string locale_time = ss.str();

	std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> builtins;
	builtins.insert({"__SEC__", std::make_unique<PreNodeInt>(local_time->tm_sec, Token(token::INT, std::to_string(local_time->tm_sec), 0, 0, ""), nullptr)});
	builtins.insert({"__MIN__", std::make_unique<PreNodeInt>(local_time->tm_min, Token(token::INT, std::to_string(local_time->tm_min), 0, 0, ""), nullptr)});
	builtins.insert({"__HOUR__", std::make_unique<PreNodeInt>(local_time->tm_hour, Token(token::INT, std::to_string(local_time->tm_hour), 0, 0, ""), nullptr)});
	builtins.insert({"__HOUR12__", std::make_unique<PreNodeInt>(local_time->tm_hour % 12, Token(token::INT, std::to_string(local_time->tm_hour % 12), 0, 0, ""), nullptr)});
	builtins.insert({"__AMPM__", std::make_unique<PreNodeKeyword>(Token(token::STRING, (local_time->tm_hour >= 12 ? "\"PM\"" : "\"AM\""), 0, 0, ""), nullptr)});
	builtins.insert({"__DAY__", std::make_unique<PreNodeInt>(local_time->tm_mday, Token(token::INT, std::to_string(local_time->tm_mday), 0, 0, ""), nullptr)});
	builtins.insert({"__MONTH__", std::make_unique<PreNodeInt>(local_time->tm_mon + 1, Token(token::INT, std::to_string(local_time->tm_mon + 1), 0, 0, ""), nullptr)});
	builtins.insert({"__YEAR__", std::make_unique<PreNodeInt>(local_time->tm_year + 1900, Token(token::INT, std::to_string(local_time->tm_year + 1900), 0, 0, ""), nullptr)});
	builtins.insert({"__YEAR2__", std::make_unique<PreNodeInt>(local_time->tm_year % 100, Token(token::INT, std::to_string(local_time->tm_year % 100), 0, 0, ""), nullptr)});
	builtins.insert({"__LOCALE_MONTH__", std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_month+"\"", 0, 0, ""), nullptr)});
	builtins.insert({"__LOCALE_MONTH_ABBR__", std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_month_abbr+"\"", 0, 0, ""), nullptr)});
	builtins.insert({"__LOCALE_DATE__", std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_date+"\"", 0, 0, ""), nullptr)});
	builtins.insert({"__LOCALE_TIME__", std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+locale_time+"\"", 0, 0, ""), nullptr)});
	builtins.insert({"__CKSP_VER__", std::make_unique<PreNodeKeyword>(Token(token::STRING, "\""+COMPILER_VERSION+"\"", 0, 0, ""), nullptr)});
	return builtins;
}