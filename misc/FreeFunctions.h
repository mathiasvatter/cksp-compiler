//
// Created by Mathias Vatter on 17.11.24.
//

#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <sstream>

#include "../Tokenizer/Tokens.h"

#include <thread>

template <typename Iterator, typename Func>
void parallel_for_each(Iterator begin, Iterator end, Func func, size_t num_threads = 0) {
	// Bestimme die Anzahl der Threads basierend auf der Hardware
	if (num_threads == 0) {
		num_threads = std::thread::hardware_concurrency();
		if (num_threads == 0) num_threads = 2; // Fallback
	}

	// Gesamtlänge berechnen
	const size_t total_elements = std::distance(begin, end);
	if (total_elements == 0) return;

	const size_t chunk_size = std::ceil(static_cast<double>(total_elements) / num_threads);

	// Threads erstellen
	std::vector<std::thread> threads;
	Iterator chunk_begin = begin;

	for (size_t t = 0; t < num_threads && chunk_begin != end; ++t) {
		Iterator chunk_end = chunk_begin;
		std::advance(chunk_end, std::min(chunk_size, static_cast<size_t>(std::distance(chunk_begin, end))));

		threads.emplace_back([chunk_begin, chunk_end, func]() {
		  for (auto it = chunk_begin; it != chunk_end; ++it) {
			  func(*it);
		  }
		});

		chunk_begin = chunk_end;
	}

	// Alle Threads beenden
	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}


/*
 * helper function to search vectors for chars, std::String and Keyword obj
 */
static bool contains(const std::vector<char>& vec, char c) {
	return std::any_of(vec.begin(), vec.end(), [c](char ch) { return ch == c; });
}

static bool contains(const std::unordered_set<char>& set, char c) {
	return set.find(c) != set.end();
}

static bool contains(const std::vector<std::string>& vec, const std::string& value) {
	return std::find(vec.begin(), vec.end(), value) != vec.end();
}

static bool contains(const std::unordered_set<std::string>& vec, const std::string& value) {
	return vec.contains(value);
}

static bool contains(const std::vector<Keyword>& vec, const std::string& value) {
	return std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
	  return kw.value == value;
	}) != vec.end();
}

static bool contains(const std::string& string, const std::string& substring) {
	return string.find(substring) != std::string::npos;
}
static bool contains(const std::unordered_set<token>& token_set, const token& tok) {
	return token_set.contains(tok);
}


inline std::string remove_substring(const std::string& str, const std::string& substring) {
	// Suche nach dem Substring im Hauptstring
	// Wenn der Substring gefunden wurde, entferne ihn
	if (const size_t pos = str.find(substring); pos != std::string::npos) {
		auto new_str = str;
		new_str.erase(pos, substring.length());
		return new_str;
	}
	return str;
}


/// takes string and returns vector of namespaces ('.')
inline std::vector<std::string> get_namespaces(const std::string& str) {
	std::vector<std::string> namespaces;
	std::istringstream iss(str);
	std::string ns;

	while (std::getline(iss, ns, '.')) {
		namespaces.push_back(ns);
	}
	return namespaces;
}


static std::string to_lower(const std::string& input) {
	std::string output = input;
	std::transform(output.begin(), output.end(), output.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	return output;
}

static std::string to_upper(const std::string& input) {
	std::string output = input;
	std::transform(output.begin(), output.end(), output.begin(),
				   [](unsigned char c) { return std::toupper(c); });
	return output;
}
