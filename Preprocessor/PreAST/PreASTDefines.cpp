//
// Created by Mathias Vatter on 23.01.24.
//

#include "PreASTDefines.h"
#include "../../Interpreter/SimpleExprInterpreter.h"
#include <sstream>

void PreASTDefines::visit(PreNodeProgram& node) {
	m_main_ptr = &node;
	for(const auto & def : node.define_statements) {
		m_define_lookup.insert({def->header->name->keyword.val, def.get()});
	}

	m_builtin_defines = get_builtin_defines();
	for(auto & def : node.define_statements) {
		def->accept(*this);
	}
//	for(auto & def : node.macro_definitions) {
//		def->accept(*this);
//	}
	for(auto & n : node.program) {
		n->accept(*this);
	}
}

void PreASTDefines::do_substitution(PreNodeAST& node, const Token& tok) {
	if (!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(tok.val)) {
//			substitute->update_token_data(tok);
			node.replace_with(std::move(substitute));
			return;
		}
	}
}

void PreASTDefines::visit(PreNodeNumber& node) {
	// substitution
	do_substitution(node, node.number);
}

void PreASTDefines::visit(PreNodeInt& node) {
	// substitution
	do_substitution(node, node.number);
}

void PreASTDefines::visit(PreNodeKeyword& node) {
	if(auto builtin_define = get_builtin_define(node.keyword.val)) {
		builtin_define->update_token_data(node.keyword);
		node.replace_with(std::move(builtin_define));
		return;
	}
	// substitution
	do_substitution(node, node.keyword);
}

std::unique_ptr<PreNodeAST> PreASTDefines::get_builtin_define(const std::string& keyword) {
	auto it = m_builtin_defines.find(keyword);
	if(it != m_builtin_defines.end()) {
		return it->second->clone();
	}
	return nullptr;
}


void PreASTDefines::visit(PreNodeOther& node) {}

void PreASTDefines::visit(PreNodeChunk& node) {
	for(auto &c : node.chunk) {
		c->accept(*this);
	}
	node.flatten();
}

void PreASTDefines::visit(PreNodeDefineHeader& node) {
	if(node.args) node.args->accept(*this);
}

void PreASTDefines::visit(PreNodeList& node) {
	for(auto &p : node.params){
		if(p) p->accept(*this);
	}
}

void PreASTDefines::visit(PreNodeDefineStatement& node) {
	node.header->accept(*this);
	node.body->accept(*this);

	auto node_body = clone_as<PreNodeChunk>(node.body.get());
	SimpleExprInterpreter eval("", 0);
	auto eval_result = eval.parse_and_evaluate(std::move(node_body->chunk));
	if(!eval_result.is_error()) {
		Token tok = Token(token::INT, std::to_string(eval_result.unwrap()), 0, 0,"");
		auto int_token = std::make_unique<PreNodeInt>(eval_result.unwrap(), tok, nullptr);
		auto node_statement = std::make_unique<PreNodeStatement>(std::move(int_token), nullptr);
		node_statement->update_parents(&node);
		node.body->chunk.clear();
		node.body->chunk.push_back(std::move(node_statement));
	}
}

bool PreASTDefines::check_recursion(const Token &tok) {
	if(m_define_call_stack.find(tok.val) != m_define_call_stack.end()) {
		// recursive function call detected
		auto error = CompileError(ErrorType::PreprocessorError, "", "", tok);
		error.m_message = "Recursive define call detected. Calling defines inside their definition is not allowed.";
		error.m_got = tok.val;
		error.exit();
		return true;
	}
	return false;
}

void PreASTDefines::visit(PreNodeDefineCall& node) {
	Token token_name = node.define->name->keyword;
	check_recursion(token_name);

	node.define->accept(*this);
	//substitution
	auto node_new_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node.parent);
	if( auto node_define_definition = get_define_definition(node.define.get())) {
		m_define_call_stack.insert(token_name.val);
		node_define_definition->parent = node.parent;
		auto substitution_vec = get_substitution_vector(node_define_definition->header.get(), node.define.get());
		m_substitution_stack.push(std::move(substitution_vec));
		node_define_definition->body->accept(*this);
		node_new_chunk = std::move(node_define_definition->body);
		node_new_chunk->parent = node.parent;
		m_substitution_stack.pop();
		m_define_call_stack.erase(token_name.val);
	}
	node.replace_with(std::move(node_new_chunk));
}

std::unique_ptr<PreNodeDefineStatement> PreASTDefines::get_define_definition(PreNodeDefineHeader *define_header) {
	auto it = m_define_lookup.find(define_header->name->keyword.val);
	if(it != m_define_lookup.end()) {
		auto copy = clone_as<PreNodeDefineStatement>(it->second);
		copy->update_parents(nullptr);
		return copy;
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

std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> PreASTDefines::get_substitution_vector(PreNodeDefineHeader* definition, PreNodeDefineHeader* call) {
	std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> substitution_vector;
	for(int i= 0; i<definition->args->params.size(); i++) {
		auto &var = definition->args->params[i]->chunk[0];
		if(definition->args->params[i]->chunk.size() > 1) {
			CompileError(ErrorType::SyntaxError,
			 "Unable to substitute <define> arguments. Found wrong number of substitution tokens in <define-header>", definition->name->keyword.line, "", definition->get_string(),definition->name->keyword.file).exit();
		}
		std::pair<std::string, std::unique_ptr<PreNodeChunk>> pair;
		pair.first = var->get_string();
		pair.second = std::move(call->args->params[i]);
		substitution_vector.insert(std::move(pair));
	}
	return substitution_vector;
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